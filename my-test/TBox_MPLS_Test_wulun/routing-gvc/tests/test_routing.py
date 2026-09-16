"""Routing correctness tests with an independent exhaustive small-graph oracle."""

import math
import random
import unittest

from gvc_poc.fixtures import FIXTURES, fixture_endpoints, get_fixture
from gvc_poc.model import Constraints, FailureSet, Link, Node, Topology
from gvc_poc.routing import plan_routes


def graph(edges, nodes=None):
    """Test helper: each edge is (link ID, source, target, cost)."""
    links = [Link(*edge) for edge in edges]
    if nodes is None:
        nodes = [Node(n) for n in sorted({x for edge in edges for x in edge[1:3]})]
    return Topology(nodes, links, name="synthetic-test")


def link_ids(path):
    return tuple(link.id for link in path.links)


def exhaustive_paths(topology, source, target):
    """Independently enumerate every simple path; no routing helpers reused.

    Only used on tiny, unconstrained graphs. Exponential enumeration is an
    intentional test oracle, not a proposed production implementation.
    """
    found = []

    def visit(node, node_ids, link_ids, cost):
        if node == target:
            found.append((cost, tuple(link_ids), tuple(node_ids)))
            return
        for link in topology.links.values():
            if link.usable and link.source == node and link.target not in node_ids:
                visit(link.target, node_ids + [link.target], link_ids + [link.id], cost + link.cost)

    visit(source, [source], [], 0)
    return sorted(found, key=lambda path: (path[0], path[1]))


def oracle_disjoint(topology, left, right, diversity):
    def physical(path):
        return {topology.links[link_id].physical_id or link_id for link_id in path[1]}

    if physical(left) & physical(right):
        return False
    if diversity in ("internal_node", "shared_risk") and set(left[2][1:-1]) & set(right[2][1:-1]):
        return False
    if diversity == "shared_risk":
        def risks(path):
            result = set()
            for link_id in path[1]:
                result.update(("risk", item) for item in topology.links[link_id].risk_groups)
            for node_id in path[2][1:-1]:
                node = topology.nodes[node_id]
                result.update(("risk", item) for item in node.risk_groups)
                if node.site_id is not None:
                    result.add(("site", node.site_id))
            return result

        if risks(left) & risks(right):
            return False
    return True


def exhaustive_pair(topology, source, target, diversity):
    candidates = exhaustive_paths(topology, source, target)
    for primary in candidates:
        for backup in candidates:
            if oracle_disjoint(topology, primary, backup, diversity):
                return primary, backup
    return None


