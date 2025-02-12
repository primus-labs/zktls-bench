
## Build

```sh
npm install
npm run download:zk-file
npm run build
```

## (Optional) Use manually:

```sh
node lib/start_attestor [port]
```

```sh
node lib/start_prover [ip] [port] [zk-engine] [request-length] [response-length]
```

Set zk-engine `gnark` for native and `snarkjs` for wasm.


## Use scripts (native):

```sh
# party ip port interface
./bench.sh 1 127.0.0.1 12345 lo
./bench.sh 2 127.0.0.1 12345 lo
```

## Use scripts (wasm):

```sh
# party ip port interface wasm
./bench.sh 1 127.0.0.1 12345 lo wasm
./bench.sh 2 127.0.0.1 12345 lo
```
