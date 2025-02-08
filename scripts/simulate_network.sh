#!/bin/bash
interface=$1
rate=$2
delay=$3
echo "$interface $rate $delay"
sudo tc qdisc add dev $interface root netem rate ${rate}mbit delay ${delay}ms
sudo tc qdisc show dev $interface 
