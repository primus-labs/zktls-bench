#!/bin/bash

for line in `cat ./config/config`
do
	echo $line
	bandwith=`echo $line | awk -F: '{print $1}'`
	delay=`echo $line | awk -F: '{print $2}'`
	request_size=`echo $line | awk -F: '{print $3}'`
	response_size=`echo $line | awk -F: '{print $4}'`
	echo "$bandwith#$delay#$request_size#$response_size"
	$1 $2 $3 $request_size $response_size
done
