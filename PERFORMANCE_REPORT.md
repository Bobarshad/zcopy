# Costk Weak-Signal Throughput Evaluation

Test dates: 2026-07-18 and 2026-07-19

## Executive summary

This evaluation supports the claim that `costk` can improve effective TCP
throughput and consistency when Wi-Fi signal quality is poor.

At strong signal, direct forwarding was fastest. At -70 dBm, corrected-splice
`costk` completed the same five-file workload in 23.75 seconds, compared with
88.87 seconds for direct forwarding. That corresponds to 249.1 Mbit/s aggregate
throughput with `costk` and 66.6 Mbit/s without it. Fixed-splice `costk` also
produced a much narrower range of results: 241.5 to 262.4 Mbit/s, compared with
16.3 to 322.4 Mbit/s direct.

The weak-signal result demonstrates a benefit in this test, but it is not yet
universal proof. The direct result contained one severe slow-tail event, the
sample size was five runs per mode, and the source was a public CDN. A larger
controlled experiment is required before making a general product claim.

## What costk does

`costk` terminates the client TCP connection on the Access Point and creates a
second connection from the AP to the server:

```text
client <-- wireless TCP --> AP / costk <-- wired TCP --> server
```

This separation can prevent wireless loss and retransmission behavior from
directly reducing the congestion window of the long Internet connection. The
tested firmware used `cubic` on the available TCP connections; the custom
wireless-aware `wtcp` congestion-control algorithm was not installed.

## Method

The same Linux kernel archive was used for every run:

```text
https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.12.1.tar.xz
147,912,528 bytes
```

Every measurement used one wget attempt, no explicit timeout, and no disk
write:

```sh
/usr/bin/time -f '%e' \
  wget --verbose --tries=1 -O /dev/null "$URL"
```

Three modes were rotated across five rounds:

1. Direct forwarding with `costk` stopped.
2. Buffered `costk` with `COSTK_USE_SPLICE=0`.
3. Corrected zero-copy splice with `COSTK_USE_SPLICE=1` and a 65,536-byte chunk.

The broken splice implementation was not used. Connection logging was disabled
during proxy measurements. Aggregate throughput is calculated as total bytes
from all five files divided by total elapsed time. Median per-run throughput is
also reported to distinguish normal speed from tail behavior.

## Radio conditions

| Condition | RSSI | SNR | AP-to-client PHY rate |
| --- | ---: | ---: | ---: |
| Near AP | -38 dBm | 59 dB | up to 2401 Mbit/s |
| Farther location | -70 dBm | 27 dB | normally 864 Mbit/s; one 576 Mbit/s sample |

The AP WAN link was 1 Gbit/s full duplex in both tests.

## Strong-signal results

| Mode | Five-file time | Aggregate throughput | Median run rate |
| --- | ---: | ---: | ---: |
| Direct | 7.24 s | 817.2 Mbit/s | 827.5 Mbit/s |
| Buffered `costk` | 8.25 s | 717.2 Mbit/s | 730.4 Mbit/s |
| Fixed-splice `costk` | 7.94 s | 745.2 Mbit/s | 753.7 Mbit/s |

At strong signal, fixed-splice `costk` was 8.8% slower than direct forwarding.
It was 3.9% faster than buffered `costk`. This is expected when the wireless
path is already reliable: splitting TCP adds processing without providing much
loss isolation benefit.

## Weak-signal results

| Mode | Five-file time | Aggregate throughput | Median run rate | Observed range |
| --- | ---: | ---: | ---: | ---: |
| Direct | 88.87 s | 66.6 Mbit/s | 300.3 Mbit/s | 16.3-322.4 Mbit/s |
| Buffered `costk` | 34.61 s | 170.9 Mbit/s | 236.2 Mbit/s | 83.9-248.1 Mbit/s |
| Fixed-splice `costk` | 23.75 s | 249.1 Mbit/s | 248.6 Mbit/s | 241.5-262.4 Mbit/s |

For the complete weak-signal workload, fixed-splice `costk`:

- Increased aggregate throughput by 274.2% compared with direct forwarding,
  or 3.74 times the direct result.
- Reduced total completion time by 73.3% compared with direct forwarding.
- Increased aggregate throughput by 45.7% compared with buffered `costk`.
- Kept every run between 241.5 and 262.4 Mbit/s.
- Reduced the coefficient of variation from 47.5% direct to 2.9%.

The direct median remained 17.2% faster than the splice median. This means
`costk` did not increase the best or typical direct speed in this sample. Its
measured benefit was avoiding severe slow-tail behavior and providing stable
batch throughput. The first direct run took 72.81 seconds; the other four took
3.67 to 4.61 seconds each.

## CPU results

The `cstk` process percentage uses a one-core scale. CPU ticks per file compare
CPU work for the same payload and are less affected by transfer duration.

### Near AP

| Proxy mode | Average `cstk` CPU | CPU ticks/file |
| --- | ---: | ---: |
| Buffered | 25.324% | 45.2 |
| Fixed splice | 22.021% | 37.4 |

Fixed splice used 13.0% less process CPU and 17.3% fewer CPU ticks per file.

### Farther location

| Proxy mode | Average `cstk` CPU | CPU ticks/file |
| --- | ---: | ---: |
| Buffered | 5.986% | 40.2 |
| Fixed splice | 4.389% | 22.0 |

Fixed splice used 26.7% less process CPU and 45.3% fewer CPU ticks per file.
These results show that the corrected splice path improves proxy efficiency in
both radio conditions.

## Interpretation

The measured behavior matches the intended role of a TCP-splitting AP proxy:

- On a strong, fast wireless link, direct forwarding wins because proxy
  overhead dominates.
- On the tested weak-signal link, direct TCP achieved higher normal speed but
  suffered one very slow run.
- Fixed-splice `costk` traded some peak speed for much more consistent delivery,
  resulting in the shortest total completion time and highest aggregate
  throughput across the five-file workload.
- Zero-copy splice reduced CPU work compared with the buffered relay.

The strongest supported conclusion is:

> Under the tested -70 dBm Wi-Fi condition, corrected-splice costk substantially
> improved effective multi-download throughput and did not exhibit the
> slow-tail behavior observed in direct forwarding, while using less proxy CPU
> than the buffered implementation.

## Limitations and next validation

The following limitations prevent treating this result as universal proof:

- Five runs per mode provide limited statistical confidence.
- A public CDN can change route, cache, server, or congestion between requests.
- Only one client, AP, file, and weak-signal position were tested.
- Packet loss, Wi-Fi retries, TCP retransmissions, RTT, and congestion-window
  history were not captured per run.
- The custom `wtcp` congestion-control algorithm was unavailable.

For product-level validation, repeat at least 30 randomized runs per mode using
a controlled wired server, several fixed RSSI levels, and both download and
upload directions. Record Wi-Fi retry counters, TCP retransmissions, RTT, CPU,
and per-run completion time. Report aggregate throughput, median, 95th-percentile
completion time, and confidence intervals.

## Raw data

- Near-AP report:
  `benchmark-results/wget-no-timeout-20260718-232407/report.md`
- Near-AP measurements:
  `benchmark-results/wget-no-timeout-20260718-232407/runs.csv`
- Farther-location report:
  `benchmark-results/wget-far-no-timeout-20260719-204023/report.md`
- Farther-location measurements:
  `benchmark-results/wget-far-no-timeout-20260719-204023/runs.csv`
- Farther-location radio samples:
  `benchmark-results/wget-far-no-timeout-20260719-204023/radio.txt`
