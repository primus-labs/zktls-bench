#!/bin/bash
if [ $# -lt 4 ]; then
  echo "Usage: $0 \${party} \${ip} \${port} \${interace} [\${tls}] [\${kind}]"
  exit 1
fi
party=$1
ip=$2
port=$3
interface=$4
tls=TLS1_3
if [ $# -ge 5 ]; then
  tls=$5
fi
if [ "$tls" != "TLS1_2" ] && [ "$tls" != "TLS1_3" ]; then
  echo "only support tls: TLS1_2 TLS1_3"
  exit 1
fi
kind=native
if [ $# -ge 6 ]; then
  kind=$6
fi
if [ "$kind" != "native" ] && [ "$kind" != "wasm" ] && [ "$kind" != "snarkjs" ]; then
  echo "only support tls: native wasm snarkjs"
  exit 1
fi

# configurations
rates=(50 100 200)
delays=(10 15 20)
requests=(1024 2048)
responses=(16 256 1024 2048)

sudo tc qdisc del dev $interface root >/dev/null 2>&1

mkdir -p logs
for rate in ${rates[@]}; do
  for delay in ${delays[@]}; do
    if [ "$party" = "1" ]; then
      delay2=$((delay * 2))
      sudo tc qdisc add dev $interface root netem rate ${rate}mbit delay ${delay2}ms
    fi
    for request in ${requests[@]}; do
      for response in ${responses[@]}; do
        if [ "$party" = "2" ]; then
          node lib/start_attestor $port >logs/2.log 2>&1
        elif [ "$party" = "1" ]; then
          echo "kind: $kind rate: $rate delay: $delay request: $request response: $response"
          sudo iptables -D INPUT -p tcp --dport $port -j ACCEPT 2>/dev/null
          sudo iptables -D OUTPUT -p tcp --sport $port -j ACCEPT 2>/dev/null
          sudo iptables -I INPUT -p tcp --dport $port -j ACCEPT
          sudo iptables -I OUTPUT -p tcp --sport $port -j ACCEPT

          logfile=logs/$party-$kind-$rate-$delay-$request-$response-$tls.log
          zkengine='gnark'
          if [ "$kind" = "wasm" ]; then
            zkengine='snarkjs'
            node bench.js xx xx $ip $port $request $response $tls >$logfile 2>&1
          elif [ "$kind" = "snarkjs" ]; then
            zkengine='snarkjs'
            node lib/start_prover $ip $port $zkengine $request $response $tls >$logfile 2>&1
          else
            node lib/start_prover $ip $port $zkengine $request $response $tls >$logfile 2>&1
          fi

          res=$(cat $logfile | grep DONE:)
          cost=$(echo $res | awk -F'[:,"}]' '{for (i=1;i<NF;i++){if($i=="cost") {print $(i+2)}}}')
          memory=$(echo $res | awk -F'[:,"}]' '{for (i=1;i<NF;i++){if($i=="memory") {print $(i+2)}}}')
          if [ "$kind" = "wasm" ]; then
            memory=$(cat $logfile | grep memStat: | awk -F'[:,"}]' '{for (i=1;i<NF;i++){if($i=="totalJSHeapSize") {print int($(i+2)/1024)}}}')
          fi
          send_bytes=$(sudo iptables -L OUTPUT -v -n | grep ":${port}" | awk '{print $2}')
          recv_bytes=$(sudo iptables -L INPUT -v -n | grep ":${port}" | awk '{print $2}')
          # sudo iptables -D INPUT -p tcp --dport $port -j ACCEPT 2>/dev/null
          # sudo iptables -D OUTPUT -p tcp --sport $port -j ACCEPT 2>/dev/null

          resfile=result-$party.csv
          if [ ! -f "$resfile" ]; then
            echo "kind,name,bandwith(Mbps),latency(ms),request_size(B),response_size(B),send_bytes(B),recv_bytes(B),cost(ms),memory(KB)" >$resfile
          fi
          echo "$kind,reclaim,$rate,$delay,$request,$response,$send_bytes,$recv_bytes,$cost,$memory" >>$resfile
        fi
        # sleep 2
      done
    done
    if [ "$party" = "1" ]; then
      sudo tc qdisc del dev $interface root >/dev/null 2>&1
    fi
    # sleep 1
  done
done
