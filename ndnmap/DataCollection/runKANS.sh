#!/bin/bash


while true
do
  ./getKANS.py >& getKANS.log 
  DATE=`date +%Y.%b.%d.%H.%M`
  mv getKANS.log getKANS.log.$DATE
  gzip getKANS.log.$DATE
done
