#!/bin/bash
interface=$1
sudo tc qdisc del dev $interface root >/dev/nul
sudo tc qdisc show dev $interface
