#! /bin/bash
party=$1
ip=$2
port=$3
interface=$4

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
for rate in ${bandwithSet[@]}
do
    for delay in ${latencySet[@]}
    do
        ./simulate_network.sh $interface $rate $delay
        ../../../target/release/${party} &
        wait
        ./reset_network.sh $interface
        sleep 3
    done
done
