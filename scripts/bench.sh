#!/bin/bash
interface=$5
if [ $# -ne 5 ]; then
    echo "Usage: $0 \${program} \${party} \${ip} \${port} \${interace}"
    exit 1
fi

for line in `cat ./config/config`
do
    echo $line
    bandwith=`echo $line | awk -F: '{print $1}'`
    delay=`echo $line | awk -F: '{print $2}'`
    request_size=`echo $line | awk -F: '{print $3}'`
    response_size=`echo $line | awk -F: '{print $4}'`
    echo "$bandwith#$delay#$request_size#$response_size"
    ./scripts/simulate_network.sh $interface $bandwith $delay

    ./build/cpp/bench/bin/zktls_bench $1 $2 $3 $4 $request_size $response_size $bandwith $delay
	./scripts/reset_network.sh $interface
    sleep 5
done
