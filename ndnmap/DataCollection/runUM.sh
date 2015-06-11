#!/bin/bash


while true
do
  ./getUM.py >& getUM.log 
  DATE=`date +%Y.%b.%d.%H.%M`
  mv getUM.log getUM.log.$DATE
  gzip getUM.log.$DATE
done
