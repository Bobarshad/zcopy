# RD01 first milestone — actual offline execution evidence

Recorded: 2026-09-12. Implementation: routing-gvc 0.1. Evidence class: **SYNTHETIC_MODEL_ONLY**. The runner was the shared Linux workstation, not an OpenWrt router or a TBox. This record does not close Phase 0 target acceptance tests.

## Reproduce the final automated run

Working directory: `/home/wdai/workspace/phase0/generic-rnd/routing-gvc`.

```sh
python3 -B -m unittest discover -s tests -q
```

Actual final output captured during this work:

```text
----------------------------------------------------------------------
Ran 86 tests in 0.038s

OK
```

Exit code: 0. Counts: 61 routing/fixture tests, 15 candidate MPLS compiler tests, 10 CLI tests. Runtime is a single workstation test invocation, not a planner SLA or router performance measurement. `-B` avoids creating Python bytecode files; no third-party dependencies, privileges or network connectivity are needed.

Coverage includes directed/asymmetric adjacency, unknown metrics, hop/delay/rate constraints on both paths, declared failures, physical/node/shared-risk diversity, exhaustive small-graph oracle comparisons, the greedy-primary trap, bounded search, fractional and large-number ordering, fixed gateway identity, label consistency/range/exhaustion, and command-line input/evidence contracts.

Review found and fixed minimum-rate truncation/overflow, silently ignored CLI failure typos, incomplete JSON experiment metadata, and acceptance of an identical primary/backup in a fabricated compiler input. The compiler still consumes the validated planner's result, not an authenticated production RPC. It checks basic physical disjointness; full requested diversity remains the planner's responsibility.

## Actual CLI demonstrations

Each invocation used `python3 -B -m gvc_poc --json --fixture NAME` plus the options below. All results explicitly reported `installed=false`, `capacity_reserved=false`, and `babel_protocol_tested=false`. These eight demonstration outputs were observed before the final input-validation hardening; the completed test suite rechecks the same routing contracts.

| Fixture | Additional options | Observed result | Search expansions | Exit |
| --- | --- | --- | ---: | ---: |
| relay-chain | `--protect` | UNPROTECTED | 5 | 2 |
| relay-chain | `--fail-node R2` | NO_PATH | 0 | 2 |
| relay-alternate | `--protect --diversity shared_risk` | PROTECTED_CANDIDATE | 9 | 0 |
| grid-5x5 | `--protect --diversity shared_risk` | PROTECTED_CANDIDATE | 18 | 0 |
| two-grid-nplus1 | `--protect` | UNPROTECTED for fixed GW-A | 7 | 2 |
| two-grid-nplus1 | `--fail-node RA` | NO_PATH to fixed GW-A | 0 | 2 |
| two-grid-nplus1 | `--fail-node RA --target GW-B` | PRIMARY_ONLY to a different exit | 18 | 0 |
| grid-5x5 | `--protect --max-expansions 1` | INDETERMINATE | 1 | 2 |

The alternate Relay fixture selected primary `R0 → R1 → R2 → R3 → R4` and backup `R0 → X1 → X2 → R4`. The 25-node grid produced two candidate paths. The two-grid failure result deliberately distinguishes restored reachability through another exit from protection of the original fixed-endpoint circuit; no NAT/public-IP or TCP-session preservation is implied.

The bounded search initially reached its limit on the remote-exit fixture. Reverse reachability and integer-safe A* lower bounds now solve this fixture without relaxing the deterministic objective. A proved endpoint physical-link cut avoids pointless protection enumeration. Other difficult graphs can still return INDETERMINATE; the tests do not establish regional scalability.

## What remains untested or unimplemented

- Actual Babel adjacency formation, convergence, routing-table behavior or integration.
- Kernel MPLS forwarding, packet captures, MTU, offload and module compatibility.
- GVC agent PREPARE/COMMIT, persistent label ownership, fencing, failure rollback and reconciliation.
- Shared-resource capacity admission, N+1 service guarantees and live failover timing.
- COSTK throughput/adaptation, radio metrics, 6 GHz operation and mobility.
- Firmware image build, router installation or recovery.

The [OpenWrt PoC plan](../../openwrt-poc/BUILD_PLAN.md) now uses GL-MT6000 Ethernet links to emulate logical 6 GHz backhaul. That future hardware experiment is distinct from this offline graph evidence. No host network, router, package, service or live data flow was changed.

The next implementation increment is the shared-capacity ledger and simulated GVC transaction lifecycle. Actual standard MPLS push/swap/pop proof follows in an isolated qualified forwarding lab; commercial-router success will not replace MaxLinear/TBox qualification.
