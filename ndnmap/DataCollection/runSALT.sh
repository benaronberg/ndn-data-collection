#!/bin/bash


while true
do
  ./getSALT.py >& getSALT.log 
  DATE=`date +%Y.%b.%d.%H.%M`
  mv getSALT.log getSALT.log.$DATE
  gzip getSALT.log.$DATE
done
