Mixed TBox MPLS validation package — 2026-09-16

Use this revision instead of the September 15 four-MPLS-unit package.
Minimum: three MPLS TBoxes A/B/D, one standard non-MPLS TBox S1 and two
traffic hosts. No fourth MPLS unit is required for the proposed initial work.

Review TOPOLOGY_PROPOSAL.md first, then TEST_GUIDE.md.
- Chain: Host L--S1 (ordinary IP)--A--B--D--Host R.
- Triangle: add direct A--D; primary A--B--D, alternate A--D.
- S1 is an ordinary-IP access router without NAT, not an MPLS transit node.
- Optional standard-box core bridging requires separately verified carriage.
- Additional standard access units are optional and need a revised address map.

DEVICE_RUN_SHEET.md contains proposed manual Linux examples for this topology,
including S1 routing, primary/alternate labels, captures, traffic and cleanup.
They are not an automated installer and have not been executed on TBoxes.
Confirm firmware/tools, actual port names, operating modes, selected scenarios
and acceptance criteria with the co-worker before binding the final run sheets.

RESULTS_TEMPLATE.csv preserves M01-M16 and adds M17-M19. M13/M15 are deferred
for a fourth MPLS unit; M16 is deferred for the agent/controller. Other cases
are NOT_RUN, including conditional Babel and bridge checks. A manual switch
does not prove automated MPLS/GVC repair. No hardware pass is claimed.

Existing OFFLINE Python scripts are unchanged. From the extracted package:
  cd routing-gvc
  python3 -B -m unittest discover -s tests -v
  python3 -B evidence/review_probes.py
Use Python 3.10+ and its standard library; no root/router connection is needed.
OFFLINE_TEST_USAGE.md gives full commands and expected results. The existing
demo fixtures are synthetic and do not configure this new hardware topology.

The copied original source READMEs retain historical four-router plans and
links to wider Phase 0 files that are not bundled here. This revision's
top-level proposal and guide govern the available-hardware test plan.

PACKAGE_VALIDATION.txt records current local verification. Historical evidence
remains in PREVIOUS_OFFLINE_VALIDATION_20260915.txt under its original date.
CHANGELOG.md explains scope and case mapping changes.
coworker-reply-draft.txt is an unsent email proposal.
SHA256SUMS.json records hashes of package payload files.
