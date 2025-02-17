# reclaim-bench


## Dependencies

- NodeJS: 18+.
- Python: 3.7+. Load wasm files for browser testing only.
- Chrome: For browser testing. You should [install chrome](../primus/INSTALL_CHROME.md) first if you plan to run WASM test on a server (**NOT DESKTOP**).


## Build

```sh
npm install
npm run download:zk-files
npm run build
npm run build:browser
```

## Script

```sh
./bench.sh party ip port interface tls kind
```

- party: 1 or 2 for the Prover or the Verifier, respectively.
- ip: The Verifier's IP.
- port: The Verifier's PORT.
- interface: The gateway interface for communication.
- tls: (Only for the Prover side) TLS1_3(default) or TLS1_2.
- kind: (Only for the Prover side) native(default) or wasm.


## Run

### Native

```sh
./bench.sh 2 127.0.0.1 12345 lo
./bench.sh 1 127.0.0.1 12345 lo
```

### Wasm

For wasm test, need start a simple http server for loading wasm files in the Prover side by running:

```sh
cd ./browser
python -m http.server
```

(Optional) You should [install chrome](../primus/INSTALL_CHROME.md) first if you plan to run WASM test on a server (**NOT DESKTOP**).

```sh
./bench.sh 2 127.0.0.1 12345 lo
./bench.sh 1 127.0.0.1 12345 lo [TLS1_3/TLS1_2] wasm
```

## Result

The results will be output to `result-1.csv` in the Prover side.


## Appendix


**The algorithm of generate proof:**
- tls1.2 use aes-128-ctr
- tls1.3 use aes-256-ctr

<br/>

**Reclaim has two fields:**
- The `responseRedactions` defines what needs to be redacted and proved. (If this is empty, i.e. no proof is done, performance will be fast. For our tests, we need to set this field)
- The `responseMatches` just matches the contents redacted by `responseRedactions` (if `responseRedactions` is empty, then will be matched from the entire response), and doesn't do proof.


<br/>

**Additional note on data traffic:**
It is possible that some machines do not fully support iptables to work, and it is possible to run traffic data on a supported machine, e.g. locally. Since latency and bandwidth do not affect data traffic volume, only different request size, response size, type (local/wasm) and tls (1.2/1.3) parameters are needed.
