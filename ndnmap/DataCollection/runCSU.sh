#!/bin/bash


while true
do
  ./getCSU.py >& getCSU.log 
  DATE=`date +%Y.%b.%d.%H.%M`
  mv getCSU.log getCSU.log.$DATE
  gzip getCSU.log.$DATE
done