class ValidationTests(unittest.TestCase):
    def test_duplicate_node_ids_rejected(self):
        with self.assertRaises(ValueError):
            Topology([Node("a"), Node("a")], [])

    def test_duplicate_link_ids_rejected(self):
        with self.assertRaises(ValueError):
            Topology([Node("a"), Node("b")], [Link("ab", "a", "b"), Link("ab", "b", "a")])

    def test_unknown_link_endpoint_rejected(self):
        with self.assertRaises(ValueError):
            Topology([Node("a")], [Link("ab", "a", "missing")])

    def test_empty_node_id_rejected(self):
        with self.assertRaises(ValueError):
            Node("")

    def test_nonfinite_link_cost_rejected(self):
        for value in (math.nan, math.inf, -math.inf):
            with self.subTest(value=value), self.assertRaises(ValueError):
                Link("ab", "a", "b", cost=value)

    def test_negative_cost_rejected(self):
        with self.assertRaises(ValueError):
            Link("ab", "a", "b", cost=-1)

    def test_invalid_capacity_rejected(self):
        for value in (-1, math.nan, math.inf):
            with self.subTest(value=value), self.assertRaises(ValueError):
                Link("ab", "a", "b", capacity_bps=value)

    def test_invalid_delay_rejected(self):
        for value in (-1, math.nan, math.inf):
            with self.subTest(value=value), self.assertRaises(ValueError):
                Link("ab", "a", "b", delay_ms=value)

    def test_invalid_constraints_rejected(self):
        for params in ({"min_rate_bps": -1}, {"min_rate_bps": math.nan}, {"max_delay_ms": -1}, {"max_delay_ms": math.inf}, {"max_hops": -1}):
            with self.subTest(params=params), self.assertRaises(ValueError):
                Constraints(**params)

    def test_missing_and_equal_query_endpoints_rejected(self):
        topology = graph([("ab", "a", "b", 1)])
        for source, target in (("missing", "b"), ("a", "missing"), ("a", "a")):
            with self.subTest(source=source, target=target), self.assertRaises(ValueError):
                plan_routes(topology, source, target)

    def test_invalid_diversity_rejected(self):
        with self.assertRaises(ValueError):
            plan_routes(graph([("ab", "a", "b", 1)]), "a", "b", diversity="imaginary")

    def test_invalid_search_budget_rejected(self):
        with self.assertRaises(ValueError):
            plan_routes(graph([("ab", "a", "b", 1)]), "a", "b", max_expansions=0)

    def test_boolean_is_not_a_numeric_budget_or_metric(self):
        with self.assertRaises(ValueError):
            Link("ab", "a", "b", cost=True)
        with self.assertRaises(ValueError):
            Constraints(max_hops=True)
        with self.assertRaises(ValueError):
            plan_routes(graph([("ab", "a", "b", 1)]), "a", "b", max_expansions=True)

    def test_duplicate_resource_identifiers_rejected(self):
        with self.assertRaises(ValueError):
            Link("ab", "a", "b", resource_ids=("one", "one"))

    def test_risk_and_failure_ids_are_collections_not_strings(self):
        with self.assertRaises(ValueError):
            Node("a", risk_groups="power")
        with self.assertRaises(ValueError):
            FailureSet(nodes="a")


