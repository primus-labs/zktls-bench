
## Build

```sh
npm install
npm run download:zk-files
npm run build
npm run build:browser
```

## (Optional) Use manually:

```sh
node lib/start_attestor [port]
```

```sh
node lib/start_prover [ip] [port] [zk-engine] [request-length] [response-length] [tls-version]
```

- set zk-engine `gnark` for native and `snarkjs` for wasm.
- tls-version: TLS1_2, TLS1_3(default) 


## Use scripts (native):

```sh
# party ip port interface
./bench.sh 1 127.0.0.1 12345 lo
./bench.sh 2 127.0.0.1 12345 lo
```

## Use scripts (wasm):

There are two methods of testing.

- 1. chrome-headless

For wasm test, need start a simple http server for loading wasm file in the prover side by running:

```sh
cd ./browser
python -m http.server
```

And then,

```sh
# party ip port interface TLS1_2/TLS1_3 wasm
# ./bench.sh 1 127.0.0.1 12345 lo TLS1_2 wasm
./bench.sh 1 127.0.0.1 12345 lo TLS1_3 wasm
./bench.sh 2 127.0.0.1 12345 lo
```


- 2. use snarkjs. (Optional)

It's the same as `chrome-headless` but no need start http server for loading wasm file. By runing:

```sh
# party ip port interface TLS1_2/TLS1_3 snarkjs
# ./bench.sh 1 127.0.0.1 12345 lo TLS1_2 snarkjs
./bench.sh 1 127.0.0.1 12345 lo TLS1_3 snarkjs
./bench.sh 2 127.0.0.1 12345 lo
```

(In real-world test runs, the performance of the two is basically the same.)


## Appendix

The algorithm of generate proof:
- tls1.2 use aes-128-ctr
- tls1.3 use aes-256-ctr

<br/>

Reclaim has two fields:
- The `responseRedactions` defines what needs to be redacted and proved. (If this is empty, i.e. no proof is done, performance will be fast. For our tests, we need to set this field)
- The `responseMatches` just matches the contents redacted by `responseRedactions` (if `responseRedactions` is empty, then will be matched from the entire response), and doesn't do proof.
