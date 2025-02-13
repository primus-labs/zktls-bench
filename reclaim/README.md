
## Build

```sh
npm install
npm run download:zk-file
npm run build
npm run build:browser
```

## Preparation

Start a mock-localhost data server in the attestor side by:


```sh
node lib/start_provider [port]
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

There are two methods of testing.

- 1. chrome-headless

For wasm test, need start a simple http server for load wasm file by:

```sh
cd ./browser
python -m http.server
```

And then,

```sh
# party ip port interface wasm
./bench.sh 1 127.0.0.1 12345 lo wasm
./bench.sh 2 127.0.0.1 12345 lo
```


- 2. use snarkjs. (Optional)

It's the same as `chrome-headless` but no need start http server for loading wasm file. By runing:

```sh
# party ip port interface snarkjs
./bench.sh 1 127.0.0.1 12345 lo snarkjs
./bench.sh 2 127.0.0.1 12345 lo
```

(In real-world test runs, the performance of the two is basically the same.)
