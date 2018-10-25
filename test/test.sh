#!/bin/bash

COUNT=0
ID=$1

while [ $COUNT -le 20 ]
do
  echo "$ID - count $COUNT"
  COUNT=$(expr $COUNT + 1)
  sleep 1
done
