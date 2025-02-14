#!/bin/bash
export RUSTFLAGS="--cfg aes_armv8"
cargo build --release
