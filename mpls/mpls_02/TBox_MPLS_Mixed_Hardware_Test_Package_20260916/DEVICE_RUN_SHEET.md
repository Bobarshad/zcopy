# Proposed manual run sheet — three MPLS TBoxes and standard access TBox

Prepared 2026-09-16. **No device commands in this document have been executed.** This is an example for topology review and later binding to the actual supported firmware/interfaces, not an automatic script. The co-worker will select the environment-specific scenarios and acceptance criteria. Run one reviewed step at a time and stop on any unexpected error.

**1. Role and address map**

```text
Host L -- S1 (ordinary IP) -- A (MPLS) -- B (MPLS) -- D (MPLS) -- Host R
                               \---------------------/
                                   direct alternate
```

S1 uses ordinary IPv4 routing without NAT. The example does not assume bridge support on S1. The alternate A–D is a separate routed Ethernet link. Keep all core links distinct, outside any common bridge; keep management independent.

The following documentation prefixes must be checked against the complete local/VPN/management route space. Have the operator provision and record these addresses, port state, MTUs and scoped firewall permissions using the vendor's supported configuration method before the route commands below. Address/interface provisioning is not automated by this document.

| Link | Left endpoint | Right endpoint |
| --- | --- | --- |
| Host L–S1 | Host L: 192.0.2.2/30 | S1 host port: 192.0.2.1/30 |
| S1–A | S1 core-facing port: 192.0.2.5/30 | A access: 192.0.2.6/30 |
| A–B | A AB: 198.51.100.1/30 | B BA: 198.51.100.2/30 |
| B–D | B BD: 198.51.100.5/30 | D DB: 198.51.100.6/30 |
| A–D alternate | A AD: 198.51.100.9/30 | D DA: 198.51.100.10/30 |
| D–Host R | D access: 203.0.113.1/30 | Host R: 203.0.113.2/30 |

The example assumes 1500-byte core interface MTUs and no additional core encapsulation beyond a single MPLS label. The target inner-IP envelope is 1496 bytes. The ordinary-IP baseline uses a route metric of 1496; the MPLS ingress routes below use `mtu 1500`, because Linux's IPv4 path deducts the four-byte MPLS LWT headroom. Setting that MPLS route metric to 1496 can deduct the overhead twice. Verify the effective PMTU on the actual vendor kernel before adopting these boundary expectations. Recalculate for the actual VLAN/security/datapath limits and record the frame convention. [Linux IPv4 MTU calculation](https://raw.githubusercontent.com/torvalds/linux/v6.12/include/net/ip.h), [MPLS LWT headroom](https://raw.githubusercontent.com/torvalds/linux/master/net/mpls/mpls_iptunnel.c).

On each TBox, use its own root/operator session and replace these placeholders with verified interface names. These are separate per-device blocks:

```sh
# A only
A_ACCESS='REPLACE_A_ACCESS_TO_S1'
A_AB='REPLACE_A_PORT_TO_B'
A_AD='REPLACE_A_ALTERNATE_TO_D'
```

```sh
# B only
B_BA='REPLACE_B_PORT_TO_A'
B_BD='REPLACE_B_PORT_TO_D'
```

```sh
# D only
D_DB='REPLACE_D_PORT_TO_B'
D_DA='REPLACE_D_ALTERNATE_TO_A'
D_ACCESS='REPLACE_D_ACCESS_TO_HOST_R'
```

No MPLS variables, module or label commands apply to S1. If the vendor exposes a different MPLS API, translate and review the complete run sheet before use.

**2. Preflight and saved state**

On S1/A/B/D record model/firmware, port/VLAN/bridge map, NAT/firewall/offload state, link speeds and supported management recovery. Record actual kernel and routing state:

```sh
uname -a
ip -V
ip -d link show
ip -4 address show
ip -4 rule show
ip -4 route show table all
```

On A/B/D also record `ip -f mpls route show`, MPLS kernel/configuration evidence, input settings and supported `net.mpls.ip_ttl_propagate` / `net.mpls.default_ttl` readbacks. Built-in facilities may not appear in `lsmod`. Have the operator load only supported matched modules if needed; do not obtain arbitrary modules from another firmware feed. Confirm labels 200, 300, 301, 400, 500 and 501 are unused on their receiving nodes, and that every /32 route below is free for this test.

In the device's own session, on each of S1/A/B/D:

