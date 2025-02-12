#!/bin/bash
#!/bin/bash
if [ $# -lt 4 ]; then
  echo "Usage: $0 \${party} \${ip} \${port} \${interace} [\${kind}]"
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

# configurations
delays=(20 50)
rates=(20 50)
requests=(1024 2048)
responses=(1024 2048)

mkdir -p logs
for delay in ${delays[@]}; do
  for rate in ${rates[@]}; do
    sudo tc qdisc add dev $interface root netem rate ${rate}mbit delay ${delay}ms
    for request in ${requests[@]}; do
      for response in ${responses[@]}; do
        echo "kind: $kind rate: $rate delay: $delay request: $request response: $response"
        if [ "$party" = "2" ]; then
          node lib/start_attestor $port
        elif [ "$party" = "1" ]; then
          zkengine='gnark'
          if [ "$kind" = "wasm" ]; then
            zkengine='snarkjs'
          fi
          sudo iptables -D INPUT -p tcp --dport $port -j ACCEPT 2>/dev/null
          sudo iptables -D OUTPUT -p tcp --sport $port -j ACCEPT 2>/dev/null
          sudo iptables -I INPUT -p tcp --dport $port -j ACCEPT
          sudo iptables -I OUTPUT -p tcp --sport $port -j ACCEPT

          logfile=logs/$kind-$rate-$delay-$request-$response.log
          node lib/start_prover $ip $port $zkengine $request $response >$logfile 2>/dev/null
          res=$(cat $logfile | grep DONE:)
          cost=$(echo $res | awk -F'[:,"}]' '{for (i=1;i<NF;i++){if($i=="cost") {print $(i+2)}}}')
          memory=$(echo $res | awk -F'[:,"}]' '{for (i=1;i<NF;i++){if($i=="memory") {print $(i+2)}}}')
          send_bytes=12345
          recv_bytes=12345

          send_bytes=$(sudo iptables -L OUTPUT -v -n | grep ":${port}" | awk '{print int($2/1024)}')
          recv_bytes=$(sudo iptables -L INPUT -v -n | grep ":${port}" | awk '{print int($2/1024)}')
          sudo iptables -D INPUT -p tcp --dport $port -j ACCEPT 2>/dev/null
          sudo iptables -D OUTPUT -p tcp --sport $port -j ACCEPT 2>/dev/null

          resfile=result-$party.csv
          if [ ! -f "$resfile" ]; then
            echo "kind,name,bandwith(Mbps),latency(ms),request_size(B),response_size(B),send_bytes(B),recv_bytes(B),cost(ms),memory(KB)" >$resfile
          fi
          echo "$kind,reclaim,$rate,$delay,$request,$response,$send_bytes,$recv_bytes,$cost,$memory" >>$resfile
        fi
        # sleep 2
      done
    done
    sudo tc qdisc del dev $interface root >/dev/null 2>&1
    # sleep 1
  done
done
