# Four-TBox MPLS lab: existing scripts and proposed device procedures

Prepared: 2026-09-15. Existing offline tests were rerun successfully today. The device commands below are a **proposed manual run sheet, not an existing automated hardware test or a result from the office TBoxes**. Substitute the actual interface names and have the lab operator confirm the isolated topology before applying them.

Equipment request updated: **four physical MPLS-capable TBoxes total, including two additional units**. Reuse them for two-node, three-node, four-node chain and diamond configurations. The detailed commands in sections 5–7 cover the initial two-node stage; section 8 specifies the additional scenarios whose per-interface run sheets follow the supplied port map. Use representative TBoxes for all forwarding roles.

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

From the ZIP, instead change into its extracted `routing-gvc/` directory. Expected result: `Ran 86 tests ...` followed by `OK`, exit 0. Today's quiet run passed all 86 tests. A result of zero discovered tests is not acceptable.

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

**3. What to ask the co-worker to provide**

- Four TBoxes with exact model, hardware revision, firmware/SDK build and kernel version recorded. Ask whether “MPLS-capable” means standard Linux kernel forwarding, a vendor hardware API, or only transparent carriage of labeled Ethernet frames. Use the same supported build where applicable and record differences.
- Supported MPLS kernel facilities and matching utilities. For the Linux path these include MPLS routing, IP tunnel encapsulation and their dependencies, supplied built-in or as matched modules such as `mpls_router`, `mpls_iptunnel` and, where required by the build, `mpls_gso`. An empty `lsmod` result alone does not prove absence: a facility may be built-in.
- Full iproute2 `ip` with MPLS support, `tcpdump`, `ping`, and preferably `iperf3`; packet capture on the test link, with TAP/SPAN if the vendor datapath is invisible to host capture. Use a ping implementation supporting `-M do` for the MTU examples.
- Two Linux traffic endpoints, or equivalent independent traffic-generator ports. They do not need MPLS support. Running traffic on the TBoxes themselves is a useful preliminary smoke test but omits the external client forwarding path.
- Separate management/recovery access and dedicated routed test ports. The inter-TBox and access ports must not share a bridge that bypasses the intended forwarding. Keep the initial test wired and isolated.
- For the diamond, A and D each need three isolated data connections: two peer links plus one traffic-host link. B and C each need two peer connections. Provide at least six data patch cables, suitable independent TBox power, and the separate management connections. Rotating roles may require remapping ports. Qualified VLANs can provide logical adjacencies where necessary, but shared trunks/switches must be recorded as shared capacity and failure resources.
- A record of firewall, NAT, policy-routing and offload behavior. Permit the specific test flows and ICMP needed for diagnosis; do not flush the firewall or change the management route. Record vendor-supported offload settings and keep them consistent for comparisons.

**4. Initial two-TBox stage and proposed acceptance scenarios**

```text
Host A -- plain IPv4 -- TBox A ===== MPLS ===== TBox B -- plain IPv4 -- Host B
                         |                       |
                         +-- separate management +
```

Both TBoxes act as ingress in one direction and egress in the other. The following are proposed lab checks, not production acceptance thresholds.

| ID | Scenario | Proposed success evidence |
| --- | --- | --- |
| M01 | Identity, software and capability | Exact build/module/tool records; MPLS configuration and route readback work |
| M02 | Ordinary IPv4 baseline | Host A/B reach each other in both directions; management remains reachable; no NAT changes their identities |
| M03 | A-to-B MPLS | A pushes label 200, B pops it; correct plain IPv4 packet reaches Host B |
| M04 | B-to-A MPLS | B pushes label 100, A pops it; correct plain IPv4 packet reaches Host A |
| M05 | Repeated bidirectional delivery | After neighbor warm-up, 100 small pings each way with zero observed loss, plus correctly matched captures |
| M06 | Missing/unknown label | Small bounded probe is dropped when the receiver's test binding is absent; restoring it restores delivery; management survives |
| M07 | Trusted MPLS input | Disabling input only on the isolated core interface stops that direction's labeled delivery; restoring the prior setting restores it |
| M08 | MTU and PMTUD | Agreed packet sizes pass; oversize DF packets fail predictably or receive correct feedback, with no unexplained black hole |
| M09 | Basic load and resources | Low-rate UDP, then bounded TCP in each direction; record actual rate/loss/retransmissions/CPU/memory; compare ordinary IP under the same settings |
| M10 | Link interruption and restoration | Disconnect only the test cable; delivery stops and resumes after reconnection, management survives; record interruption. This is recovery, not alternate-path failover |
| M11 | Transit SWAP | Use three of the four requested TBoxes and a second MPLS segment; see section 8 |

