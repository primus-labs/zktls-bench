#!/bin/bash
interface=$1
sudo tc qdisc del dev $interface root
sudo tc qdisc show dev $interface 
