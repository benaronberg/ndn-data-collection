#!/bin/bash


while true
do
  ./getUIUC.py >& getUIUC.log 
  DATE=`date +%Y.%b.%d.%H.%M`
  mv getUIUC.log getUIUC.log.$DATE
  gzip getUIUC.log.$DATE
done
