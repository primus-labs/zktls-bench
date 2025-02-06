#!/bin/bash

for line in `cat ./config/config`
do
	echo $line
	bandwith=`echo $line | awk -F: '{print $1}'`
	delay=`echo $line | awk -F: '{print $2}'`
	request_size=`echo $line | awk -F: '{print $3}'`
	response_size=`echo $line | awk -F: '{print $4}'`
	echo "$bandwith#$delay#$request_size#$response_size"
	tc qdisc add dev lo root netem delay ${delay}ms
	tc qdisc add dev lo root netem rate ${delay}mbit
	$1 $2 $3 $request_size $response_size
	tc qdisc del dev lo root netem delay ${delay}ms
	tc qdisc del dev lo root netem rate ${delay}mbit
done