```sh
MPLS_LAB_OLD_FORWARD=$(cat /proc/sys/net/ipv4/ip_forward)
printf 'old IPv4 forwarding=%s\n' "$MPLS_LAB_OLD_FORWARD"
```

Changing IPv4 forwarding can reset other IPv4 defaults. Save those settings and restore any affected values too, as described by the [Linux IPv4 sysctl documentation](https://docs.kernel.org/networking/ip-sysctl.html). Do not disable security or flush routes/firewalls globally to make the test work.

On each of A/B/D, after supported MPLS facilities are available:

```sh
MPLS_LAB_OLD_LABELS=$(cat /proc/sys/net/mpls/platform_labels)
printf 'old label-table size=%s\n' "$MPLS_LAB_OLD_LABELS"
```

Save the per-interface input settings on the indicated box:

```sh
# A
A_OLD_AB_INPUT=$(cat "/proc/sys/net/mpls/conf/$A_AB/input")
A_OLD_AD_INPUT=$(cat "/proc/sys/net/mpls/conf/$A_AD/input")
printf 'A AB=%s AD=%s\n' "$A_OLD_AB_INPUT" "$A_OLD_AD_INPUT"
```

```sh
# B
B_OLD_BA_INPUT=$(cat "/proc/sys/net/mpls/conf/$B_BA/input")
B_OLD_BD_INPUT=$(cat "/proc/sys/net/mpls/conf/$B_BD/input")
printf 'B BA=%s BD=%s\n' "$B_OLD_BA_INPUT" "$B_OLD_BD_INPUT"
```

```sh
# D
D_OLD_DB_INPUT=$(cat "/proc/sys/net/mpls/conf/$D_DB/input")
D_OLD_DA_INPUT=$(cat "/proc/sys/net/mpls/conf/$D_DA/input")
printf 'D DB=%s DA=%s\n' "$D_OLD_DB_INPUT" "$D_OLD_DA_INPUT"
```

Save the printed values outside the shell. A new SSH session will not retain these variables. Do not proceed with missing/invalid saved values or unresolved interface/route ownership.

**3. Ordinary-IP baseline including standard S1**

On each of S1/A/B/D:

```sh
sysctl -w net.ipv4.ip_forward=1
```

Install host-side routes on the Linux endpoints:

```sh
# Host L
sudo ip route add 203.0.113.2/32 via 192.0.2.1
```

```sh
# Host R
sudo ip route add 192.0.2.2/32 via 203.0.113.1
```

On S1, its host-side network is directly connected. Its forward route goes to A:

```sh
ip route add 203.0.113.2/32 via 192.0.2.6
```

On A, retain the return route to remote Host L through S1:

```sh
ip route add 192.0.2.2/32 via 192.0.2.5 dev "$A_ACCESS"
ip route add 203.0.113.2/32 via 198.51.100.2 dev "$A_AB" mtu 1496
```

On B:

```sh
ip route add 192.0.2.2/32 via 198.51.100.1 dev "$B_BA"
ip route add 203.0.113.2/32 via 198.51.100.6 dev "$B_BD" mtu 1496
```

On D:

```sh
ip route add 192.0.2.2/32 via 198.51.100.5 dev "$D_DB" mtu 1496
```

First verify all directly connected neighbors, then ping between Host L and Host R. Verify the actual route selected by policy rules and capture the original 192.0.2.2/203.0.113.2 addresses through S1. Any NAT or unexpected path must be resolved before MPLS. S1 sees plain IP in both directions. Gather optional ordinary-IP load results with the same MTU/offload settings as the later MPLS run.

**4. Three-node MPLS chain**

On A/B/D only, enlarge the label table if necessary; do not shrink an existing table:

```sh
if [ "$MPLS_LAB_OLD_LABELS" -lt 1024 ]; then
    printf '1024\n' > /proc/sys/net/mpls/platform_labels
fi
```

Enable input only on the isolated primary core receive interfaces:

```sh
# A
printf '1\n' > "/proc/sys/net/mpls/conf/$A_AB/input"
```

```sh
# B
printf '1\n' > "/proc/sys/net/mpls/conf/$B_BA/input"
printf '1\n' > "/proc/sys/net/mpls/conf/$B_BD/input"
```

```sh
# D
printf '1\n' > "/proc/sys/net/mpls/conf/$D_DB/input"
```

Table size and per-interface input are separate controls in [Linux MPLS](https://docs.kernel.org/networking/mpls-sysctl.html). Record label ownership and stage the egress bindings first:

```sh
# D: forward-path egress to directly connected Host R
ip -f mpls route add 300 via inet 203.0.113.2 dev "$D_ACCESS"
```

```sh
# A: reverse-path egress to S1, NOT directly to remote Host L
ip -f mpls route add 500 via inet 192.0.2.5 dev "$A_ACCESS"
```

Next install B's two transit bindings:

```sh
# B: forward swap 200 -> 300 and reverse swap 400 -> 500
ip -f mpls route add 200 as 300 via inet 198.51.100.6 dev "$B_BD"
ip -f mpls route add 400 as 500 via inet 198.51.100.1 dev "$B_BA"
```

Replace only the ordinary-IP baseline entries created by this run sheet, using `change` so a missing route causes an error. On A:

```sh
ip route change 203.0.113.2/32 encap mpls 200 via 198.51.100.2 dev "$A_AB" mtu 1500
```

On D:

```sh
ip route change 192.0.2.2/32 encap mpls 400 via 198.51.100.5 dev "$D_DB" mtu 1500
```

S1 and the hosts keep their ordinary-IP routes. Keep A's return route via S1 and B's baseline routes for ordinary IP/control error handling. The sender ingress routes remain labeled during negative tests; underlying IP reachability is not permission to bypass MPLS. The [iproute2 route manual](https://raw.githubusercontent.com/iproute2/iproute2/main/man/man8/ip-route.8.in) defines these encapsulation/next-hop operations.

Expected forwarding:

| Direction | A–B frame | B–D frame | Access traffic |
| --- | --- | --- | --- |
| Host L → Host R | Label 200 | Label 300 | Plain IP through S1 and after D |
| Host R → Host L | Label 500 (B → A) | Label 400 (D → B) | Plain IP before D and A → S1 → Host L |

Read back on each MPLS box:

```sh
ip -d -4 route show
ip -f mpls route show
```

Inspect the actual forwarded lookup on A and D respectively:

```sh
# A
ip route get 203.0.113.2 from 192.0.2.2 iif "$A_ACCESS"
```

```sh
# D
ip route get 192.0.2.2 from 203.0.113.2 iif "$D_ACCESS"
```

**5. Captures and bounded traffic**

Run separate short captures at A–B, both sides of B and D's access interface. Include S1's access/forward ports for M18. For each chosen interface set its actual name in that device session:

```sh
MPLS_CAPTURE_IF='REPLACE_WITH_ONE_INTERFACE'
tcpdump -ni "$MPLS_CAPTURE_IF" -s 0 -w /tmp/mpls-stage.pcap 'ether proto 0x8847 or arp or icmp'
```

This core filter assumes the example's untagged/logical interface. Adjust for physical VLAN tags or use a short unfiltered capture. For endpoint traffic including the later TCP/UDP tests on a plain-IP access interface:

```sh
tcpdump -ni "$MPLS_CAPTURE_IF" -s 0 -w /tmp/access-stage.pcap 'host 192.0.2.2 or host 203.0.113.2'
```

Stop captures with Ctrl-C and copy them to the result bundle under unique device/interface/stage names before restarting. Bound capture storage during load. Use a supported TAP/SPAN if acceleration hides traffic. Match sequence IDs, payload and inner IP addresses; a successful ping alone is not label evidence. Aggregate counters are not per-label counts.

From Host L, warm neighbors and then record a proposed smoke test:

```sh
ping -c 3 203.0.113.2
ping -c 100 -s 1200 203.0.113.2
```

From Host R:

```sh
ping -c 3 192.0.2.2
ping -c 100 -s 1200 192.0.2.2
```

For MTU, use a ping implementation supporting `-M do`. From Host L:

```sh
ping -c 5 -M do -s 1400 203.0.113.2
ping -c 5 -M do -s 1468 203.0.113.2
ping -c 5 -M do -s 1469 203.0.113.2
```

Repeat from Host R toward 192.0.2.2. With IPv4 without options, 1468 bytes plus IPv4/ICMP headers is 1496 bytes; the single label adds four. The last probe exceeds the example route envelope and should produce controlled rejection/PMTU feedback. Save observations rather than assume where rejection occurs. Label length and Ethernet identification follow [RFC 3032](https://www.rfc-editor.org/rfc/rfc3032.html).

For bounded load, start a dedicated server on Host R:

```sh
iperf3 -s -B 203.0.113.2 -p 5201
```

On Host L:

```sh
iperf3 -c 203.0.113.2 -B 192.0.2.2 -p 5201 -u -b 10M -l 1200 -t 30 -J > udp-forward.json
iperf3 -c 203.0.113.2 -B 192.0.2.2 -p 5201 -u -b 10M -l 1200 -t 30 -R -J > udp-reverse.json
```

Only when the operator is ready for the load of an unrestricted single TCP stream:

```sh
iperf3 -c 203.0.113.2 -B 192.0.2.2 -p 5201 -t 30 -J > tcp-forward.json
iperf3 -c 203.0.113.2 -B 192.0.2.2 -p 5201 -t 30 -R -J > tcp-reverse.json
```

Save the server session too. `-R` reverses data and `-J` emits JSON; see [iperf3 documentation](https://software.es.net/iperf/invoking.html). Record CPU/memory/drops/speeds/offloads on all TBoxes and the hosts. S1 may limit the measured end-to-end throughput. These are candidate test settings; no 5 Gbit/s or frame-level qualification is inferred.

**6. Scoped negative tests on the primary chain**

Perform one fault at a time with captures and a working baseline before/after. Keep the alternate inactive for these checks.

- Missing transit label: on B remove only `ip -f mpls route del 200`, send five Host L → Host R pings, verify label 200 arrives but no corresponding forwarded packet leaves B, then restore B's exact 200→300 entry from section 4.
- Missing egress label: on D remove only `ip -f mpls route del 300`, send five probes, verify label 300 arrives without ordinary-IP delivery, then restore D's exact entry. Repeat reverse-direction cases independently using B's 400 and A's 500 entries. There must be no unlabeled-IP fallback.
- Input gate: on B set only `/proc/sys/net/mpls/conf/$B_BA/input` to 0, send five forward probes, observe the gate and management reachability, then restore it to 1 for the active lab. Restore the pre-lab value only during cleanup. Qualify reverse inputs separately. This does not prove rejection on an untested access port.

For TTL expiry, after confirming the installed iproute2 supports an MPLS TTL attribute, on A temporarily force the outgoing label TTL to 1:

```sh
ip route change 203.0.113.2/32 encap mpls 200 ttl 1 via 198.51.100.2 dev "$A_AB" mtu 1500
```

Send five probes from Host L. Capture MPLS TTL 1 arriving at B and no corresponding forwarding beyond B. Record any supported drop/error evidence; do not require an ICMP error the platform cannot supply. This controls the label TTL directly, avoiding an assumption about the extra ordinary-IP hop through S1. Restore the exact original A route, omitting the temporary TTL override so the saved system TTL policy applies again:

```sh
ip route change 203.0.113.2/32 encap mpls 200 via 198.51.100.2 dev "$A_AB" mtu 1500
```

Verify normal delivery again. A corresponding D ingress route can be used for the independently specified reverse test. Record actual TTL propagation settings and observed packet TTLs.

**7. Direct A–D alternate and manual replacement (M17)**

Qualify the direct neighbors and MTU first. Enable input on the isolated alternate receive interfaces and stage distinct egress labels:

```sh
# A
printf '1\n' > "/proc/sys/net/mpls/conf/$A_AD/input"
ip -f mpls route add 501 via inet 192.0.2.5 dev "$A_ACCESS"
```

```sh
# D
printf '1\n' > "/proc/sys/net/mpls/conf/$D_DA/input"
ip -f mpls route add 301 via inet 203.0.113.2 dev "$D_ACCESS"
```

First, without any fault, switch the two endpoint ingress entries under operator control. On A:

```sh
ip route change 203.0.113.2/32 encap mpls 301 via 198.51.100.10 dev "$A_AD" mtu 1500
```

On D:

```sh
ip route change 192.0.2.2/32 encap mpls 501 via 198.51.100.9 dev "$D_DA" mtu 1500
```

Prove forward label 301 and reverse label 501 on A–D, plain-IP access delivery and no transit through B. The alternate has push/pop only. Retain the primary egress/transit entries so restoring primary is a pair of ingress changes:

```sh
# A
ip route change 203.0.113.2/32 encap mpls 200 via 198.51.100.2 dev "$A_AB" mtu 1500
```

```sh
# D
ip route change 192.0.2.2/32 encap mpls 400 via 198.51.100.5 dev "$D_DB" mtu 1500
```

After verifying both branches separately, run the selected primary-link or B-power failure. Record fault/detection time, confirm the primary failure, execute the alternate ingress changes above and record each command time plus traffic recovery. Repeat each direction and the agreed restoration policy. A and D changes are independent; do not call the operation atomic or its interruption an automatic GVC failover time.

If B is power-cycled, volatile route/label entries may disappear. Before restoring the primary, restore B's recorded test configuration using its supported workflow and reverify it. Do not silently assume persistent state. Failure of A, D or S1 has no surviving service path in this topology.

**8. Separate conditional experiments**

For M14, use a separately reviewed supported Babel configuration and ordinary-IP test destinations or a separate stage. Ensure the measured routes are actually learned and selected by Babel, not shadowed by the static /32 entries above. Control link metrics to obtain the intended initial path, verify neighbors/route withdrawal and capture the changed ordinary-IP data path. Babel convergence alone does not rewrite the explicit static MPLS bindings in this example. S1 may retain static access routes and need not run Babel.

For M19, an additional standard TBox may replace the direct cable with a supported isolated bridge: `A — standard bridge — D`. Prove MPLS EtherType carriage, label/TTL/payload and VLAN preservation, MTU, ARP and any required control multicast before using that segment. It remains one logical MPLS adjacency with a new physical resource/failure point. No MPLS `ip route`, label operation or transit-node pass is assigned to that standard box. Do not bridge A–B and B–D together or create a common Layer-2 triangle.

If only S1 is available and is repurposed as the bridge, it is a separate configuration: reconnect Host L to A, revise the access subnet/next-hop and cleanup records, and review the new run sheet. Do not use the routed-S1 commands unchanged or place one box in two assumed independent roles. Bridge support is optional; the direct A–D test can proceed without it.

**9. Cleanup and evidence**

Stop traffic/server/captures and copy results before cleanup. Remove only test entries actually created, resolving partial-run differences explicitly. Do not flush entire tables.

```sh
# A
ip route del 203.0.113.2/32
ip route del 192.0.2.2/32
ip -f mpls route del 500
# Delete 501 only if the alternate stage installed it.
ip -f mpls route del 501
printf '%s\n' "$A_OLD_AB_INPUT" > "/proc/sys/net/mpls/conf/$A_AB/input"
printf '%s\n' "$A_OLD_AD_INPUT" > "/proc/sys/net/mpls/conf/$A_AD/input"
```

```sh
# B
ip route del 192.0.2.2/32
ip route del 203.0.113.2/32
ip -f mpls route del 200
ip -f mpls route del 400
printf '%s\n' "$B_OLD_BA_INPUT" > "/proc/sys/net/mpls/conf/$B_BA/input"
printf '%s\n' "$B_OLD_BD_INPUT" > "/proc/sys/net/mpls/conf/$B_BD/input"
```

```sh
# D
ip route del 192.0.2.2/32
ip -f mpls route del 300
# Delete 301 only if the alternate stage installed it.
ip -f mpls route del 301
printf '%s\n' "$D_OLD_DB_INPUT" > "/proc/sys/net/mpls/conf/$D_DB/input"
printf '%s\n' "$D_OLD_DA_INPUT" > "/proc/sys/net/mpls/conf/$D_DA/input"
```

On S1 remove its test route `ip route del 203.0.113.2/32`. On Host L remove the added 203.0.113.2/32 route; on Host R remove the added 192.0.2.2/32 route. Restore original forwarding on each of S1/A/B/D using that device's saved value:

```sh
printf '%s\n' "$MPLS_LAB_OLD_FORWARD" > /proc/sys/net/ipv4/ip_forward
```

On A/B/D, before reducing the label table, confirm no other operator or process has added entries that need the enlarged range. Then restore the saved size on that device:

```sh
printf '%s\n' "$MPLS_LAB_OLD_LABELS" > /proc/sys/net/mpls/platform_labels
```

Restore other affected IPv4 defaults, vendor port/address/VLAN/firewall/offload changes and any daemon changes against the saved baseline. Values saved before a reboot must come from the external record, not empty shell variables. Verify management and original services, and record cleanup. Preserve per-case results, exact topology/builds, selected acceptance criteria, captures and timestamps in the returned evidence bundle.
