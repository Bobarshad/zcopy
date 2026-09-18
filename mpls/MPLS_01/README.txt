Four-TBox MPLS handoff, 2026-09-15

Equipment request: four physical MPLS-capable TBoxes total (two additional),
two external traffic hosts, isolated routed data links, independent power and
separate management/recovery. Reuse units for 2-node, 3-node, 4-node chain and
diamond topologies. Forwarding roles use representative TBoxes.

Start with TEST_GUIDE.md. It lists existing scripts and exact usage. Sections
5-7 contain the proposed initial two-node manual run sheet; section 8 specifies
three/four-node scenarios. Their port-specific run sheets require the device
interface map. No device test has been executed. Automated GVC repair requires
further software implementation and is not implied by manual path switching.

Run the included OFFLINE tests from the extracted archive:
  cd routing-gvc
  python3 -B -m unittest discover -s tests -v
  python3 -B evidence/review_probes.py
Python 3.10+; standard library; no root/router connection needed.

Source remains copied unchanged from Phase 0. Some original README links point
to wider Phase 0 documentation not included in this small package.
RESULTS_TEMPLATE.csv lists M01-M16, all NOT_RUN.
OFFLINE_VALIDATION.txt records validation of the unchanged Python source.
coworker-reply-draft.txt requests the additional units; it has not been sent.
SHA256SUMS.json contains payload hashes. No firmware image is included.