For M03–M05, ping alone is insufficient. Capture labeled core frames and plain access-side frames with matching inner source/destination and packet identity. A switch passing EtherType 0x8847 transparently is not evidence of a label operation. Readback of a route alone is also insufficient. Save aggregate counters, but do not call them per-label counters unless the backend supplies that specific measurement.

**5. Proposed Linux manual run sheet**

This example assumes the supported TBox interface is standard Linux/iproute2. A vendor-only MPLS API needs its own equivalent commands. No command in this section has been executed against the office TBoxes.

Have the operator provision the following unused, isolated addresses and record the interface mapping. These documentation address ranges are examples; check for conflicts with all local routes, VPNs and management networks first.

| Device/interface | IPv4 address | Role |
| --- | --- | --- |
| Host A test NIC | 192.0.2.2/30 | Traffic source/sink |
| TBox A access | 192.0.2.1/30 | Plain IPv4 toward Host A |
| TBox A core | 198.51.100.1/30 | MPLS toward B |
| TBox B core | 198.51.100.2/30 | MPLS toward A |
| TBox B access | 203.0.113.1/30 | Plain IPv4 toward Host B |
| Host B test NIC | 203.0.113.2/30 | Traffic source/sink |

Assume core MTU 1500 and one transport label. Record actual MTUs and all VLAN/security overhead. The example uses a conservative test-route IP MTU of 1496 to allow four bytes for the label. This does not qualify other encapsulations or vendor frame limits.

On each TBox set shell variables to its own actual port names. The following placeholder values must be replaced; they are not detected automatically:

```sh
MPLS_CORE_IF='REPLACE_WITH_CORE_INTERFACE'
MPLS_ACCESS_IF='REPLACE_WITH_ACCESS_INTERFACE'
```

Before changes, save the operator session and the following readbacks from each box to the result bundle:

```sh
uname -a
ip -V
ip -d link show
ip -4 address show
ip -4 rule show
ip -4 route show table all
ip -f mpls route show
ip -s link show dev "$MPLS_CORE_IF"
sysctl net.ipv4.ip_forward
cat /proc/sys/net/mpls/platform_labels
cat "/proc/sys/net/mpls/conf/$MPLS_CORE_IF/input"
```

Also save firmware identity, relevant firewall/offload configuration and bridge membership using the vendor's supported read-only commands. A missing `/proc/sys/net/mpls` may mean modules are not loaded, not necessarily that the image lacks them. Ask the operator to load supported matched modules if appropriate; do not install arbitrary packages from another kernel build.

After the MPLS facilities are available, capture restoration values in the same shell on each TBox:

```sh
MPLS_OLD_FORWARD=$(cat /proc/sys/net/ipv4/ip_forward)
MPLS_OLD_LABELS=$(cat /proc/sys/net/mpls/platform_labels)
MPLS_OLD_INPUT=$(cat "/proc/sys/net/mpls/conf/$MPLS_CORE_IF/input")
printf 'forward=%s labels=%s input=%s\n' "$MPLS_OLD_FORWARD" "$MPLS_OLD_LABELS" "$MPLS_OLD_INPUT"
```

