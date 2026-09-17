"""Read-only CLI contracts; no subprocess, network or filesystem writes."""
from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
import json
import unittest

from gvc_poc.__main__ import main


class CliTests(unittest.TestCase):
    def invoke(self, *args):
        output = StringIO()
        with redirect_stdout(output):
            status = main([*args, "--json"])
        return status, json.loads(output.getvalue())

    def test_candidate_is_explicitly_not_installed(self):
        status, result = self.invoke("--protect")
        self.assertEqual(status, 0)
        self.assertEqual(result["status"], "PROTECTED_CANDIDATE")
        self.assertEqual(result["candidate_mpls"]["installation_state"], "PLAN_ONLY")
        for field in ("installed", "capacity_reserved", "babel_protocol_tested"):
            self.assertIs(result[field], False)

    def test_disconnection_exit_status(self):
        status, result = self.invoke("--fixture", "relay-chain", "--fail-node", "R2")
        self.assertEqual(status, 2)
        self.assertEqual(result["status"], "NO_PATH")
        self.assertIsNone(result["candidate_mpls"])

    def test_unprotected_is_not_success(self):
        status, result = self.invoke("--fixture", "relay-chain", "--protect")
        self.assertEqual(status, 2)
        self.assertEqual(result["status"], "UNPROTECTED")

    def test_indeterminate_is_not_compiled(self):
        status, result = self.invoke("--fixture", "grid-5x5", "--protect", "--max-expansions", "1")
        self.assertEqual(status, 2)
        self.assertEqual(result["status"], "INDETERMINATE")
        self.assertIsNone(result["candidate_mpls"])

    def test_tiny_positive_rate_is_not_rounded_to_zero(self):
        _, result = self.invoke("--min-rate-mbps", "0.0000001")
        self.assertGreater(result["constraints"]["min_rate_bps"], 0)

    def test_invalid_or_overflowing_rate_is_argument_error(self):
        for value in ("nan", "inf", "-1", "1e308"):
            with self.subTest(value=value), redirect_stderr(StringIO()):
                with self.assertRaises(SystemExit) as caught:
                    main(["--min-rate-mbps", value])
                self.assertEqual(caught.exception.code, 2)

    def test_capacity_boundary_is_not_rounded_down(self):
        status, result = self.invoke("--fixture", "relay-chain", "--min-rate-mbps", "5000.0000001")
        self.assertEqual(status, 2)
        self.assertEqual(result["status"], "NO_PATH")

    def test_unknown_failure_ids_are_errors(self):
        for kind in ("node", "link", "site", "risk"):
            with self.subTest(kind=kind), redirect_stderr(StringIO()):
                with self.assertRaises(SystemExit) as caught:
                    main(["--fail-" + kind, "not-a-fixture-id"])
                self.assertEqual(caught.exception.code, 2)

    def test_known_site_failure_is_applied(self):
        status, result = self.invoke("--fixture", "relay-chain", "--fail-site", "site:R2")
        self.assertEqual(status, 2)
        self.assertEqual(result["status"], "NO_PATH")

    def test_result_identifies_experiment(self):
        _, result = self.invoke("--fixture", "relay-chain", "--fail-node", "R2", "--protect",
                                "--diversity", "shared_risk", "--max-expansions", "123")
        self.assertEqual(result["failures"]["node"], ["R2"])
        self.assertEqual(result["diversity"], "shared_risk")
        self.assertIs(result["require_backup"], True)
        self.assertEqual(result["max_expansions"], 123)


if __name__ == "__main__":
    unittest.main()
