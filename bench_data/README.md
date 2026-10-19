
## Data header

| kind | name | bandwidth(Mbps) | latency(ms) | request_size(B) | response_size(B) | send_bytes(B) | recv_bytes(B) | cost(ms) | memory(KB) |
| ---- | ---- | --------------- | ----------- | --------------- | ---------------- | ------------- | ------------- | -------- | ---------- |

- kind: native or browser(wasm)
- name: description
- bandwidth: 50/100/200 Mbps
- latency: configured per-interface delay of 10/15/20 ms; see [Injected delay and round-trip time](#injected-delay-and-round-trip-time).
- request_size: 1024/2048 Bytes (0 for origo/reclaim)
- response_size: 16/256/1024/2048 Bytes
- send_bytes: the amount of data the prover sends to the verifier
- recv_bytes: the amount of data the prover receives from the verifier
- cost: time required for a single attestation
- memory: memory

So, each file has 73(37, for origo/reclaim) lines, one header line and 72 (3x3x2x4) or 36 (3x3x1x4) lines of data.



## Injected delay and round-trip time

The `latency(ms)` column records the configured `netem delay` value. It is a network-emulation setting, rather than a measured round-trip time (RTT). The [`tc-netem` manual](https://man7.org/linux/man-pages/man8/tc-netem.8.html) describes this delay as applying to packets going out on the selected interface.

The [Primus benchmark script](../primus/scripts/bench.sh) and [TLSNotary benchmark script](../tlsn/bench.sh) configure this delay on each party's selected interface. For a prover and verifier on separate hosts, with each direction passing through one interface configured with delay `d`, the added RTT is approximately `d + d = 2 * d`:

| Configured delay / legacy `latency(ms)` | Approximate added RTT with both directions shaped |
| --- | --- |
| 10 ms | 20 ms |
| 15 ms | 30 ms |
| 20 ms | 40 ms |

Measured RTT also includes the underlying network's RTT and effects such as rate limiting, queueing, and scheduling. The `cost(ms)` column measures the whole attestation, which may include many protocol round trips and computation.

The `2 * d` conversion depends on the traffic path. [Reclaim](../reclaim/bench.sh) installs its qdisc only in the prover branch, while [Origo](../origo/bench.sh) configures one selected interface from its client script. A shared loopback interface can also carry both directions through the same qdisc. Check which packet directions traverse the configured interface before converting a row to an RTT estimate or comparing harnesses.

Existing CSV files retain their recorded headers and values. Interpret the legacy `latency(ms)` field as the injected per-interface delay; use a separate RTT measurement when the actual RTT is needed.


## Machine Configuration
### X86
- Cloud Machine
  - CPU Platform: Intel Cascade Lake
  - Machine Type: c2-standard-8
  - Architecture: x86/64
- OS: Ubuntu-22.04
- CPU: 8 vCPUs, Intel(R) Xeon(R) CPU@ 3.10GHz
- Memory: 32GB

### ARM
- Cloud Machine
  - CPU Platform: Ampere Altra
  - Machine Type: t2a-standard-8
  - Architecture: arm64
- OS: Ubuntu-22.04
- CPU: 8 vCPUs, 3.1GHz
- Memory: 32GB

