import unittest
from dataclasses import replace
from gvc_poc.fixtures import get_fixture, fixture_endpoints
from gvc_poc.model import FailureSet
from gvc_poc.routing import plan_routes
from gvc_poc.gvc import compile_routes


def route(name="relay-alternate", **kwargs):
    return plan_routes(get_fixture(name), *fixture_endpoints(name), **kwargs)


class CompilerTests(unittest.TestCase):
    def test_ingress_transit_egress(self):
        plan = compile_routes("chain", route("relay-chain"))
        self.assertEqual(plan.installation_state, "PLAN_ONLY")
        self.assertEqual(plan.primary[0].action, "PUSH")
        self.assertIsNone(plan.primary[0].incoming_label)
        self.assertEqual(plan.primary[-1].action, "POP")
        self.assertIsNone(plan.primary[-1].outgoing_label)
        self.assertTrue(all(op.action == "SWAP" for op in plan.primary[1:-1]))
        self.assertFalse(plan.backup)

    def test_adjacent_labels_match(self):
        plan = compile_routes("grid", route("grid-5x5"))
        for up, down in zip(plan.primary, plan.primary[1:]):
            self.assertEqual(up.outgoing_label, down.incoming_label)
            self.assertEqual(up.next_hop, down.node)

    def test_selected_adjacencies(self):
        result = route("grid-5x5")
        plan = compile_routes("grid", result)
        self.assertEqual([op.output_link for op in plan.primary[:-1]], [e.id for e in result.primary.links])

    def test_primary_backup_local_labels_do_not_collide(self):
        result = route(require_backup=True, diversity="internal_node")
        self.assertEqual(result.status, "PROTECTED_CANDIDATE")
        plan = compile_routes("pair", result)
        local = {op.node: op.incoming_label for op in plan.primary if op.incoming_label is not None}
        for op in plan.backup:
            if op.node in local:
                self.assertNotEqual(op.incoming_label, local[op.node])
        self.assertEqual(plan.primary[-1].node, plan.backup[-1].node)

    def test_labels_are_local_not_global(self):
        plan = compile_routes("chain", route("relay-chain"))
        self.assertTrue(all(op.incoming_label == 16000 for op in plan.primary[1:]))

    def test_label_exhaustion(self):
        with self.assertRaisesRegex(ValueError, "exhausted"):
            compile_routes("small", route(require_backup=True), label_start=16, label_end=16)

    def test_invalid_label_ranges(self):
        for first, last in [(15, 100), (100, 99), (16, 1048576), (True, 100)]:
            with self.subTest(first=first, last=last), self.assertRaises(ValueError):
                compile_routes("bad", route(), label_start=first, label_end=last)

    def test_empty_id(self):
        with self.assertRaises(ValueError):
            compile_routes(" ", route())

    def test_no_path_not_compiled(self):
        result = route("relay-chain", failures=FailureSet(nodes=frozenset({"R2"})))
        self.assertEqual(result.status, "NO_PATH")
        with self.assertRaises(ValueError):
            compile_routes("broken", result)

    def test_indeterminate_not_compiled(self):
        result = route("grid-5x5", require_backup=True, max_expansions=1)
        self.assertEqual(result.status, "INDETERMINATE")
        with self.assertRaises(ValueError):
            compile_routes("unknown", result)

    def test_missing_backup(self):
        with self.assertRaises(ValueError):
            compile_routes("missing", replace(route(require_backup=True), backup=None))

    def test_fabricated_protection_cannot_reuse_primary(self):
        result = route(require_backup=True)
        with self.assertRaisesRegex(ValueError, "distinct physical"):
            compile_routes("false-pair", replace(result, backup=result.primary))

    def test_backup_cannot_change_endpoint(self):
        result = route(require_backup=True)
        with self.assertRaisesRegex(ValueError, "preserve"):
            compile_routes("wrong-exit", replace(result, backup=route("grid-5x5").primary))

    def test_unprotected_status_preserved(self):
        result = route("relay-chain", require_backup=True)
        self.assertEqual(result.status, "UNPROTECTED")
        plan = compile_routes("unprotected", result)
        self.assertEqual(plan.route_status, "UNPROTECTED")
        self.assertFalse(plan.backup)

    def test_reproducible(self):
        result = route(require_backup=True)
        self.assertEqual(compile_routes("same", result), compile_routes("same", result))


if __name__ == "__main__":
    unittest.main()
