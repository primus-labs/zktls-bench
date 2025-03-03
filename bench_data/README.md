
## Data header

| kind | name | bandwidth(Mbps) | latency(ms) | request_size(B) | response_size(B) | send_bytes(B) | recv_bytes(B) | cost(ms) | memory(KB) |
| ---- | ---- | --------------- | ----------- | --------------- | ---------------- | ------------- | ------------- | -------- | ---------- |

- kind: native or browser(wasm)
- name: description
- bandwidth: 50/100/200 Mbps
- latency: 10/15/20 ms
- request_size: 1024/2048 Bytes (0 for origo/reclaim)
- response_size: 16/256/1024/2048 Bytes
- send_bytes: the amount of data the prover sends to the verifier
- recv_bytes: the amount of data the prover receives from the verifier
- cost: time required for a single attestation
- memory: memory

So, each file has 73(37, for origo/reclaim) lines, one header line and 72 (3x3x2x4) or 36 (3x3x1x4) lines of data.



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

