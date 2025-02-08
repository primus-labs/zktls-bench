# zktls-bench
## CPP
### Compile
Compile the project by running:
```bash
./scripts/x_compile.sh cpp
```

### configure 
The path of the configuration file is `./config/config`. All fields are seperated by `:`. From the left to the right, the meaning of the fields are `bandwith(Mbps)`, `delay(ms)`, `request_size(Bytes)`, `response_size(Bytes)`.

### bench
For mpc-tls model, there are two bench programs, namely `test_protocol` and `test_prot_on_off`. The first program disable online-offline while the second program enable online-offline. You can run `./scripts/bench.sh` to start bench.
```bash
./scripts/bench.sh ${program} ${party} ${ip} ${port} ${interface}
```
For example, you can start `test_protocol` on the prover side by running:
```bash
./scripts/bench.sh test_protocol 1 127.0.0.1 12345 lo
```
On the verifier side, you can start by running:
```bash
./scripts/bench.sh test_protocol 2 127.0.0.1 12345 lo
```

For proxy-tls model, the bench program is `test_prove_proxy_tls`. You can run `./scripts/bench.sh` to start bench in the same way as the mpc-tls model.

**Note**: To simulate network, you should run `tc` command as `ROOT`.
### bench result
Bench result is outputed in `output_${program}_${party}.csv`. From the left to right, the fields are `bandwith(Mbps)`, `latency(ms)`, `request_size(Bytes)`, `response_size(Bytes)`, `send bytes(KBytes)`, `total cost time(ms)`.


## WASM
### Compile
Compile the project by running:
```bash
./scripts/x_compile.sh wasm
```


