# September 16 package changes

- Replaced the four-MPLS-unit request with three available MPLS TBoxes A/B/D plus one standard access TBox S1; extra standard units are optional.
- Kept a real three-node MPLS chain for push/swap/pop. Added a direct A–D link for a triangle alternate around transit B, preserving the transport endpoints.
- Added an ordinary-IP, no-NAT role for standard TBoxes. Transparent MPLS carriage through a standard bridge is a separate conditional experiment, never assumed MPLS label-switching support.
- Retained M01–M16 IDs. Applied M01–M10 to the revised mixed path; M03/M04 now terminate on A/D with B in transit. Kept M13 (four-MPLS-node chain) and M15 (four-MPLS-node diamond) explicitly deferred for hardware; M16 remains deferred for software.
- Added M17 triangle manual replacement, M18 standard access integration and M19 optional bridge carriage. Manual switching, Babel route convergence and automated GVC repair have distinct evidence requirements.
- Replaced the earlier two-node configuration example with a three-core-node run sheet, S1 return routing, primary/alternate label bindings and scoped cleanup. Commands remain unexecuted device examples requiring interface and firmware confirmation.
- Corrected the MTU examples for Linux's MPLS LWT headroom deduction and made the temporary TTL override restore the original route policy exactly. Effective PMTU still requires validation on the actual firmware.
- Preserved the offline Python source and tests unchanged; separated detailed offline usage from device instructions. Historical test evidence keeps its original date.
- Rewrote the email as a proposal for the co-worker to review before supplying their environment-specific scenarios. A fourth MPLS unit no longer delays the initial validation.
- Published a new dated ZIP. September 15 ZIPs are historical four-MPLS proposals; send the September 16 mixed-TBox package for this request.
