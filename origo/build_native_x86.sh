#!/bin/bash
set -e

cd ./3rd/web-prover

cargo build --release
cargo build -p client --release
cargo build -p notary --release
