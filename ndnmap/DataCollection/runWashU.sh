#!/bin/bash


while true
do
  ./getWashU.py >& getWashU.log 
  DATE=`date +%Y.%b.%d.%H.%M`
  mv getWashU.log getWashU.log.$DATE
  gzip getWashU.log.$DATE
done
