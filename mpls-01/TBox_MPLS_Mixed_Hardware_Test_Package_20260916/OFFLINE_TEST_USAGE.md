# Existing offline routing/GVC scripts

Source retained unchanged from the prior package. These tests run on a workstation; they do not implement the new hardware topology or install routes. The top-level September 16 proposal governs device testing.

**1. What already exists**

The project currently contains an offline Python routing/GVC prototype. It computes paths and candidate MPLS instructions; it does not connect to TBoxes, configure a kernel, run Babel, reserve capacity or transmit packets. No project script automating real MPLS push/swap/pop was found in the Phase 0 or COSTK test directories. The existing COSTK throughput runners serve a different purpose.

Original directory: `/home/wdai/workspace/phase0/generic-rnd/routing-gvc/`. The accompanying ZIP contains the same source under `routing-gvc/`.

| File | Purpose | Where it runs |
| --- | --- | --- |
| `tests/test_gvc.py` | 15 tests of candidate PUSH/SWAP/POP compilation, label consistency, endpoint preservation and invalid-plan rejection | Workstation |
| `tests/test_routing.py` | 61 tests of graph/path selection, constraints, diversity and simulated failures | Workstation |
| `tests/test_cli.py` | 10 tests of command-line validation, output and statuses | Workstation |
| `gvc_poc/__main__.py` | Demonstration planner; invoke as `python3 -B -m gvc_poc` | Workstation |
| `evidence/review_probes.py` | Ten diagnostic observations, including known missing capacity/scheduling behavior | Workstation |
| `gvc_poc/gvc.py` | Compiler library used by the tests/planner; not a standalone runner | Imported by Python |
| `tests/README.md` | Existing detailed instructions and expected scenario outcomes | Documentation |

Python 3.10 or newer and its standard library are sufficient. No pip packages, root access, router connection or MPLS kernel modules are required for these offline tests. Copy the entire `routing-gvc/` folder; individual test files depend on `gvc_poc/`.

**2. Run the existing scripts**

From the original workspace:

```sh
cd /home/wdai/workspace/phase0/generic-rnd/routing-gvc
python3 --version
python3 -B -m unittest discover -s tests -v
```

From the ZIP, instead change into its extracted `routing-gvc/` directory. Expected result: `Ran 86 tests ...` followed by `OK`, exit 0. The September 15 run passed all 86 tests; current package verification is recorded in `PACKAGE_VALIDATION.txt`. A result of zero discovered tests is not acceptable.

Run individual components, still from that same directory:

```sh
python3 -B -m unittest tests.test_gvc -v
python3 -B -m unittest tests.test_routing -v
python3 -B -m unittest tests.test_cli -v
python3 -B evidence/review_probes.py
```

The diagnostic script should finish with `10 observations matched v0.1`. Some observations deliberately demonstrate absent shared-airtime and aggregate admission checks. Matching them records the current limitation; it does not establish that those features work.

Display a primary/backup candidate and its MPLS operations:

```sh
python3 -B -m gvc_poc --fixture relay-alternate --protect --diversity shared_risk
```

Expected:

```text
Result: PROTECTED_CANDIDATE
Primary: R0 -> R1 -> R2 -> R3 -> R4
Backup: R0 -> X1 -> X2 -> R4
```

The output also lists PUSH at the ingress, SWAP at intermediate nodes and POP at the egress. Repeated label numbers on different nodes are legitimate because labels are node-local. These are proposed instructions, not installed state.

Other useful examples:

| Command suffix after `python3 -B -m gvc_poc` | Expected status | Exit |
| --- | --- | --- |
| `--fixture relay-chain` | PRIMARY_ONLY | 0 |
| `--fixture relay-chain --protect` | UNPROTECTED: no diverse backup | 2 |
| `--fixture relay-chain --fail-node R2` | NO_PATH | 2 |
| `--fixture relay-alternate --protect --diversity shared_risk --fail-site site:R2` | UNPROTECTED: surviving primary R0–X1–X2–R4, no second path | 2 |
| `--fixture grid-5x5 --protect --diversity shared_risk` | PROTECTED_CANDIDATE | 0 |
| `--fixture relay-chain --min-rate-mbps 6000` | NO_PATH: exceeds the fixture's synthetic per-link capacity | 2 |
| `--fixture grid-5x5 --protect --max-expansions 1` | INDETERMINATE: search budget exhausted | 2 |

Run expected-negative commands individually. An unguarded command returning 2 stops a shell script using `set -e`. An argument error also returns 2, so read the status/reason.

Inspect options and save a proposed plan:

```sh
python3 -B -m gvc_poc --help
python3 -B -m gvc_poc --fixture relay-alternate --protect --json > candidate-plan.json
```

The JSON explicitly states `evidence_class: SYNTHETIC_MODEL_ONLY`, `installed: false`, `capacity_reserved: false`, and `babel_protocol_tested: false`. The shell redirection creates the file; the planner itself only writes stdout. Do not execute the JSON as device commands.

Supported options include `--source`, `--target`, `--protect`, `--diversity link|internal_node|shared_risk`, `--min-rate-mbps`, `--max-delay-ms`, `--max-hops`, `--max-expansions`, and repeatable `--fail-node|--fail-link|--fail-site|--fail-risk`. IDs are case-sensitive. Quote a directional link, for example `--fail-link 'R1>R2'`; specify the reverse separately to remove both directions. There is no arbitrary topology-file or TBox-address input.

