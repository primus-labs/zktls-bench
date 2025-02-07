# zktls-bench
### Compile
Compile the project by running:
```bash
./scripts/x_compile.sh cpp
```

### configure 
The path of the configuration file is `./config/config`. All fields are seperated by `:`. From the left to the right, the meaning of the fields are `bandwith(Mbps)`, `delay(ms)`, `request_size(Bytes)`, `response_size(Bytes)`.

### bench
For mpc-tls model, there are two bench programs, namely `./build/cpp/bin/test_protocol` and `./build/cpp/bin/test_prot_on_off`. The first program disable online-offline while the second program enable online-offline. You can run `./scripts/bench.sh` to start bench.
```bash
./scripts/bench.sh ${program} ${party} ${ip} ${port} ${interface}
```
For example, you can start `test_protocol` on the prover side by running:
```bash
./scripts/bench.sh ./build/cpp/bench/bin/test_protocol 1 127.0.0.1 12345 lo
```
On the verifier side, you can start by running:
```bash
./scripts/bench.sh ./build/cpp/bench/bin/test_protocol 2 127.0.0.1 12345 lo
```

For proxy-tls model, the bench program is `./build/cpp/bin/test_prove_proxy_tls`. You can run `./scripts/bench.sh` to start bench in the same way as the mpc-tls model.
### bench result
Bench result is outputed in `output_${party}.csv`. From the left to right, the fields are `request_size`, `response_size`, `send bytes`, `total cost time`.
