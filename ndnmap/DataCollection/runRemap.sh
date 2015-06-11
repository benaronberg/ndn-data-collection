#!/bin/bash


while true
do
  ./getRemap.py >& getRemap.log 
  DATE=`date +%Y.%b.%d.%H.%M`
  mv getRemap.log getRemap.log.$DATE
  gzip getRemap.log.$DATE
done
