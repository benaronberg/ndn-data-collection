#!/bin/bash


while true
do
  ./getHOUS.py >& getHOUS.log 
  DATE=`date +%Y.%b.%d.%H.%M`
  mv getHOUS.log getHOUS.log.$DATE
  gzip getHOUS.log.$DATE
done