Record those numbers outside the shell as well. Changing `ip_forward` resets IPv4 configuration defaults, as documented in the [Linux IPv4 sysctl reference](https://docs.kernel.org/networking/ip-sysctl.html); have the operator record and restore any affected per-interface settings. Use boxes dedicated to this test.

Verify labels 100 and 200 and the example /32 routes are unused. Use `add`, not `replace`, for initial configuration so an existing entry causes a stop for inspection. Run steps individually and stop if any prerequisite or command fails.

On Host A, after its local IP address is provisioned:

```sh
sudo ip route add 203.0.113.2/32 via 192.0.2.1
```

On Host B:

```sh
sudo ip route add 192.0.2.2/32 via 203.0.113.1
```

On both TBoxes as root:

```sh
sysctl -w net.ipv4.ip_forward=1
```

For an initial ordinary-IP baseline, on A:

```sh
ip route add 203.0.113.2/32 via 198.51.100.2 dev "$MPLS_CORE_IF" mtu 1496
```

On B:

```sh
ip route add 192.0.2.2/32 via 198.51.100.1 dev "$MPLS_CORE_IF" mtu 1496
```

Confirm each directly connected neighbor is reachable, then ping between the hosts. Fix ordinary routing/firewall issues before MPLS. Collect any desired baseline load results using the commands below. Then remove only these newly created baseline host routes: `ip route del 203.0.113.2/32` on A and `ip route del 192.0.2.2/32` on B.

On both TBoxes, enable the isolated MPLS receive interface. Increase the label table only if needed; do not shrink an existing table:

```sh
if [ "$MPLS_OLD_LABELS" -lt 1024 ]; then
    printf '1024\n' > /proc/sys/net/mpls/platform_labels
fi
printf '1\n' > "/proc/sys/net/mpls/conf/$MPLS_CORE_IF/input"
```

The table size must exceed the highest installed label; interface input is a separate control. The [Linux MPLS documentation](https://docs.kernel.org/networking/mpls-sysctl.html) describes both settings and warns that reducing the table size removes entries outside it.

Install egress bindings first. On A, label 100 pops to Host A:

```sh
ip -f mpls route add 100 via inet 192.0.2.2 dev "$MPLS_ACCESS_IF"
```

On B, label 200 pops to Host B:

```sh
ip -f mpls route add 200 via inet 203.0.113.2 dev "$MPLS_ACCESS_IF"
```

Then enable ingress. On A:

```sh
ip route add 203.0.113.2/32 encap mpls 200 via 198.51.100.2 dev "$MPLS_CORE_IF" mtu 1496
```

On B:

```sh
ip route add 192.0.2.2/32 encap mpls 100 via 198.51.100.1 dev "$MPLS_CORE_IF" mtu 1496
```

These use standard route encapsulation and MPLS next-hop operations; see the [iproute2 route manual](https://raw.githubusercontent.com/iproute2/iproute2/main/man/man8/ip-route.8.in) and the [Linux MPLS implementation](https://raw.githubusercontent.com/torvalds/linux/master/net/mpls/af_mpls.c). Babel and a GVC agent are not required for this static forwarding test. Babel adjacency and learned-route behavior should be a separate subsequent test with the supplied daemon/configuration.

Read back on both boxes:

```sh
ip -d -4 route show
ip -f mpls route show
```

On A, inspect the forwarded lookup too:

```sh
ip route get 203.0.113.2 from 192.0.2.2 iif "$MPLS_ACCESS_IF"
```

On B:

```sh
ip route get 192.0.2.2 from 203.0.113.2 iif "$MPLS_ACCESS_IF"
```

The test destinations must select their intended labeled routes, including under any existing policy rules.

**6. Traffic, captures and expected results**

In separate terminal sessions on both TBoxes, start captures before a small probe:

```sh
tcpdump -ni "$MPLS_CORE_IF" -s 0 -w /tmp/mpls-core.pcap 'ether proto 0x8847 or arp or icmp'
```

Also capture on each access interface in another session:

```sh
tcpdump -ni "$MPLS_ACCESS_IF" -s 0 -w /tmp/mpls-access.pcap 'host 192.0.2.2 or host 203.0.113.2'
```

Stop each capture with Ctrl-C after the small test; copy files promptly to the workstation using distinct A/B names. These filters assume the logical Ethernet interfaces in the example. For a physical VLAN trunk, use an appropriate VLAN-aware filter or a short unfiltered capture. If hardware offload hides traffic, use the supported mirror/TAP path. Do not interpret an empty host capture as proof of no forwarding. Avoid indefinite full-packet captures during load tests on small `/tmp` storage.

On Host A, warm the path, then record the second run:

```sh
ping -c 3 203.0.113.2
ping -c 100 -s 1200 203.0.113.2
```

On Host B:

```sh
ping -c 3 192.0.2.2
ping -c 100 -s 1200 192.0.2.2
```

Expected core labels are 200 toward B and 100 toward A, with a single label and bottom-of-stack set. Access-side packets should be ordinary IPv4 retaining the original host addresses. Match ICMP sequence/identifiers and payloads across captures; exclude ordinary IP, NAT or bridging bypass.

For M06, temporarily delete only B's test label while A's ingress route remains installed:

```sh
ip -f mpls route del 200
```

Send five pings from Host A. Confirm labeled packets arrive at B but are not delivered to Host B as plain IP. Restore on B:

```sh
ip -f mpls route add 200 via inet 203.0.113.2 dev "$MPLS_ACCESS_IF"
```

Delivery should resume. Repeat symmetrically for A's label 100. Preserve ingress routes during the negative test so deleting a label cannot accidentally change the sender into an ordinary-IP test.

For M07, on B temporarily set only its isolated core input to zero:

```sh
printf '0\n' > "/proc/sys/net/mpls/conf/$MPLS_CORE_IF/input"
```

Send five probes A-to-B, observe rejection, then restore input to 1 for the remaining lab tests. Repeat on A. This tests the input gate; testing rejection on actual untrusted access ports needs a separate controlled label injector and is not implied by this result.

For M08, with IPv4 headers without options and a 1500-byte core payload MTU, 1468 bytes of ICMP data plus 28 bytes of IPv4/ICMP plus one four-byte label totals 1500. On Host A:

```sh
ping -c 5 -M do -s 1400 203.0.113.2
ping -c 5 -M do -s 1468 203.0.113.2
ping -c 5 -M do -s 1469 203.0.113.2
```

Repeat toward Host A from Host B. The first two sizes should fit the configured envelope; the last exceeds it. Record whether the sender rejects it after PMTU discovery or receives appropriate ICMP feedback. A successful fit test alone does not establish all PMTUD/fragmentation behavior. Different VLAN, encryption or driver limits require recalculation.

For M09, run a dedicated test server on Host B:

```sh
iperf3 -s -B 203.0.113.2 -p 5201
```

From Host A, begin with bounded UDP, one direction at a time:

```sh
iperf3 -c 203.0.113.2 -B 192.0.2.2 -p 5201 -u -b 10M -l 1200 -t 30 -J > udp-a-to-b.json
iperf3 -c 203.0.113.2 -B 192.0.2.2 -p 5201 -u -b 10M -l 1200 -t 30 -R -J > udp-b-to-a.json
```

Then, within the lab's agreed resource ceiling, run a single TCP stream each way. The following TCP commands offer as much load as that stream can achieve; omit them until the operator is ready for that load:

```sh
iperf3 -c 203.0.113.2 -B 192.0.2.2 -p 5201 -t 30 -J > tcp-a-to-b.json
iperf3 -c 203.0.113.2 -B 192.0.2.2 -p 5201 -t 30 -R -J > tcp-b-to-a.json
```

`-R` reverses the data direction; `-J` produces JSON. These are [standard iperf3 options](https://software.es.net/iperf/invoking.html). Save receiver as well as sender results, TBox CPU/memory, negotiated port rates, drops and offload state. If the installed version supports `--get-server-output`, it can collect server output in the client result. Otherwise save the server session separately. Stop the dedicated server with Ctrl-C afterward.

Ten Mbit/s and 30 seconds are proposed smoke-test settings, not a Relay service requirement. Repeat at agreed higher rates and packet sizes only after the basic test passes. Use equal MTU, traffic and offload settings for ordinary-IP/MPLS comparisons. iperf3 does not replace frame-level acceptance testing, encryption qualification, RF-distance testing or the proposed 5 Gbit/s requirement.

**7. Cleanup**

Stop traffic and captures first. Remove only entries created for this test; do not flush a routing table. On A:

```sh
ip route del 203.0.113.2/32
ip -f mpls route del 100
```

On B:

```sh
ip route del 192.0.2.2/32
ip -f mpls route del 200
```

Before restoring the label-table size, confirm no concurrent operator installed labels needing the larger table. A reduction removes entries outside the restored size. Once confirmed, on each box use its own saved values:

```sh
printf '%s\n' "$MPLS_OLD_INPUT" > "/proc/sys/net/mpls/conf/$MPLS_CORE_IF/input"
printf '%s\n' "$MPLS_OLD_FORWARD" > /proc/sys/net/ipv4/ip_forward
printf '%s\n' "$MPLS_OLD_LABELS" > /proc/sys/net/mpls/platform_labels
```

Restore any affected IPv4 defaults, firewall/offload changes and test interface configuration using the saved baseline. On Host A remove its added remote /32 route; on Host B remove its added remote /32 route. The lab operator should remove only addresses/port changes introduced for this run and verify management and original services still work. Do not rely on reboot as the cleanup mechanism.

**8. Three- and four-TBox stages**

The equipment request is four physical TBoxes total, with two external Linux traffic hosts. Reuse the same units in these topologies; four plus three plus two are not separate equipment allocations.

| Configuration | Physical TBoxes used | Purpose |
| --- | --- | --- |
| A–B | 2 | Initial M01–M10 push/pop and diagnostic checks |
| A–B–D | 3 | One transit swap, reverse path and TTL expiry |
| A–B–C–D | 4 | Two successive transit swaps and accumulated forwarding/MTU/resource behavior |
| A–B–D and A–C–D | 4 | Primary/alternate paths with the same endpoints, link/transit-node failures and path replacement |

For the three-node chain, use `Host A — TBox A — TBox B — TBox D — Host B`. For example, observe A pushing 200, B swapping 200 to 300 and D popping 300. Capture both sides of B to prove the label change. Rotate the units through the transit role and specify/test the reverse bindings independently. Stage egress and transit entries before ingress. The initial two-node command blocks must not be copied unchanged into this topology: B is now a transit node, and D owns the egress binding.

For the four-node chain, observe two real transit operations, for example 200 → 300 at B and 300 → 400 at C, followed by pop at D. Use unused receiver-owned labels and separate adjacencies, preserving one label in each core packet. Measure the end-to-end path in both directions; a per-link result does not replace the chain result.

For the diamond:

```text
                  TBox B
                 /      \
Host A — TBox A              TBox D — Host B
                 \      /
                  TBox C
```

The four core links are A–B, B–D, A–C and C–D. Verify A–B–D and A–C–D separately before any failure. Install distinct destination label bindings where both paths enter D (and A for return traffic), record port/risk sharing and keep the circuit endpoints fixed. Disconnect one primary link or power down only the primary transit TBox B, then validate the surviving path and restore the original condition. Repeat with roles reversed. Power loss of A or D removes the shared circuit endpoint and is an outage case; the diamond does not provide redundant endpoints or NAT-owner migration.

| ID | Additional scenario | Proposed success evidence / prerequisite |
| --- | --- | --- |
| M11 | Three-TBox push/swap/pop | Per-hop captures prove one label, actual swap, preserved endpoint identity and bidirectional delivery; rotate transit roles |
| M12 | Controlled TTL expiry | Low-volume labeled probes reach the selected transit hop with an expiring TTL and are not forwarded beyond it; verify captured TTL, configured TTL policy and supported drop/error evidence; restore and retest normal delivery |
| M13 | Four-TBox chain | Captures prove both swaps in each specified direction; MTU, traffic and resource results identify the complete tested path |
| M14 | Babel route learning/withdrawal | With the supported daemon/configuration, record neighbors, ordinary-IP routes and withdrawal/recovery for a link and transit-node failure; verify actual ordinary-IP path changes. This does not establish repair of a static MPLS circuit |
| M15 | Diamond paths and controlled manual replacement | Both preconfigured MPLS paths work; a link/transit failure is observed; an operator switches ingress to the surviving path and back. Record command times separately from failure/detection, packet loss, return path and surviving capacity. Report the switch as manual |
| M16 | Automated GVC installation/repair | Requires the implemented and qualified agent/controller. Test prepare/readback/commit, rollback and automated protection under load. Keep NOT_RUN until software prerequisites exist; successful manual replacement does not pass this case |

For M12, finalize the bounded injection method against the installed utilities and exact TTL propagation behavior before executing it. For M14, run the ordinary-IP/Babel test distinctly from static MPLS and capture the changed data path. An underlay route withdrawal by itself does not rewrite the explicit MPLS label/next-hop bindings. For M15, compare surviving capacity against a pre-agreed lab traffic rate and record loss/interruption; no automatic repair-time target is claimed from a human-operated change.

The four-unit requirement follows the existing Phase 0 three-node forwarding and four-node diamond plan. It covers these wired bench topologies. Exact reproduction of larger model fixtures, longer RF chains, independent exit services or site-specific N+1 protection needs a separately mapped equipment list. The Python fixtures remain offline tests; their size alone does not require that many physical units. Shared-capacity admission, automated GVC repair and production RF/service acceptance keep their respective software and qualification prerequisites.

**9. Results to request**

Ask for a single bundle containing topology/port mapping; exact firmware, kernel and tool versions; before/after routes and settings; commands and errors; paired packet captures; ping and iperf logs; CPU/memory/interface counters and offload settings; a PASS/FAIL/BLOCKED/NOT_RUN entry for each M01–M16 case; and cleanup confirmation. Report unsupported counters explicitly. Record dates, operators, traffic direction, frame/packet size conventions and the tested build.

M01–M10 are a two-node functional subset relevant to Phase 0 P0-T08/T09/T10/T12. M11–M15 extend the physical bench evidence using the requested units; M16 depends on subsequent software integration. These checks do not automatically close the complete qualification procedures, and no device test is marked passed merely because the offline Python suite passes.

**10. Suggested reply to the co-worker**

> Thank you for making two MPLS-capable TBoxes available. Please arrange four MPLS-capable TBoxes in total, including two additional units, so we can test transit forwarding and alternate paths on representative TBox hardware.
>
> We will reuse these four units in the following configurations:
>
> 1. Two-TBox path: ordinary-IP baseline, MPLS push/pop in both directions, packet captures, missing-label/input-gate checks, MTU/PMTUD, and bounded UDP/TCP load.
> 2. Three-TBox chain: ingress push, intermediate label swap and egress pop, including reverse traffic and controlled TTL-expiry tests. Rotate the transit role to check each unit.
> 3. Four-TBox chain: repeated label swapping across two transit nodes, bidirectional delivery, MTU and load/resource measurements.
> 4. Four-TBox diamond: primary A-B-D and alternate A-C-D, keeping the same ingress A and egress D. Test each path, primary-link failure, transit-TBox failure, restoration and reverse-path operation. Record packet loss, interruption time and surviving-path capacity. Test Babel route learning and withdrawal separately from MPLS path switching.
>
> Please also provide two Linux traffic hosts, suitable data cables and independent power for the TBoxes, plus separate management/recovery access. For the diamond, A and D each need three isolated data connections: one to a traffic host and two to their TBox peers. B and C each need two peer connections. Please send the available port/interface map so we can finalize the run sheets.
>
> Please confirm each unit's model, hardware revision, firmware/SDK and kernel version, whether standard Linux/iproute2 MPLS is supported, and availability of tcpdump, iperf3 and Babel. Return the exact configuration, captures, traffic logs and CPU/memory/drop measurements for each case.
>
> Our offline routing/GVC suite has passed 86 tests. Initial device validation will use static MPLS entries and controlled manual path switching; automated GVC installation and repair remain a later software integration step. We will record those cases separately until the agent is ready. The four-unit lab covers these bench topologies; any longer-chain or site-specific protection test will include its own equipment requirement.
