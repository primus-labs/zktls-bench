#!/bin/bash
if [ $# -lt 4 ]; then
  echo "Usage: $0 \${party} \${ip} \${port} \${interace}[\${kind}]"
  exit 1
fi
party=$1
ip=$2
port=$3
interface=$4
kind=native
if [ $# -ge 5 ]; then
  kind=$5
fi
if [ "$kind" != "native" ] && [ "$kind" != "wasm" ]; then
  echo "only support tls: native wasm"
  exit 1
fi

# Check if we are running as root.
if [ "$EUID" -ne 0 ]; then
  echo "This script must be run as root"
  exit
fi
export VERIFIER_IP=$ip
export VERIFIER_PORT=$port

bandwithSet=(50 100 200)
latencySet=(10 15 20)
./reset_network.sh $interface
for rate in ${bandwithSet[@]}; do
  for delay in ${latencySet[@]}; do
    ./simulate_network.sh $interface $rate $delay
    if [ "$kind" = "wasm" ]; then
      ./3rd/tlsn/target/release/${party}
    else
      ./3rd/tlsn/target/release/${party}-memory
    fi
    ./reset_network.sh $interface
    sleep 3
    if [ "$party" = "prover" ]; then
      sed -i "s/0,0,0,0,/${rate},${delay},${rate},${delay},/" metrics.csv
    fi
  done
done
