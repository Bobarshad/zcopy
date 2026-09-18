# Mixed TBox MPLS validation package — September 16 revision

This revision supersedes the September 15 four-MPLS-TBox proposal for the current hardware allocation. **Use three MPLS-capable TBoxes A, B and D plus one standard, non-MPLS TBox S1.** More standard units are optional. Availability of a fourth MPLS unit does not gate the initial work.

This is a proposal for the co-worker's topology review and selection of environment-specific scenarios. Traffic rates, durations, fault actions and acceptance criteria below are candidate lab settings, not agreed production requirements. No device configuration has been applied and no hardware case has passed.

**1. Start here**

| File | Purpose |
| --- | --- |
| [TOPOLOGY_PROPOSAL.md](TOPOLOGY_PROPOSAL.md) | Equipment, roles, diagrams, alternatives and coverage boundaries |
| [DEVICE_RUN_SHEET.md](DEVICE_RUN_SHEET.md) | Proposed standard-Linux example for the mixed access path, three-node MPLS chain and direct alternate |
| [OFFLINE_TEST_USAGE.md](OFFLINE_TEST_USAGE.md) | Existing Python script inventory, commands, expected results and limitations |
| [RESULTS_TEMPLATE.csv](RESULTS_TEMPLATE.csv) | M01–M19 case register, including explicitly deferred cases |
| [coworker-reply-draft.txt](coworker-reply-draft.txt) | Unsent reply describing the revised proposal |
| [PACKAGE_VALIDATION.txt](PACKAGE_VALIDATION.txt) | Verification of this package; offline/source and syntax checks only |
| [CHANGELOG.md](CHANGELOG.md) | Differences from the September 15 package |

**2. Hardware and software requirements**

- Three MPLS TBoxes with recorded models, hardware revisions, firmware/SDK and kernel versions. Confirm whether their supported MPLS interface is standard Linux/iproute2 or a vendor API. The manual example applies to the former.
- One standard TBox S1 with supported IPv4 forwarding, static routes, and a configuration preserving the test endpoint addresses without NAT. S1 needs no MPLS facilities or Babel daemon for this role. If only bridge mode is supported, review an access-bridge variant with its own address map; do not apply the routed-S1 commands unchanged.
- Two independent traffic hosts with IP routing, ping and preferably iperf3, plus capture facilities. Confirm exact utilities before using options. The original Python prototype remains on a workstation.
- Triangle data connections: A and D each need three isolated adjacencies (access plus two peers), B needs two, and S1 needs two. There are six data links in the minimum triangle including the two traffic hosts. Management connections are additional. Use independently controllable TBox power for node-failure tests.
- The three MPLS boxes need matched kernel facilities and MPLS-capable iproute2, plus supported captures/counters. Facilities may be built-in or modules. Babel testing is conditional on a supported daemon/configuration on the three core nodes; it is not assumed from MPLS support.
- Preserve separate management/recovery. Record bridging, VLANs, NAT/firewall, policy routes, MTUs, negotiated speeds and offload state. Shared switches, trunks or power cannot be described as independent failure resources.

