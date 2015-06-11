#!/bin/sh

./runCSU.sh >2 runCSU.log &
./runHOUS.sh >2 runHOUS.log &
./runKANS.sh >2 runKANS.log &
./runRemap.sh >2 runRemap.log &
./runSALT.sh >2 runSALT.log &
./runUCLA.sh >2 runUCLA.log &
./runUCSD.sh >2 runUCSD.log &
./runUIUC.sh >2 runUIUC.log &
./runUM.sh >2 runUM.log &
./runWASH.sh >2 runWASH.log &
./runWashU.sh >2 runWashU.log &
