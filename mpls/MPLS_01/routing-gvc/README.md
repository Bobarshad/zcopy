# Routing and GVC prototype — first generic R&D milestone

Version 0.1. Approval/scope: [RD01](../WORK_ORDER.md). This is workstation-only Python 3.10+ code using the standard library. It runs without root, downloads, network sockets or installed packages.

Reviewed on 2026-09-13 for Rev9: **retain this implementation as the offline reference**. All 86 existing tests passed. The [walkthrough](../WALKTHROUGH_REVIEW.md) explains what is established and demonstrates the missing shared-airtime/concurrent-admission behavior; the [next increment](../NEXT_INCREMENT.md) specifies the Relay additions. This revision updates the work plan and adds a review harness without changing the v0.1 solver or original fixtures.

## What is implemented

- A validated directed node/link graph with explicit physical-link IDs, sites, risk groups and shared-resource identifiers.
- Loop-free, deterministic constrained path search with synthetic link costs, rate limits, additive delay and hop bounds.
- Primary and candidate backup selection for physical-link, internal-node and declared shared-risk diversity.
- Failures of nodes, directional links, sites and risk groups.
- Candidate single-label MPLS PUSH/SWAP/POP actions with node-local simulated labels, fixed endpoints and distinct labels where primary and backup share receiving nodes.
- Four synthetic fixtures and automated tests, including independent exhaustive checks on small graphs.

No actual Babel protocol, GVC RPC/lifecycle daemon, kernel programming, capacity admission or radio behavior is implemented yet. Labels are local to one in-memory candidate plan; production label ownership requires actual agent reservations and fencing. This code is not a firmware package.

## Quick start

The step-by-step [testing README](tests/README.md) includes prerequisites, copyable commands, expected results, negative cases, troubleshooting and the Python-versus-C recommendation.

```sh
cd /home/wdai/workspace/phase0/generic-rnd/routing-gvc
python3 -B -m unittest discover -s tests -v
python3 -B -m gvc_poc --fixture relay-alternate --protect
python3 -B -m gvc_poc --fixture grid-5x5 --protect --diversity shared_risk
python3 -B -m gvc_poc --fixture relay-chain --fail-node R2
```

The last example deliberately models a broken chain and returns exit code 2. Exit code 0 means the requested primary or candidate protected plan was found; exit code 2 covers NO_PATH, UNPROTECTED, INDETERMINATE and argument errors. Inspect the printed status to distinguish these outcomes.

Use `--help` for constraints and failures. Optional `--json` prints an actual result on stdout; it creates no JSON file. No generated fixture JSON is needed.

## Implementation language recommendation

Keep this Python implementation on the workstation as the algorithm prototype and regression reference. Plan a small C userspace GVC agent for the eventual router, reuse native Babel and standard kernel MPLS, and keep constrained home mesh leaves telemetry-only. The controller can remain Python off-router for the PoC. No C agent is implemented yet and no C-versus-Python memory reduction has been measured; the [testing README](tests/README.md#7-python-or-c) explains the division and qualification steps.

## Fixtures

| Fixture | Default endpoints | Demonstration |
| --- | --- | --- |
| relay-chain | R0 → R4 | No physically independent alternate; removing R2 partitions the path |
| relay-alternate | R0 → R4 | Explicit alternative Relay path with the same endpoints |
| grid-5x5 | G-0-0 → G-4-4 | 25 physical model nodes, with explicitly declared directed adjacencies |
| two-grid-nplus1 | A-0-0 → GW-A | Two 25-node grids and Relay/exit nodes; GW-B is a different exit, not a backup endpoint for a GW-A circuit |

All numeric capacities/delays and formed links are synthetic fixture assumptions. A shared-resource ID is metadata only in this milestone: it is not yet a reservation or a scenario ledger. Per-link capacity eligibility does not prove simultaneous service guarantees over shared radios.

Preserve these synthetic 5/10 Gbps values. Add separate terminal, unsynchronized and synchronized Relay scenarios in the next increment with explicit capacity provenance, service directions and timing semantics. An active-slot rate is not automatically a wall-clock through-rate, and independent path requests do not aggregate their demand or reserve bandwidth.

For the two-grid fixture, separately asking for GW-B can show another reachable exit. It cannot prove preserved NAT/public identity or established TCP continuity. A backup for one fixed-endpoint circuit must still end at the original endpoint.

## Search contract

Paths are ordered by cost and deterministic adjacency IDs. Protection searches consider alternative primaries rather than falsely declaring protection impossible when the first shortest path has no eligible backup. The objective is primary-preferred deterministic feasibility, not globally minimum total primary-plus-backup cost.

A global expansion budget bounds popped search states across both primary and backup work; the same limit also caps their combined queued states. Graph filtering and reverse shortest-path/reachability preprocessing are outside that count, so it is not a wall-clock budget. Exhaustion is INDETERMINATE, never proof of disconnection or impossibility. Integer-safe A* lower bounds accelerate the supplied fixtures; other numeric costs retain bounded uniform-cost ordering. This prototype can grow combinatorially on difficult graphs; exhaustive small-graph tests are correctness evidence, not a regional-scale performance guarantee.

Declared edge delay is additive synthetic delay, not a sum of measured radio RTTs. Unknown capacity cannot meet a positive requested rate. Unknown delay cannot meet a finite delay bound. Failed or unusable edges are ineligible. Common ingress/egress failure is outside same-endpoint path protection; endpoint loss means that circuit is unavailable.

| Result | Meaning |
| --- | --- |
| PRIMARY_ONLY | Primary found; no backup requested |
| PROTECTED_CANDIDATE | Requested diversity has a feasible fixed-endpoint pair in the supplied graph |
| UNPROTECTED | A primary exists but a proven endpoint physical-link cut or completed exhaustive search rules out the requested pair |
| NO_PATH | No eligible primary after complete search |
| INDETERMINATE | Search budget exhausted; no complete protection/absence conclusion |

## Limits and next work

Candidate compilation does not activate traffic, handle PREPARE/COMMIT, reserve capacity or prove trusted kernel input. Babel remains the future ordinary-routing underlay, not a protocol reimplemented by this path solver. Healthy backhaul stays bypass; COSTK cannot feed path costs. Mobility, SR-MPLS, offload and production throughput are not demonstrated.

The next increment is the [shared-capacity/airtime/schedule ledger and admission model](../NEXT_INCREMENT.md), followed by simulated GVC transactions. Explicit direction pairs, normal/degraded/failure capacities, aggregate downstream demand, resource evidence and verified overflow reclamation are required. The isolated real MPLS forwarding fixture follows its source-build/runtime gates. [OpenWrt PoC planning](../openwrt-poc/README.md) and [Phase 0 vendor gates](../../0b-vendor/QUESTION_REGISTER.md) remain separate.

To reproduce the ten September 13 review observations, including deliberate demonstrations of absent admission/scheduling, run `python3 -B evidence/review_probes.py` from this directory. These observations are not next-milestone acceptance tests or measured Relay performance.

Actual execution evidence is recorded in [FIRST_MILESTONE.md](evidence/FIRST_MILESTONE.md).
