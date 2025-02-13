#!/bin/bash
interface=$1
sudo tc qdisc del dev $interface root >/dev/null
sudo tc qdisc show dev $interface
