#!/bin/bash
set -e

cd ../3rd/tlsn/crates/benches

export RUSTFLAGS="--cfg aes_armv8"

cd browser/wasm
rustup run nightly wasm-pack build --release --target web
cd ../../binary
cargo build --release --features browser-bench