class PrimaryRoutingTests(unittest.TestCase):
    def test_directed_adjacency_does_not_imply_reverse(self):
        topology = graph([("ab", "a", "b", 1)])
        self.assertEqual(plan_routes(topology, "a", "b").status, "PRIMARY_ONLY")
        self.assertEqual(plan_routes(topology, "b", "a").status, "NO_PATH")

    def test_cost_tie_break_uses_link_ids(self):
        topology = graph([("z", "s", "a", 1), ("a-end", "a", "t", 1), ("a", "s", "b", 1), ("z-end", "b", "t", 1)])
        result = plan_routes(topology, "s", "t")
        self.assertEqual(link_ids(result.primary), ("a", "z-end"))
        self.assertEqual(result.primary.cost, 2)

    def test_cheapest_path_need_not_have_fewest_hops(self):
        topology = graph([("direct", "s", "t", 9), ("sa", "s", "a", 1), ("ab", "a", "b", 1), ("bt", "b", "t", 1)])
        self.assertEqual(plan_routes(topology, "s", "t").primary.nodes, ("s", "a", "b", "t"))

    def test_hop_bound_changes_selected_path(self):
        topology = graph([("direct", "s", "t", 9), ("sa", "s", "a", 1), ("at", "a", "t", 1)])
        result = plan_routes(topology, "s", "t", constraints=Constraints(max_hops=1))
        self.assertEqual(link_ids(result.primary), ("direct",))

    def test_impossible_hop_bound(self):
        topology = graph([("sa", "s", "a", 1), ("at", "a", "t", 1)])
        self.assertEqual(plan_routes(topology, "s", "t", constraints=Constraints(max_hops=1)).status, "NO_PATH")

    def test_positive_rate_requires_known_sufficient_capacity(self):
        for capacity in (None, 99, 100):
            topology = Topology([Node("s"), Node("t")], [Link("st", "s", "t", capacity_bps=capacity)])
            result = plan_routes(topology, "s", "t", constraints=Constraints(min_rate_bps=100))
            self.assertEqual(result.status, "PRIMARY_ONLY" if capacity == 100 else "NO_PATH")

    def test_unconstrained_unknown_capacity_is_not_zero(self):
        topology = graph([("st", "s", "t", 1)])
        self.assertEqual(plan_routes(topology, "s", "t").status, "PRIMARY_ONLY")

    def test_bounded_delay_rejects_unknown(self):
        topology = graph([("st", "s", "t", 1)])
        result = plan_routes(topology, "s", "t", constraints=Constraints(max_delay_ms=10))
        self.assertEqual(result.status, "NO_PATH")

    def test_unbounded_unknown_delay_remains_unknown(self):
        topology = graph([("st", "s", "t", 1)])
        self.assertIsNone(plan_routes(topology, "s", "t").primary.delay_ms)

    def test_delay_bound_is_end_to_end_and_inclusive(self):
        topology = Topology([Node(n) for n in ("s", "a", "t")], [Link("sa", "s", "a", delay_ms=4), Link("at", "a", "t", delay_ms=6)])
        self.assertEqual(plan_routes(topology, "s", "t", constraints=Constraints(max_delay_ms=9)).status, "NO_PATH")
        result = plan_routes(topology, "s", "t", constraints=Constraints(max_delay_ms=10))
        self.assertEqual(result.primary.delay_ms, 10)

    def test_stale_or_unusable_adjacency_is_not_routable(self):
        topology = Topology([Node("s"), Node("t")], [Link("st", "s", "t", usable=False)])
        self.assertEqual(plan_routes(topology, "s", "t").status, "NO_PATH")

    def test_link_failure_is_directional_when_declared_directional(self):
        topology = graph([("ab", "a", "b", 1), ("ba", "b", "a", 1)])
        failures = FailureSet(links=frozenset({"ab"}))
        self.assertEqual(plan_routes(topology, "a", "b", failures=failures).status, "NO_PATH")
        self.assertEqual(plan_routes(topology, "b", "a", failures=failures).status, "PRIMARY_ONLY")

    def test_failed_source_or_target_has_no_path(self):
        topology = graph([("st", "s", "t", 1)])
        for node in ("s", "t"):
            with self.subTest(node=node):
                result = plan_routes(topology, "s", "t", failures=FailureSet(nodes=frozenset({node})))
                self.assertEqual(result.status, "NO_PATH")

    def test_endpoint_site_failure_is_not_exempt(self):
        topology = Topology([Node("s", site_id="source-site"), Node("t")], [Link("st", "s", "t")])
        result = plan_routes(topology, "s", "t", failures=FailureSet(sites=frozenset({"source-site"})))
        self.assertEqual(result.status, "NO_PATH")

    def test_link_and_node_risk_failures(self):
        topology = Topology([Node("s"), Node("a", risk_groups=frozenset({"power"})), Node("t")], [Link("sa", "s", "a", risk_groups=frozenset({"weather"})), Link("at", "a", "t")])
        for risk in ("power", "weather"):
            with self.subTest(risk=risk):
                self.assertEqual(plan_routes(topology, "s", "t", failures=FailureSet(risks=frozenset({risk}))).status, "NO_PATH")

    def test_budget_exhaustion_is_not_proof_of_no_path(self):
        topology = graph([("sa", "s", "a", 1), ("ab", "a", "b", 1), ("bt", "b", "t", 1)])
        self.assertEqual(plan_routes(topology, "s", "t", max_expansions=1).status, "INDETERMINATE")
        self.assertEqual(plan_routes(topology, "s", "t", max_expansions=100).status, "PRIMARY_ONLY")

    def test_zero_cost_cycle_does_not_create_repeated_nodes(self):
        topology = graph([("sa", "s", "a", 0), ("as", "a", "s", 0), ("at", "a", "t", 1), ("st", "s", "t", 2)])
        result = plan_routes(topology, "s", "t")
        self.assertEqual(result.primary.nodes, ("s", "a", "t"))
        self.assertEqual(len(set(result.primary.nodes)), len(result.primary.nodes))

    def test_zero_hop_bound_cannot_reach_distinct_endpoint(self):
        topology = graph([("st", "s", "t", 1)])
        self.assertEqual(plan_routes(topology, "s", "t", constraints=Constraints(max_hops=0)).status, "NO_PATH")


