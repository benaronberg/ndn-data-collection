#!/bin/bash


while true
do
  ./getWASH.py >& getWASH.log 
  DATE=`date +%Y.%b.%d.%H.%M`
  mv getWASH.log getWASH.log.$DATE
  gzip getWASH.log.$DATE
done
