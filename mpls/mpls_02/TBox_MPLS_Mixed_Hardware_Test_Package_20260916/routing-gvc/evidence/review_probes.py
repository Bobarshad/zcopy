"""Reproduce the September 13 review observations on the existing v0.1 model.

Run from routing-gvc: python3 -B evidence/review_probes.py
These are SYNTHETIC_MODEL_ONLY observations, including deliberate demonstrations
of missing admission/scheduling. A limitation observed here is not a production
acceptance condition. Revisit this harness when the next milestone is implemented.
No RF fixture, production capacity or forwarding state is changed.
"""

from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from gvc_poc.fixtures import get_fixture
from gvc_poc.model import Constraints, FailureSet, Link, Node, Topology
from gvc_poc.routing import plan_routes


def observe(name, actual, expected, meaning):
    if actual != expected:
        raise AssertionError(f"{name}: expected {expected!r}; got {actual!r}")
    print(f"{name}: {actual}; {meaning}")


def two_stage(capacity):
    return Topology(
        [Node(n) for n in ("A", "B", "C")],
        [Link("AB", "A", "B", capacity_bps=capacity,
              resource_ids=("B-shared-airtime",)),
         Link("BC", "B", "C", capacity_bps=capacity,
              resource_ids=("B-shared-airtime",))],
    )


print("SYNTHETIC_MODEL_ONLY — review observations; no admission or installation")
limited = two_stage(2_590_000_000)
observe("P01 estimated bottleneck / 5 Gbps request",
        plan_routes(limited, "A", "C", Constraints(min_rate_bps=5e9)).status,
        "NO_PATH", "per-link rate filter correctly rejects the request")
observe("P02 same assumed bottleneck / 2 Gbps request",
        plan_routes(limited, "A", "C", Constraints(min_rate_bps=2e9)).status,
        "PRIMARY_ONLY", "candidate reachability only; this is not measured service")

unknown = two_stage(None)
observe("P03 unknown rate / positive request",
        plan_routes(unknown, "A", "C", Constraints(min_rate_bps=1)).status,
        "NO_PATH", "unknown capacity does not establish a positive guarantee")
observe("P04 unknown rate / unconstrained reachability",
        plan_routes(unknown, "A", "C").status,
        "PRIMARY_ONLY", "reachability is allowed without a service-rate claim")

scheduled = two_stage(6_050_000_000)
observe("P05 active-slot rate mistakenly supplied as capacity",
        plan_routes(scheduled, "A", "C", Constraints(min_rate_bps=5e9)).status,
        "PRIMARY_ONLY", "LIMIT: shared airtime and active-slot semantics are absent")
print("P05 conditional timing bound: 1/(1/6.05 + 1/6.05) = 3.025 Gbps; "
      "only if 6.05 Gbps is active-slot throughput and RX/TX intervals are exclusive")

shared = Topology([Node("A"), Node("B")], [
    Link("AB", "A", "B", capacity_bps=5e9, resource_ids=("half-duplex",)),
    Link("BA", "B", "A", capacity_bps=5e9, resource_ids=("half-duplex",)),
])
observe("P06 opposing 3 Gbps requests on declared shared resource",
        tuple(plan_routes(shared, s, t, Constraints(min_rate_bps=3e9)).status
              for s, t in (("A", "B"), ("B", "A"))),
        ("PRIMARY_ONLY", "PRIMARY_ONLY"),
        "LIMIT: no paired-direction or shared 5 Gbps budget admission")
observe("P07 two independent 3 Gbps requests on one 5 Gbps arc",
        tuple(plan_routes(shared, "A", "B", Constraints(min_rate_bps=3e9)).status
              for _ in range(2)),
        ("PRIMARY_ONLY", "PRIMARY_ONLY"),
        "LIMIT: calls do not reserve capacity; aggregate 6 Gbps is not admitted")

same_site = Topology(
    [Node("S"), Node("A", site_id="mast"),
     Node("B", site_id="mast"), Node("T")],
    [Link("SA", "S", "A"), Link("AT", "A", "T"),
     Link("SB", "S", "B"), Link("BT", "B", "T")],
)
observe("P08 distinct nodes sharing one site",
        tuple(plan_routes(same_site, "S", "T", require_backup=True,
                          diversity=d).status
              for d in ("internal_node", "shared_risk")),
        ("PROTECTED_CANDIDATE", "UNPROTECTED"),
        "site diversity needs declared site/risk data and shared_risk selection")

alternate = get_fixture("relay-alternate")
observe("P09 independent declared alternate after primary site failure",
        plan_routes(alternate, "R0", "R4",
                    failures=FailureSet(sites=frozenset({"site:R2"}))).primary.nodes,
        ("R0", "X1", "X2", "R4"), "fixed endpoints remain unchanged")

exits = get_fixture("two-grid-nplus1")
failure = FailureSet(nodes=frozenset({"RA"}))
observe("P10 original versus different exit after RA failure",
        tuple(plan_routes(exits, "A-0-0", t, failures=failure).status
              for t in ("GW-A", "GW-B")),
        ("NO_PATH", "PRIMARY_ONLY"),
        "a reachable new exit does not preserve the original circuit or NAT state")
print("10 observations matched v0.1; RF, N+1 admission and firmware remain unqualified")
