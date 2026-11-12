#!/usr/bin/env bash
# Generate the self-signed TLS certificates used by the local mock servers.
#
# These files (and their private keys) are intentionally NOT committed — see
# reclaim/.gitignore. Run this once after cloning, before starting
# `https_server.py` or `src/mock-provider-server.ts`.
set -euo pipefail
cd "$(dirname "$0")"

# Used by https_server.py (certfile="cert.pem", keyfile="key.pem")
openssl req -x509 -newkey rsa:4096 -nodes \
  -out cert.pem -keyout key.pem -days 365 \
  -subj "/CN=localhost"

# Used by src/mock-provider-server.ts (./cert/private-key.pem + ./cert/public-cert.pem)
mkdir -p cert
openssl req -x509 -newkey rsa:2048 -nodes \
  -out cert/public-cert.pem -keyout cert/private-key.pem -days 365 \
  -subj "/CN=localhost"

echo "Generated cert.pem, key.pem, cert/public-cert.pem, cert/private-key.pem"
