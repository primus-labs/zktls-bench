#!/bin/bash
set -e

cd ./3rd/tlsn/crates/benches

cd browser/wasm
rustup run nightly wasm-pack build --release --target web
cd ../../binary
cargo build --release --features browser-bench
