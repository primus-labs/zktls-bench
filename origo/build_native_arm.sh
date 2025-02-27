#!/bin/bash
set -e

cd ./3rd/web-prover

export RUSTFLAGS="--cfg aes_armv8"
cargo build --release
cargo build -p client --release
cargo build -p notary --release
