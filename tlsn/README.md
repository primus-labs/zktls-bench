# tlsn-bench

## Dependencies

- Rust.
- Chrome: For browser testing. You should [install chrome](../primus/INSTALL_CHROME.md) first if you plan to run WASM test on a server (**NOT DESKTOP**).


## Patch Code

The official bench is a local, single-computer one. In order to facilitate multi-computer testing and better control of latency and bandwidth, simply patched the source code, which does not have any impact on the original code.

```bash
bash ./patch_code.sh
```

## Compile

### Native on x86


```bash
bash ./build_native_x86.sh
```

### Wasm on x86



```bash
bash ./build_wasm_x86.sh
```


### Native on arm


```bash
bash ./build_native_arm.sh
```

## Script

```sh
sudo ./bench.sh party ip port interface kind
```

- party: 1 or 2 for the Prover or the Verifier, respectively.
- ip: The Verifier's IP.
- port: The Verifier's PORT.
- interface: The gateway interface for communication.
- kind: native(default) or wasm.

## Run

(Optional) You should [install chrome](../primus/INSTALL_CHROME.md) first if you plan to run WASM test on a server (**NOT DESKTOP**).


```bash
sudo ./bench.sh 2 0.0.0.0 12345 lo [native|wasm]
sudo ./bench.sh 1 127.0.0.1 12345 lo [native|wasm]
```

## Result

The results will be output to `metrics.csv` in the Prover side.

The delay values in `bench.sh` are outgoing `netem` delays on each party's selected interface. With one shaped interface in each direction between separate hosts, a configured delay of `d` adds approximately `2 * d` to the RTT. See [the shared delay and RTT explanation](../bench_data/README.md#injected-delay-and-round-trip-time) when interpreting the benchmark data.