The standard access TBox carries plain IP; do not place a standard IP router in the middle of a bare-MPLS path. An optional standard unit can act as an L2 bridge only after its actual firmware, ports and datapath demonstrate transparent carriage. Ethernet bridging forwards frames at Layer 2; that is distinct from an MPLS label operation. [Linux bridge documentation](https://docs.kernel.org/networking/bridge.html), [MPLS Ethernet encoding](https://www.rfc-editor.org/rfc/rfc3032.html).

**3. Proposed execution sequence**

1. Review the topology and co-worker's intended scenarios. Fill in the physical units, port names, build identities, allowed fault actions and evidence requirements. Verify the standard unit's access role.
2. Establish the complete ordinary-IP baseline through S1–A–B–D, including return routes and endpoint identity preservation.
3. Stage MPLS egress and transit entries downstream first, then enable ingress. Capture push, swap and pop across A–B–D; configure and test both directions. Rotate the physical units through the transit role in subsequent runs.
4. Run bounded delivery, missing-label, input-gate, TTL, MTU and load checks on that chain. Record S1 and the hosts as potential throughput limits.
5. Qualify the direct A–D alternate independently. Test controlled manual replacement after a primary-link or B failure, with separate timestamps for fault, detection, operator commands and data recovery. Specify both directions; changes on A and D are not an atomic transaction.
6. If supported, separately test ordinary-IP Babel learning/withdrawal on the triangle. Keep this run distinct from the static MPLS test: a Babel route change is not proof that explicit label bindings were repaired.
7. Consider optional bridge carriage only after the standard firmware/port capabilities are known. It is not a dependency of steps 1–6. Leave deferred hardware/software cases deferred.

**4. Case mapping and evidence**

Existing case IDs remain traceable. M01–M10 retain their behavioral checks but now apply to the current three-MPLS-node path with ordinary-IP access through S1. In this revision, D is the far MPLS endpoint; B is a transit node. M03/M04's exact labels and interfaces therefore change from the earlier two-node example. M13 and M15 keep their original four-MPLS-node meanings. New triangle/access/bridge experiments have their own IDs.

| ID | Current interpretation | Planning status / prerequisite |
| --- | --- | --- |
| M01 | Record all models/builds; establish MPLS capabilities on A/B/D and plain-IP capability on S1 | NOT_RUN |
| M02 | Ordinary-IP baseline through S1–A–B–D, both directions, no NAT | NOT_RUN |
| M03 | Forward push at A, swap at B, pop at D; match Host L → Host R packets | NOT_RUN |
| M04 | Reverse push at D, swap at B, pop at A toward S1; match return packets | NOT_RUN |
| M05 | Repeated bidirectional delivery with matched sequence IDs/captures | NOT_RUN |
| M06 | Missing/unknown transit and egress label: bounded probes drop, no IP fallback, recovery after restoration | NOT_RUN |
| M07 | MPLS input gate on isolated receiving core interfaces | NOT_RUN |
| M08 | MTU and PMTUD across the complete mixed path, including S1 | NOT_RUN |
| M09 | Bounded UDP/TCP and resource observations, with all devices and host limits identified | NOT_RUN |
| M10 | Test-link interruption/reconnection on the chain, management preserved | NOT_RUN; no alternate-path claim |
| M11 | Three representative MPLS nodes, captured transit swap; rotate roles and repeat both directions | NOT_RUN |
| M12 | Controlled MPLS TTL expiry at B with captured incoming TTL and restored normal route | NOT_RUN; exact command/tool support confirmed first |
| M13 | Original four-MPLS-node chain with two successive transit swaps | DEFERRED: fourth MPLS node unavailable |
| M14 | Ordinary-IP Babel learning, preferred path, withdrawal and rejoin on the triangle | NOT_RUN; supported daemon/configuration required |
| M15 | Original four-MPLS-node diamond with an MPLS transit node on each branch | DEFERRED: fourth MPLS node unavailable |
| M16 | Automated GVC installation, transactions and repair | DEFERRED: agent/controller implementation and qualification |
| M17 | New triangle: A–B–D primary, direct A–D alternate, controlled manual path replacement and restoration | NOT_RUN; both branches and return paths qualified first |
| M18 | New standard-TBox access integration: routing, endpoint identity, bidirectional delivery, MTU and resource limits | NOT_RUN; supported no-NAT access configuration |
| M19 | Optional standard-TBox L2 bridge carrying MPLS on the alternate link | NOT_RUN, CONDITIONAL; actual carriage/VLAN/MTU capability required |

Status vocabulary: `NOT_RUN` is an unexecuted candidate; `DEFERRED` retains a broader-plan case outside this hardware/software iteration; `BLOCKED` records a prerequisite found unavailable during preparation; `PASS` and `FAIL` require an executed, agreed case and evidence. A deferred case is not a pass or a removed requirement. Preserve the separately supplied readiness/prerequisite fields in the CSV.

For ordinary delivery, a proposed initial check is 100 small pings each way after neighbor warm-up, with no observed loss and consistent captures. For load, start with 10 Mbit/s UDP for 30 seconds each way and increase only within the chosen lab envelope. These settings are review candidates. They do not establish production CIR, raw frame zero-loss acceptance, 5 Gbit/s service or radio range.

For M17, prove traffic uses B on the primary and bypasses B on the alternate, with the same ingress A, egress D and host addresses. Shared endpoints A/D and access router S1 remain single points for this service. Their failure is an outage experiment, not protected service. The direct alternate has no intermediate MPLS swap; M17 cannot close M13 or M15.

For M19, the bridge must preserve each labeled frame's label/TTL/payload across its two sides, with the required VLAN treatment and MTU. A bridge has no MPLS transit role and adds no label hop. In this optional experiment A and D remain adjacent MPLS peers over a bridged Ethernet segment. Do not introduce GRE/IP tunnels or presume Babel multicast passes without a separate tested design. A bridge-capability failure does not prevent the direct A–D experiment.

**5. Existing scripts**

The copied `routing-gvc/` source is unchanged. From that directory:

```sh
python3 -B -m unittest discover -s tests -v
python3 -B evidence/review_probes.py
python3 -B -m gvc_poc --fixture relay-alternate --protect --diversity shared_risk
```

See [OFFLINE_TEST_USAGE.md](OFFLINE_TEST_USAGE.md) for individual script usage. The CLI uses its existing synthetic fixtures and has no TBox-address or arbitrary-topology-file input. Its demo graph is not a representation of this revised lab. `installed=false`, `capacity_reserved=false` and `babel_protocol_tested=false` remain accurate. These scripts do not automate the manual run sheet.

The copied source README retains historical links and the original four-router target; this top-level guide governs the current hardware iteration. The September 15 validation log is retained under its original date. The current package log identifies any rerun of the offline tests and syntax checks; no such check is a device execution.

**6. Evidence and next step**

Return a bundle containing physical topology/port map, models/builds, source/utility versions, selected scenarios and acceptance criteria, exact commands/configuration, errors, pre/post settings, matched core/access captures, direction-specific traffic and resource logs, fault/operator/recovery timestamps, per-case dispositions and cleanup confirmation. Mark unsupported counters explicitly; aggregate interface statistics are not automatically per-label counts. Identify any capture blind spot caused by acceleration and use a supported external capture path if required.

The co-worker should review [TOPOLOGY_PROPOSAL.md](TOPOLOGY_PROPOSAL.md) and supply the local scenarios, port map and supported modes. Then bind the example run sheet to those interfaces and agreed tests. No extra MPLS hardware is a prerequisite for this revision's chain, triangle and mixed-access proposal.
