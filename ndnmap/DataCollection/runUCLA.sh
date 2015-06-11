#!/bin/bash


while true
do
  ./getUCLA.py >& getUCLA.log 
  DATE=`date +%Y.%b.%d.%H.%M`
  mv getUCLA.log getUCLA.log.$DATE
  gzip getUCLA.log.$DATE
done
