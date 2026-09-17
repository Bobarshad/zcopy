"""Read-only CLI: synthetic fixtures, candidate plans, no network operations."""
import argparse
from dataclasses import asdict
import json
import math
from .fixtures import get_fixture, fixture_endpoints
from .model import Constraints, FailureSet
from .routing import plan_routes
from .gvc import compile_routes

NAMES = ("relay-chain", "relay-alternate", "grid-5x5", "two-grid-nplus1")


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--fixture", choices=NAMES, default="relay-alternate")
    p.add_argument("--source")
    p.add_argument("--target")
    p.add_argument("--protect", action="store_true")
    p.add_argument("--diversity", choices=("link", "internal_node", "shared_risk"), default="internal_node")
    p.add_argument("--min-rate-mbps", type=float, default=0.0)
    p.add_argument("--max-delay-ms", type=float)
    p.add_argument("--max-hops", type=int)
    p.add_argument("--max-expansions", type=int, default=50000)
    for kind in ("node", "link", "site", "risk"):
        p.add_argument("--fail-" + kind, action="append", default=[])
    p.add_argument("--json", action="store_true", help="JSON on stdout only; creates no files")
    a = p.parse_args(argv)
    try:
        if not math.isfinite(a.min_rate_mbps) or a.min_rate_mbps < 0:
            raise ValueError("Minimum rate must be finite and nonnegative")
        graph = get_fixture(a.fixture)
        known = {
            "node": set(graph.nodes), "link": set(graph.links),
            "site": {n.site_id for n in graph.nodes.values() if n.site_id is not None},
            "risk": {risk for item in (*graph.nodes.values(), *graph.links.values())
                     for risk in item.risk_groups},
        }
        for kind, identifiers in known.items():
            unknown = set(getattr(a, "fail_" + kind)) - identifiers
            if unknown:
                raise ValueError("Unknown " + kind + " failure identifiers: " + ", ".join(sorted(unknown)))
        src, dst = fixture_endpoints(a.fixture)
        src, dst = a.source or src, a.target or dst
        constraints = Constraints(min_rate_bps=a.min_rate_mbps * 1000000,
                                  max_delay_ms=a.max_delay_ms, max_hops=a.max_hops)
        failures = FailureSet(nodes=frozenset(a.fail_node), links=frozenset(a.fail_link),
                              sites=frozenset(a.fail_site), risks=frozenset(a.fail_risk))
        result = plan_routes(graph, src, dst, constraints=constraints, diversity=a.diversity,
                             require_backup=a.protect, failures=failures, max_expansions=a.max_expansions)
        plan = compile_routes("demo-" + a.fixture, result) if result.status in {
            "PRIMARY_ONLY", "UNPROTECTED", "PROTECTED_CANDIDATE"} else None
    except (ValueError, KeyError) as error:
        p.error(str(error))
    def describe(route):
        return None if route is None else {
            "nodes": route.nodes, "links": [e.id for e in route.links],
            "cost": route.cost, "synthetic_delay_ms": route.delay_ms}
    output = {
        "prototype": "routing-gvc-0.1", "evidence_class": "SYNTHETIC_MODEL_ONLY",
        "fixture": a.fixture, "source": src, "target": dst, "constraints": asdict(constraints),
        "require_backup": a.protect, "diversity": a.diversity,
        "max_expansions": a.max_expansions,
        "failures": {kind: sorted(getattr(a, "fail_" + kind)) for kind in known},
        "status": result.status, "reason": result.reason, "expansions": result.expansions,
        "primary": describe(result.primary), "backup": describe(result.backup),
        "candidate_mpls": None if plan is None else asdict(plan),
        "installed": False, "capacity_reserved": False, "babel_protocol_tested": False,
    }
    if a.json:
        print(json.dumps(output, indent=2, allow_nan=False))
    else:
        print("SYNTHETIC MODEL ONLY — no routes, labels or reservations installed")
        print("Fixture:", a.fixture, "| endpoints:", src, "->", dst)
        print("Result:", result.status, "| search expansions:", result.expansions)
        print("Reason:", result.reason)
        for name in ("primary", "backup"):
            row = output[name]
            print(name.capitalize() + ":", "NONE" if row is None else " -> ".join(row["nodes"]))
        if plan is not None:
            for name in ("primary", "backup"):
                operations = getattr(plan, name)
                if operations:
                    print(name.capitalize() + " candidate MPLS actions:")
                    for op in operations:
                        print(" ", op.node, op.action, "in=" + str(op.incoming_label),
                              "out=" + str(op.outgoing_label), "via=" + str(op.output_link))
        print("Limits: no Babel daemon, kernel forwarding, radio proof, N+1 admission or active repair.")
    return 0 if result.status in {"PRIMARY_ONLY", "PROTECTED_CANDIDATE"} else 2


if __name__ == "__main__":
    raise SystemExit(main())
