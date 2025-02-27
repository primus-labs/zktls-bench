#!/bin/bash
if [ $# -lt 2 ]; then
    echo "$0 \${port} \${interface}"
fi

bandwithSet=(50 100 200)
latencySet=(10 15 20)
reqSize=(1024 2048)
responseSize=(16 256 1024 2048)
port=$1
interface=$2
kind=native
if [ $# -ge 3 ]; then
    kind=$3
fi

if [ "$kind" != "wasm" && "$kind" != "native" ]; then
    echo "kind can only be native or wasm"
fi

curdir=$(pwd)

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
                sudo iptables -D INPUT -p tcp --sport $port -j ACCEPT 2>/dev/null
                sudo iptables -D OUTPUT -p tcp --dport $port -j ACCEPT 2>/dev/null
                sudo iptables -I INPUT -p tcp --sport $port -j ACCEPT
                sudo iptables -I OUTPUT -p tcp --dport $port -j ACCEPT
                memory=0

                if [ "$kind" == "native" ]; then
                    cargo run --release -p client -- --log-level WARN --config ./fixture/client.origo_${req}_${resp}.json > tmp.log
                    totalCost=$(grep totalCost tmp.log | awk -F' ' '{print $2}')
                    memoryStat=$(grep heapMaxBytes tmp.log)
                    if [ -n "$memoryStat" ]; then
                        memory=$(echo $memoryStat | awk -F' ' '{print int($2 / 1024)}')
                    fi
                elif [ "$kind" == "wasm" ]; then
                    cd ${curdir}/../fixture
                    rm client.origo_tcp_local.json
                    ln -s client.origo_${req}_${resp}.json client.origo_tcp_local.json
                    sleep 5
                    cd $curdir
                    node test.js > tmp.log
                    totalCost=$(cat tmp.log | grep "worker thread" | tail -n 1 | awk '{print $3}') 
                    memory=$(cat tmp.log | grep "WASM Memory Usage" | tail -n 1 | awk '{print $5}')
                fi

                send_bytes=$(sudo iptables -L OUTPUT -v -n | grep ":${port}" | awk '{print $2}')
                recv_bytes=$(sudo iptables -L INPUT -v -n | grep ":${port}" | awk '{print $2}')
                if [ ! -f result.log ]; then
                    echo "kind,name,bandwith(Mbps),latency(ms),request_size(B),response_size(B),send_bytes(B),recv_bytes(B),cost(ms),memory(KB)" > result.log
                fi
                echo "${kind},all,${rate},${delay},${req},${resp},${send_bytes},${recv_bytes},${totalCost},${memory}" >> result.log

            done
        done
        sudo tc qdisc del dev $interface root >/dev/null 2>&1
    done
done