class ProtectionTests(unittest.TestCase):
    def test_chain_is_explicitly_unprotected(self):
        result = plan_routes(get_fixture("relay-chain"), "R0", "R4", require_backup=True)
        self.assertEqual(result.status, "UNPROTECTED")
        self.assertIsNotNone(result.primary)
        self.assertIsNone(result.backup)

    def test_no_primary_is_no_path_not_unprotected(self):
        topology = Topology([Node("s"), Node("t")], [])
        self.assertEqual(plan_routes(topology, "s", "t", require_backup=True).status, "NO_PATH")

    def test_same_physical_link_is_not_independent_backup(self):
        topology = Topology([Node("s"), Node("t")], [Link("one", "s", "t", physical_id="same-radio"), Link("two", "s", "t", physical_id="same-radio")])
        self.assertEqual(plan_routes(topology, "s", "t", require_backup=True).status, "UNPROTECTED")

    def test_internal_node_diversity_is_stronger_than_link_diversity(self):
        topology = Topology([Node(n) for n in ("s", "a", "t")], [Link("sa1", "s", "a"), Link("sa2", "s", "a"), Link("at1", "a", "t"), Link("at2", "a", "t")])
        self.assertEqual(plan_routes(topology, "s", "t", require_backup=True, diversity="link").status, "PROTECTED_CANDIDATE")
        self.assertEqual(plan_routes(topology, "s", "t", require_backup=True, diversity="internal_node").status, "UNPROTECTED")

    def test_shared_link_risk_invalidates_otherwise_disjoint_paths(self):
        topology = Topology([Node("s"), Node("t")], [Link("one", "s", "t", risk_groups=frozenset({"same-tower"})), Link("two", "s", "t", risk_groups=frozenset({"same-tower"}))])
        self.assertEqual(plan_routes(topology, "s", "t", require_backup=True, diversity="link").status, "PROTECTED_CANDIDATE")
        self.assertEqual(plan_routes(topology, "s", "t", require_backup=True, diversity="shared_risk").status, "UNPROTECTED")

    def test_distinct_internal_nodes_can_share_failed_site(self):
        nodes = [Node("s"), Node("a", site_id="one-site"), Node("b", site_id="one-site"), Node("t")]
        topology = graph([("sa", "s", "a", 1), ("at", "a", "t", 1), ("sb", "s", "b", 1), ("bt", "b", "t", 1)], nodes)
        self.assertEqual(plan_routes(topology, "s", "t", require_backup=True, diversity="internal_node").status, "PROTECTED_CANDIDATE")
        self.assertEqual(plan_routes(topology, "s", "t", require_backup=True, diversity="shared_risk").status, "UNPROTECTED")
        self.assertEqual(plan_routes(topology, "s", "t", failures=FailureSet(sites=frozenset({"one-site"}))).status, "NO_PATH")

    def test_shared_endpoint_risk_does_not_preclude_path_diversity(self):
        topology = Topology([Node("s", risk_groups=frozenset({"endpoint"})), Node("t", risk_groups=frozenset({"endpoint"}))], [Link("one", "s", "t"), Link("two", "s", "t")])
        self.assertEqual(plan_routes(topology, "s", "t", require_backup=True, diversity="shared_risk").status, "PROTECTED_CANDIDATE")

    def test_greedy_cheapest_primary_trap_is_escaped(self):
        topology = graph([("sa", "s", "a", 1), ("ab", "a", "b", 1), ("bt", "b", "t", 1), ("ac", "a", "c", 2), ("ct", "c", "t", 2), ("sd", "s", "d", 2), ("db", "d", "b", 2)])
        self.assertEqual(link_ids(plan_routes(topology, "s", "t").primary), ("sa", "ab", "bt"))
        result = plan_routes(topology, "s", "t", require_backup=True)
        expected = exhaustive_pair(topology, "s", "t", "link")
        self.assertEqual(result.status, "PROTECTED_CANDIDATE")
        self.assertEqual(link_ids(result.primary), expected[0][1])
        self.assertEqual(link_ids(result.backup), expected[1][1])
        self.assertEqual(result.primary.cost, 5)

    def test_shared_risk_matches_independent_oracle(self):
        topology = Topology([Node(n) for n in ("s", "a", "b", "t")], [Link("sa", "s", "a", risk_groups=frozenset({"common"})), Link("at", "a", "t"), Link("sb", "s", "b", risk_groups=frozenset({"common"})), Link("bt", "b", "t"), Link("direct", "s", "t", cost=5)])
        expected = exhaustive_pair(topology, "s", "t", "shared_risk")
        result = plan_routes(topology, "s", "t", require_backup=True, diversity="shared_risk")
        self.assertEqual((link_ids(result.primary), link_ids(result.backup)), (expected[0][1], expected[1][1]))

    def test_fractional_costs_preserve_exhaustive_oracle_order(self):
        topology = graph([("sa", "s", "a", 0.1), ("at", "a", "t", 0.2), ("sb", "s", "b", 0.15), ("bt", "b", "t", 0.15), ("direct", "s", "t", 0.3)])
        expected = exhaustive_paths(topology, "s", "t")[0]
        primary = plan_routes(topology, "s", "t").primary
        self.assertEqual((primary.cost, link_ids(primary)), expected[:2])
        for diversity in ("link", "internal_node", "shared_risk"):
            with self.subTest(diversity=diversity):
                pair = exhaustive_pair(topology, "s", "t", diversity)
                result = plan_routes(topology, "s", "t", require_backup=True, diversity=diversity)
                self.assertEqual(result.status, "PROTECTED_CANDIDATE")
                self.assertEqual((link_ids(result.primary), link_ids(result.backup)), (pair[0][1], pair[1][1]))

    def test_large_integral_costs_preserve_exhaustive_oracle_order(self):
        # Integral floats outside the safe exact-sum range require the bounded
        # uniform-cost fallback, not a potentially rounded reverse-cost bound.
        large = float(2**53 + 4)
        topology = graph([("a-first", "s", "a", large), ("a-end", "a", "t", 1), ("b-first", "s", "b", large - 2), ("b-end", "b", "t", 2), ("z-direct", "s", "t", large)])
        expected = exhaustive_paths(topology, "s", "t")[0]
        primary = plan_routes(topology, "s", "t").primary
        self.assertEqual((primary.cost, link_ids(primary)), expected[:2])
        pair = exhaustive_pair(topology, "s", "t", "link")
        result = plan_routes(topology, "s", "t", require_backup=True)
        self.assertEqual(result.status, "PROTECTED_CANDIDATE")
        self.assertEqual((link_ids(result.primary), link_ids(result.backup)), (pair[0][1], pair[1][1]))

    def test_capacity_constraint_also_applies_to_backup(self):
        topology = Topology([Node("s"), Node("t")], [Link("one", "s", "t", capacity_bps=100), Link("two", "s", "t", capacity_bps=99)])
        result = plan_routes(topology, "s", "t", constraints=Constraints(min_rate_bps=100), require_backup=True)
        self.assertEqual(result.status, "UNPROTECTED")
        self.assertEqual(link_ids(result.primary), ("one",))

    def test_delay_constraint_also_applies_to_backup(self):
        topology = Topology([Node("s"), Node("t")], [Link("one", "s", "t", delay_ms=5), Link("two", "s", "t", delay_ms=6)])
        result = plan_routes(topology, "s", "t", constraints=Constraints(max_delay_ms=5), require_backup=True)
        self.assertEqual(result.status, "UNPROTECTED")

    def test_link_risk_and_internal_node_risk_share_same_failure_domain(self):
        topology = Topology([Node("s"), Node("a", risk_groups=frozenset({"power"})), Node("t")], [Link("sa", "s", "a"), Link("at", "a", "t"), Link("st", "s", "t", risk_groups=frozenset({"power"}))])
        result = plan_routes(topology, "s", "t", require_backup=True, diversity="shared_risk")
        self.assertIsNone(exhaustive_pair(topology, "s", "t", "shared_risk"))
        self.assertEqual(result.status, "UNPROTECTED")

    def test_protection_search_budget_is_not_false_unprotected_result(self):
        topology = graph([("sa", "s", "a", 1), ("at", "a", "t", 1), ("sb", "s", "b", 1), ("bt", "b", "t", 1)])
        result = plan_routes(topology, "s", "t", require_backup=True, max_expansions=1)
        self.assertEqual(result.status, "INDETERMINATE")
        self.assertEqual(plan_routes(topology, "s", "t", require_backup=True).status, "PROTECTED_CANDIDATE")

    def test_deterministic_random_small_graphs_match_exhaustive_oracle(self):
        rng = random.Random(20260912)
        for iteration in range(16):
            nodes = [Node(str(i), site_id=f"site-{i}") for i in range(5)]
            edges = [(f"{a}>{b}", str(a), str(b), rng.randint(1, 4)) for a in range(5) for b in range(5) if a != b and rng.random() < 0.32]
            topology = graph(edges, nodes)
            paths = exhaustive_paths(topology, "0", "4")
            primary_result = plan_routes(topology, "0", "4")
            with self.subTest(iteration=iteration, mode="primary"):
                self.assertEqual(primary_result.status, "PRIMARY_ONLY" if paths else "NO_PATH")
                if paths:
                    self.assertEqual(link_ids(primary_result.primary), paths[0][1])
            for diversity in ("link", "internal_node", "shared_risk"):
                with self.subTest(iteration=iteration, diversity=diversity):
                    pair = exhaustive_pair(topology, "0", "4", diversity)
                    result = plan_routes(topology, "0", "4", require_backup=True, diversity=diversity)
                    self.assertEqual(result.status, "PROTECTED_CANDIDATE" if pair else "UNPROTECTED" if paths else "NO_PATH")
                    if pair:
                        self.assertEqual((link_ids(result.primary), link_ids(result.backup)), (pair[0][1], pair[1][1]))


