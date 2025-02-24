#!/bin/bash

bandwithSet=(50 100 200)
latencySet=(10 15 20)
reqSize=(1024 2048)
responseSize=(16 256 1024 2048)
interface=lo
port=38081

export RUSTFLAGS="--cfg aes_armv8"
sudo tc qdisc del dev $interface root >/dev/null 2>&1
for rate in ${bandwithSet[@]}
do
    for delay in ${latencySet[@]}
    do
        sudo tc qdisc add dev $interface root netem rate ${rate}mbit delay ${delay}ms
        sudo tc qdisc show dev $interface
        for req in ${reqSize[@]}
        do
            for resp in ${responseSize[@]}
            do
                sudo iptables -D INPUT -p tcp --dport $port -j ACCEPT 2>/dev/null
                sudo iptables -D OUTPUT -p tcp --sport $port -j ACCEPT 2>/dev/null
                sudo iptables -I INPUT -p tcp --dport $port -j ACCEPT
                sudo iptables -I OUTPUT -p tcp --sport $port -j ACCEPT
                cargo run --release -p client -- --log-level WARN --config ./fixture/client.origo_${req}_${resp}.json > tmp.log
                totalCost=$(grep totalCost tmp.log | awk -F' ' '{print $2}')
                send_bytes=$(sudo iptables -L OUTPUT -v -n | grep ":${port}" | awk '{print $2}')
                recv_bytes=$(sudo iptables -L INPUT -v -n | grep ":${port}" | awk '{print $2}')
                echo "${rate}#${delay}#${req}#${resp}#${send_bytes}#${recv_bytes}#${totalCost}" >> result.log
            done
        done
        sudo tc qdisc del dev $interface root >/dev/null 2>&1
    done
done
