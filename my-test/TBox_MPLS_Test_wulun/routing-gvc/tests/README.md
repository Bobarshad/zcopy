# How to test the routing and GVC prototype

Updated: 2026-09-12. Applies to the Python prototype in `generic-rnd/routing-gvc/`, not the existing COSTK iperf runners. GVC means Grid Virtual Circuit here; the common design also covers Relay TBoxes.

Start with sections 1–3. The remaining examples explain failures, constraints and implementation choices. All commands below run on the workstation. No router, Internet connection, administrator privileges, Babel installation or MPLS module is required.

## 1. Open the correct directory

```sh
cd /home/wdai/workspace/phase0/generic-rnd/routing-gvc
python3 --version
```

Use Python **3.10 or newer**. The current workstation reports Python 3.10.12. If you copy the project elsewhere, change the `cd` path to the directory containing both `gvc_poc/` and `tests/`. Stay in that directory for every command below, including commands that test an individual file. Do not run from inside `tests/`.

The prototype uses only the Python standard library: no `pip install`, virtual environment, C compiler or `sudo` is needed. `-B` prevents Python from writing bytecode cache files. Run it as a module with `-m gvc_poc`, not as `python3 gvc_poc/__main__.py`.

## 2. Run all automated tests

```sh
python3 -B -m unittest discover -s tests -v
```

Expected: each test ends in `ok`, followed by `Ran 86 tests ...` and `OK`. The elapsed time varies. The complete suite was rerun successfully on 2026-09-12. A later implementation may add tests, so the important checks are a nonzero test count, no failures/errors, and `OK`.

The suite contains 61 routing/fixture tests, 15 candidate MPLS compiler tests and 10 CLI tests. Negative scenarios are part of the suite: correctly rejecting an impossible route is a passing test, not an error. The test runner itself returns exit code 0 when all assertions pass.

For quieter output:

```sh
python3 -B -m unittest discover -s tests -q
```

To isolate a component:

```sh
python3 -B -m unittest tests.test_routing -v
python3 -B -m unittest tests.test_gvc -v
python3 -B -m unittest tests.test_cli -v
```