class FixtureTests(unittest.TestCase):
    def test_exact_fixture_names_and_endpoints(self):
        self.assertEqual(set(FIXTURES), {"relay-chain", "relay-alternate", "grid-5x5", "two-grid-nplus1"})
        for name in FIXTURES:
            with self.subTest(name=name):
                topology = get_fixture(name)
                source, target = fixture_endpoints(name)
                self.assertEqual(topology.name, name)
                self.assertIn(source, topology.nodes)
                self.assertIn(target, topology.nodes)

    def test_unknown_fixture_rejected(self):
        for factory in (get_fixture, fixture_endpoints):
            with self.assertRaises(ValueError):
                factory("unknown")

    def test_fixture_calls_are_fresh(self):
        for name in FIXTURES:
            first, second = get_fixture(name), get_fixture(name)
            self.assertIsNot(first, second)
            self.assertIsNot(first.links, second.links)
            self.assertEqual(first.links, second.links)

    def test_bidirectional_arcs_have_shared_physical_identity(self):
        for name in FIXTURES:
            topology = get_fixture(name)
            for link in topology.links.values():
                reverse = [candidate for candidate in topology.links.values() if candidate.source == link.target and candidate.target == link.source and candidate.physical_id == link.physical_id]
                with self.subTest(name=name, link=link.id):
                    self.assertEqual(len(reverse), 1)
                    self.assertEqual(reverse[0].risk_groups, link.risk_groups)
                    self.assertEqual(reverse[0].resource_ids, link.resource_ids)
                    self.assertGreater(link.capacity_bps, 0)
                    self.assertGreater(link.delay_ms, 0)

    def test_grid_has_25_nodes_and_80_directed_links(self):
        topology = get_fixture("grid-5x5")
        self.assertEqual((len(topology.nodes), len(topology.links)), (25, 80))
        result = plan_routes(topology, *fixture_endpoints("grid-5x5"))
        self.assertEqual(result.primary.cost, 8)
        self.assertEqual(len(result.primary.links), 8)

    def test_all_fixture_primary_results_are_deterministic(self):
        for name in FIXTURES:
            topology = get_fixture(name)
            source, target = fixture_endpoints(name)
            first = plan_routes(topology, source, target)
            reordered = Topology(list(reversed(list(topology.nodes.values()))), list(reversed(list(topology.links.values()))), name=topology.name)
            second = plan_routes(reordered, source, target)
            with self.subTest(name=name):
                self.assertEqual(first.status, "PRIMARY_ONLY")
                self.assertEqual(first.primary, second.primary)

    def test_relay_site_failure_partitions_plain_chain(self):
        result = plan_routes(get_fixture("relay-chain"), "R0", "R4", failures=FailureSet(sites=frozenset({"site:R2"})))
        self.assertEqual(result.status, "NO_PATH")

    def test_independent_relay_path_survives_primary_site_failure(self):
        topology = get_fixture("relay-alternate")
        protected = plan_routes(topology, "R0", "R4", require_backup=True, diversity="shared_risk")
        self.assertEqual(protected.status, "PROTECTED_CANDIDATE")
        result = plan_routes(topology, "R0", "R4", failures=FailureSet(sites=frozenset({"site:R2"})))
        self.assertEqual(result.primary.nodes, ("R0", "X1", "X2", "R4"))

    def test_nplus1_has_two_grids_four_relays_and_distinct_gateways(self):
        topology = get_fixture("two-grid-nplus1")
        self.assertEqual(len(topology.nodes), 56)
        self.assertEqual(sum(node.role == "tbox" for node in topology.nodes.values()), 50)
        self.assertEqual(sum(node.role == "relay" for node in topology.nodes.values()), 4)
        self.assertEqual({node.id for node in topology.nodes.values() if node.role == "gateway"}, {"GW-A", "GW-B"})

    def test_nplus1_crosslink_is_one_shared_resource(self):
        topology = get_fixture("two-grid-nplus1")
        cross = [link for link in topology.links.values() if link.physical_id == "crosslink:AB"]
        self.assertEqual(len(cross), 2)
        self.assertTrue(all(link.resource_ids == ("shared-crosslink-ab",) for link in cross))
        self.assertEqual({(link.source, link.target) for link in cross}, {("RDA", "RDB"), ("RDB", "RDA")})

    def test_nplus1_fixed_gateway_has_proved_unprotected_endpoint_cut(self):
        topology = get_fixture("two-grid-nplus1")
        for diversity in ("link", "internal_node", "shared_risk"):
            with self.subTest(diversity=diversity):
                result = plan_routes(topology, "A-0-0", "GW-A", require_backup=True, diversity=diversity)
                self.assertEqual(result.status, "UNPROTECTED")
                self.assertEqual(result.primary.nodes[-1], "GW-A")
                self.assertIsNone(result.backup)

    def test_alternate_exit_is_not_fixed_endpoint_backup(self):
        topology = get_fixture("two-grid-nplus1")
        failures = FailureSet(nodes=frozenset({"RA"}))
        same_exit = plan_routes(topology, "A-0-0", "GW-A", failures=failures)
        other_exit = plan_routes(topology, "A-0-0", "GW-B", failures=failures)
        self.assertEqual(same_exit.status, "NO_PATH")
        self.assertEqual(other_exit.status, "PRIMARY_ONLY")
        self.assertEqual(other_exit.primary.nodes[-1], "GW-B")
        self.assertIn("RDA>RDB", link_ids(other_exit.primary))


if __name__ == "__main__":
    unittest.main()
