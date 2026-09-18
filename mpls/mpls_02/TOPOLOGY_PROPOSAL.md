# Revised proposal: three MPLS TBoxes with standard TBoxes

Prepared: 2026-09-16. Status: **proposal for the co-worker's review**. The available equipment is three MPLS-capable TBoxes and one or more standard TBoxes. The scenarios below are proposed checks; the co-worker will confirm the final scenarios and criteria for their environment. No office-device result is claimed.

We can begin with **three MPLS-capable TBoxes, one standard TBox and two traffic hosts**. The standard TBox participates in the ordinary-IP access path. The MPLS TBoxes provide ingress, transit and egress forwarding. Adding a direct link between the same MPLS endpoints provides a useful alternate path without waiting for another MPLS unit.

## 1. Equipment and roles

| Device | Capability and proposed role | Data connections for the triangle |
| --- | --- | --- |
| A | MPLS ingress/egress | 3: S1, B, D |
| B | MPLS transit; rotate this role among all three units | 2: A, D |
| D | MPLS egress/ingress | 3: B, A, Host R |
| S1 | Standard TBox; ordinary IPv4 access router with no NAT | 2: Host L, A |
| Host L / Host R | Independent traffic endpoints; MPLS support unnecessary | 1 each |

Provide separate management/recovery access, independent TBox power and **five data cables for the chain or six for the triangle**, excluding management and capture connections. A second standard TBox, S2, may be inserted between D and Host R; it needs two data connections and adds one cable.

Each connection means an isolated data adjacency. Dedicated physical ports are preferred for the initial test. If supported VLANs are necessary, record the shared switch/trunk, capacity and failure dependencies. A physical cable or power failure then may affect multiple logical links. Do not join the core links into one common Ethernet bridge.

Confirm model, hardware revision, firmware/SDK, kernel, supported MPLS interface and capture/traffic utilities before finalizing the port map. Availability of MPLS forwarding does not by itself establish Babel availability or hardware-offload visibility.

## 2. Initial chain: access integration and one transit swap

```text
Host L -- IPv4 -- S1 -- IPv4 -- A ===== MPLS ===== B ===== MPLS ===== D -- IPv4 -- Host R
                 standard      push              swap              pop
                               <---------- reverse roles apply ----------
```

For forward traffic, A adds the label, B swaps it and D removes it. For reverse traffic, D adds the label, B swaps it and A removes it before forwarding ordinary IP toward S1. Configure and verify each direction independently. Rotate physical units through the transit role so each MPLS TBox is exercised as a label-switching node.

S1 routes ordinary IPv4 only, preserves the original endpoint addresses and has an explicit return path. It needs no MPLS bindings. The reverse egress on A forwards to S1's directly connected address, not directly to Host L's address on the remote subnet. If S1 cannot provide the required routed mode, a vendor-supported **access bridge carrying ordinary IP** can be reviewed as an alternative; record the selected mode and update addressing before running it.

Proposed checks are the ordinary-IP baseline, bidirectional push/swap/pop, matched captures, unknown-label and input-gate behavior, controlled TTL expiry, MTU/PMTUD, bounded UDP/TCP load and interruption/restoration. Capture both sides of B to establish a real label swap. The additional IP hop through S1 changes host-to-core TTL accounting, so TTL-expiry checks must inspect the label TTL at the intended transit hop.

## 3. Triangle: alternate path using the same three MPLS units

```text
                  +-------- B --------+
                  |                   |
Host L -- S1 ---- A ===== direct ===== D -- Host R

Primary MPLS path:   A -> B -> D
Alternate MPLS path: A ------> D
```

The three core links are A–B, B–D and A–D. A and D remain the circuit endpoints. Qualify each path in both directions before introducing failures. The alternate uses push/pop without an intermediate swap; it complements the transit test in the primary chain.

Proposed failure checks include interruption of A–B or B–D, loss of transit B, interruption of the direct alternate link and restoration. Preconfigure each path, then use controlled **manual ingress-path replacement** to demonstrate delivery over the surviving path. Record failure time, operator action, interruption, loss, return-path behavior and surviving capacity separately. A human-operated switch does not demonstrate automated GVC repair or a controller restoration-time target.

The paths have different internal nodes and links, but share A, D, both access paths and any common power/switch/trunk infrastructure. These shared resources remain common failure points. Losing A, D or S1 is an outage case in this proposal. The triangle cannot establish redundant endpoints, access-router redundancy or NAT-owner migration.

If a supported Babel build is available, ordinary-IP route learning and withdrawal can be tested separately on these links. Babel recovery alone does not change a manually installed MPLS circuit; automated GVC installation and repair remain dependent on the implemented agent/controller.

## 4. Optional experiment: standard TBox as a transparent core bridge

After the direct triangle works, a spare standard TBox may be considered for this separate experiment:

```text
A ==== MPLS Ethernet ==== standard TBox in qualified bridge mode ==== MPLS Ethernet ==== D
```

This is conditional, not a prerequisite. Ethernet bridging forwards using link-layer information; it is distinct from IP routing. Whether a particular standard TBox transparently carries labeled frames depends on its supported bridge, filtering and datapath behavior and must be verified. [Linux Ethernet bridging documentation](https://docs.kernel.org/networking/bridge.html)

Validate unicast MPLS EtherType `0x8847`, VLAN handling, label preservation, MTU and captures on both sides. A label adds four bytes; account for the actual complete frame format. [RFC 3032, MPLS label encoding and LAN carriage](https://www.rfc-editor.org/rfc/rfc3032.html)

The bridge adds no MPLS label operation and cannot substitute for a second transit label-switching node. A standard TBox in ordinary routed-IP mode cannot directly replace a bare-MPLS core hop. MPLS-over-IP tunnels would introduce a separate encapsulation and test design and are not assumed here. Reusing S1 for this experiment requires a separate run and recabling; do not count it as simultaneously providing the access-router function.

## 5. Coverage and remaining work

| Available now, subject to supported configuration | Deferred or separate |
| --- | --- |
| Standard-IP access integration and MPLS push/swap/pop | Two successive transit swaps through four physical MPLS TBoxes |
| Transit-role rotation, TTL, MTU and bounded load | Symmetric four-MPLS diamond with one transit MPLS unit on each branch |
| Triangle link/transit-node failures and manual path replacement | Automated GVC installation, rollback and repair until the agent is ready |
| Optional qualified standard-box bridge carriage | RF range, production throughput, complete site N+1 and service acceptance |

A fourth MPLS unit can extend the bench later. It is not a prerequisite for the proposed chain and triangle work. Standard boxes do not erase the deferred MPLS coverage gaps.

**Next step:** ask the co-worker to review this topology, identify available ports and standard-box operating modes, and specify their test scenarios, offered traffic rates and evidence requirements. Finalize the addressing, per-device run sheets and acceptance criteria against that response. Eligible unexecuted cases remain `NOT_RUN`; cases waiting for additional hardware or software are `DEFERRED`. Offline Python model results are recorded separately.
