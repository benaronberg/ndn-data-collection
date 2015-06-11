#!/bin/bash


while true
do
  ./getUCSD.py >& getUCSD.log 
  DATE=`date +%Y.%b.%d.%H.%M`
  mv getUCSD.log getUCSD.log.$DATE
  gzip getUCSD.log.$DATE
done
