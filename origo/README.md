# origo-bench

## Dependencies

### Install Rust
```bash
curl https://sh.rustup.rs | sh
. "$HOME"/.cargo/env
rustup install nightly-2024-10-28
rustup component add rust-src --toolchain nightly-2024-10-28-x86_64-unknown-linux-gnu
```

### Install Dependency
```bash
sudo apt-get install libssl-dev
sudo apt-get install pkg-config
sudo apt-get install clang
sudo apt-get install protobuf-compiler
```
### Download Circuit Files
```bash
cd 3rd/web-prover
make artifacts
```

### Patch
```bash
bash ./patch_code.sh origo.patch
```
Or
```bash
bash ./patch_code.sh origo-memory.patch
```
**Note**: the first patch will not profiler memory usage. If you need to profiler memory usage, you should use the second patch file. 

## Compile

### Native on x86

```bash
bash ./build_native_x86.sh
```

### Native on arm

```bash
bash ./build_native_arm.sh
```

### Wasm on x86
```bash
bash ./build_wasm_x86.sh
```

## Native Bench(x86 or arm)

### Start Mock Server
```bash
bash mock_server.sh
```

### Start Notary Server
```bash
bash notary_server.sh
```

### Start Bench Client
```bash
bash bench.sh 38081 lo
```

## Wasm Bench

### Start Mock Server
```bash
bash mock_server.sh
```

### Start Notary Server
```bash
bash notary_server.sh
```

### Start Http Sever
```bash
bash http_server.sh
```

### Start Bench Client
```bash
bash bench.sh 38081 lo wasm
```
