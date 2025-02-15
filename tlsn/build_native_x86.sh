#!/bin/bash
set -e

cd ../3rd/tlsn/crates/benches

cd binary
cargo build --release
