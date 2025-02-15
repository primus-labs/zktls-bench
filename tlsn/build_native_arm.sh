#!/bin/bash
set -e

cd ../3rd/tlsn/crates/benches

export RUSTFLAGS="--cfg aes_armv8"
cd binary
cargo build --release