These commands use the standard [Python unittest command-line interface](https://docs.python.org/3/library/unittest.html#command-line-interface). Historical first-milestone results and limits are in [FIRST_MILESTONE.md](../evidence/FIRST_MILESTONE.md).

## 3. See a working primary and backup

```sh
python3 -B -m gvc_poc --fixture relay-alternate --protect --diversity shared_risk
```

Expected key lines:

```text
Result: PROTECTED_CANDIDATE
Primary: R0 -> R1 -> R2 -> R3 -> R4
Backup: R0 -> X1 -> X2 -> R4
```

The actual result line also includes the search expansion count. Exit code: **0**. The output then lists candidate `PUSH`, `SWAP` and `POP` actions at ingress, transit and egress nodes. Numeric labels have node-local meaning: the same number can legitimately appear on different nodes.

`PROTECTED_CANDIDATE` means two paths satisfy the requested diversity in this synthetic graph. It does not mean routes have been installed, capacity reserved or live failover proven. The program is a one-shot planner; it is not a background agent watching the network.

## 4. Compare normal operation and failures

Run these examples individually. Some deliberately return exit code **2**; do not put them unguarded in a shell script using `set -e`, or that script will stop at the first expected negative result.

### A. A chain without an alternative

```sh
python3 -B -m gvc_poc --fixture relay-chain
python3 -B -m gvc_poc --fixture relay-chain --protect
python3 -B -m gvc_poc --fixture relay-chain --fail-node R2
```

Expected, in order:

| Request | Status / exit | Interpretation |
| --- | --- | --- |
| Ordinary primary | PRIMARY_ONLY / 0 | R0 → R1 → R2 → R3 → R4 is usable |
| Require a backup | UNPROTECTED / 2 | A primary exists, but the chain has no independent alternate |
| Remove R2 | NO_PATH / 2 | The chain is disconnected |

### B. A failure with a surviving alternate

```sh
python3 -B -m gvc_poc --fixture relay-alternate --protect --diversity shared_risk --fail-site site:R2
```

Expected: **UNPROTECTED**, exit **2**, with primary `R0 → X1 → X2 → R4`. Traffic could use that surviving candidate path if a future installer activated it, but there are no longer two independent paths. `--protect` still requests two paths after the failure. This is not the same as `NO_PATH`.

Every invocation starts from a fresh fixture. This example recomputes a proposal after a declared failure; it does not perform a stateful switch, preserve packets in flight or measure restoration time.

### C. A 5 × 5 TBox grid

```sh
python3 -B -m gvc_poc --fixture grid-5x5 --protect --diversity shared_risk
```

Expected: **PROTECTED_CANDIDATE**, exit **0**, between `G-0-0` and `G-4-4`. The fixture has 25 TBox model nodes and 80 directed links, representing 40 bidirectional physical adjacencies. They are not home mesh-router nodes, and no real RF links were measured.

### D. Two grids and distinct Internet exits

```sh
python3 -B -m gvc_poc --fixture two-grid-nplus1 --fail-node RA
python3 -B -m gvc_poc --fixture two-grid-nplus1 --fail-node RA --target GW-B
```

Expected: first **NO_PATH / 2** to the original `GW-A`; then **PRIMARY_ONLY / 0** to the different exit `GW-B`, through `RDA → RDB`. Reaching GW-B does not preserve GW-A's NAT/public address or existing TCP sessions. It is a separate endpoint request, not a protected backup for the original circuit.

### E. An impossible rate and an exhausted search budget

```sh
python3 -B -m gvc_poc --fixture relay-chain --min-rate-mbps 6000
python3 -B -m gvc_poc --fixture grid-5x5 --protect --max-expansions 1
```

Expected: **NO_PATH / 2**, then **INDETERMINATE / 2**. The chain's synthetic link capacity is 5,000 Mbps, below the requested 6,000 Mbps. This is not a throughput test. INDETERMINATE means the planner reached a resource limit; it does not prove that a path or backup is impossible.

## 5. Options, identifiers and machine-readable output

```sh
python3 -B -m gvc_poc --help
python3 -B -m gvc_poc --fixture relay-alternate --protect --json
```

The second command prints JSON to the terminal; it creates no file. In particular, verify:

```json
{
  "evidence_class": "SYNTHETIC_MODEL_ONLY",
  "installed": false,
  "capacity_reserved": false,
  "babel_protocol_tested": false
}
```

This is an excerpt, not the complete result. The full output also identifies the fixture, endpoints, constraints, requested diversity/protection, failures, search limit, result and candidate label operations. Any candidate plan has `installation_state: PLAN_ONLY`. Save the full command/output with the software version if you need an experiment record; do not turn these flags into a deployment claim.

| Option | Meaning / unit |
| --- | --- |
| `--source`, `--target` | Existing fixture node IDs; distinct endpoints are required |
| `--protect` | Require a primary and a compatible backup |
| `--diversity link` | Distinct physical links, not just distinct directed edge names |
| `--diversity internal_node` | Link diversity plus different intermediate nodes; CLI default |
| `--diversity shared_risk` | Also avoid declared common link/internal-node risks and intermediate sites |
| `--min-rate-mbps` | Minimum per-link eligible capacity in decimal Mbps; not a reservation |
| `--max-delay-ms` | Bound on additive synthetic one-way path delay, not measured RTT |
| `--max-hops` | Maximum directed links in each path |
| `--max-expansions` | Positive integer; default 50,000, bounding popped and combined queued path states, not wall-clock time |
| `--fail-node`, `--fail-link`, `--fail-site`, `--fail-risk` | Repeatable exact identifiers from the selected fixture |

Identifiers are case-sensitive and unknown failure IDs are errors. Node `R2` is different from site `site:R2`. A link ID such as `R1>R2` contains a shell operator: quote it when used, for example `--fail-link 'R1>R2'`. That removes one directed link only; to model both directions, also specify `--fail-link 'R2>R1'`. The fixture definitions are in [fixtures.py](../gvc_poc/fixtures.py). The current CLI has no arbitrary topology-file input.

The CLI returns 0 for PRIMARY_ONLY or PROTECTED_CANDIDATE and 2 for UNPROTECTED, NO_PATH, INDETERMINATE or an argument error. Look at the printed status/reason; an argument error prints usage and an error instead of a route result.

## 6. Troubleshooting

| Symptom | What to check |
| --- | --- |
| `No module named gvc_poc` or `tests.test_*` | Return to the project directory from section 1; keep both package directories together |
| `attempted relative import with no known parent package` | Use `python3 -B -m gvc_poc`, not the path to `__main__.py` |
| `python3: command not found` or unsupported syntax | Use an installed Python 3.10+ on the workstation; this guide does not install Python on a router |
| `Ran 0 tests` | This is not a pass: check the working directory and `-s tests` |
| `Unknown ... failure identifiers` | Check the fixture and exact node/site/link/risk name |
| Exit 2 | May be expected; compare the status with section 4 rather than treating every exit 2 as a crash |
| INDETERMINATE | A bounded search stopped; inspect topology/constraints and review a larger budget deliberately, not without a memory limit |
| No interface or routing-table changes | Correct: the program only prints a candidate plan |

If a unit test genuinely fails, retain the full traceback, command, Python version and any local changes. Do not alter expected results merely to make a test pass.

## 7. Python or C?

**Recommendation: keep Python for the workstation prototype and test harness; write a small C userspace agent for the eventual router deployment.** This is an implementation recommendation, not a completed C port or a measured memory comparison.

| Component | Recommended implementation / placement |
| --- | --- |
| Current graph/path planner, simulations and regression tests | Keep Python on the workstation; use it as a readable reference for algorithm behavior |
| Initial GVC controller | Python on a management workstation/server is reasonable for the PoC; qualify scale and availability before production use |
| Router-resident GVC agent | Small C userspace daemon, bounded state and dependencies; native kernel interfaces for the qualified backend |
| Babel routing | Reuse the existing native `babeld`; do not rewrite Babel in either language |
| MPLS packet forwarding | Reuse qualified standard Linux MPLS in the kernel, not a Python/C userspace packet-forwarding loop |
| Memory-limited home mesh leaf | Keep the existing small C telemetry reporter only; no GVC agent, Python runtime, COSTK proxy or MPLS installation |

The [upstream Babel implementation](https://github.com/jech/babeld) supplies the routing daemon. OpenWrt's [pinned MPLS package definition](https://github.com/openwrt/openwrt/blob/v25.12.5/package/kernel/linux/modules/netsupport.mk) supplies standard kernel-module integration, subject to exact-target qualification. Our new work is GVC control, lifecycle and integration, not a replacement routing protocol or MPLS kernel stack.

Keeping the controller off the router avoids making every router carry the prototype's interpreter and full topology-search state. C is a suitable choice for a size-focused resident agent because we can bound its allocations and runtime dependencies, but a C rewrite alone does not guarantee small memory use, safety or scalable path search. We have not measured a C-versus-Python footprint for this GVC application and should not promise a particular reduction.

The proposed division is control versus forwarding: the controller plans circuits, the router agent validates/installs/reconciles them, and the kernel forwards packets. The controller is not in the per-packet path. Router-local preinstalled repair and controller-loss behavior must be explicitly designed and tested; this offline prototype does not provide them.

Suggested implementation sequence:

1. Keep the Python reference and finish shared-capacity and transaction tests.
2. Specify the controller/agent protocol, label ownership, generation fencing, limits and restart behavior before writing the native backend.
3. Implement the bounded C agent and its OpenWrt package; use checked parsing and appropriate authentication, not arbitrary shell commands received over the network.
4. Reuse the same fixtures/expected outcomes as differential tests for any C planning code. Add parser fuzzing, sanitizer runs and allocation-failure tests on a development host.
5. Measure actual package/dependency storage, idle/peak memory, large-topology/large-circuit behavior, leak-free restart/soak behavior and failure recovery on the chosen hardware. Establish pass/fail budgets before declaring it deployable.

If later profiling shows that planning must run locally on a TBox, move only the justified bounded components to C and keep Python as the test oracle. No C port, daemon packaging or change to the existing COSTK implementation is made by this README.

## 8. How this leads to the Ethernet backhaul demo

These tests do not send traffic through the GL-MT6000. The future lab maps separate routed Ethernet cables to logical 6 GHz adjacencies, first proving Babel and standard MPLS, then GVC installation and the four-router alternate path, and later controlled impairment/COSTK experiments. See the [hardware build and testing plan](../../openwrt-poc/BUILD_PLAN.md).

Passing this Python suite is not a Babel-convergence, actual MPLS-forwarding, iperf, RSSI, 6 GHz, mobility, N+1 capacity or TBox qualification result. Healthy backhaul remains in bypass; COSTK is separately qualified endpoint/service policy, not a required proxy on every transit node.
