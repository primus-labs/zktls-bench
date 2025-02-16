# primus(otls)-bench

## Native(cpp)

### Compile


```bash
./scripts/x_compile.sh cpp
```

### Configure

The path of the configuration file is [./config/config](./config/config). All fields are seperated by `:`. The meanings of the fields are `bandwith(Mbps)`, `delay(ms)`, `request_size(Byte)`, `response_size(Byte)` in order.

### bench

For `mpc-tls` model, there are two bench programs, namely `test_protocol` and `test_prot_on_off`. The first program disable online-offline while the second program enable online-offline. You can run `./scripts/bench.sh` to start bench.

**Note**: To simulate network, you should run `tc` command as `ROOT`.

```bash
./scripts/bench.sh ${program} ${party} ${ip} ${port} ${interface}
```

For example, benching on localhost, you can start `test_protocol` on the prover side by running:
```bash
./scripts/bench.sh test_protocol 1 127.0.0.1 12345 lo
```

On the verifier side, you can start by running:
```bash
./scripts/bench.sh test_protocol 2 127.0.0.1 12345 lo
```

For `proxy-tls` model, the bench program is `test_prove_proxy_tls`. The same way as the `mpc-tls` model.


### Result

Bench result is outputed in `result-${party}.csv`. The columns are `kind`,`name`,`bandwith(Mbps)`,`latency(ms)`,`request_size(B)`,`response_size(B)`,`send_bytes(B)`,`recv_bytes(B)`,`cost(ms)`,memory(KB).


## WASM

(Optional) You should first install chrome if you plan to run on a server (NOT DESKTOP) by runing: (for example on ubuntu server)

```sh
wget https://dl.google.com/linux/direct/google-chrome-stable_current_amd64.deb
sudo apt-get install -f
sudo dpkg -i google-chrome-stable_current_amd64.deb
```


### Compile

Reference [emsdk](https://emscripten.org/docs/getting_started/downloads.html) to install emcc.

Compile the project by running:

```bash
./scripts/x_compile.sh wasm
```

Go to  `./browser/wasm` and run: (need nodejs)

```sh
npm install
```

### Configure

The `Configuration` is the same as native.

### bench

Before the wasm test, it is necessary to compile the c++ version (since it is used by the verifier) and then start a simple http server to load and only load the wasm file.

```sh
bash ./scripts/start_http_server.sh
```


<br/>
Next, it's similar to the native test.

The verifier side is the same as native bench, and the prover side is similar but simply add `wasm` to the end of the command line arguments:

```bash
./scripts/bench.sh ${program} 1 ${ip} ${port} ${interface} wasm
```


### Result

The `Result` is the same as native.

