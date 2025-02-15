
Based on [bench_data#20e54d0](https://github.com/primus-labs/zktls-bench/tree/20e54d0).

## Data header

| kind | name | bandwith(Mbps) | latency(ms) | request_size(B) | response_size(B) | send_bytes(B) | recv_bytes(B) | cost(ms) | memory(KB) |
| ---- | ---- | -------------- | ----------- | --------------- | ---------------- | ------------- | ------------- | -------- | ---------- |

- kind: native or browser(wasm)
- name: description
- bandwith: 50/100/200 Mbps
- latency: 10/15/20 ms
- request_size: 1024/2048 Bytes
- response_size: 16/256/1024/2048 Bytes
- send_bytes: the amount of data the prover sends to the verifer
- recv_bytes: the amount of data the prover receives from the verifer
- cost: time required for a single attestation
- memory: memory

So, each file has 73 lines, one header line and 72 (3x3x2x4) lines of data.



## Machine Configuration



## Plotting


