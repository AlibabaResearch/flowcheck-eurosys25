#!/bin/bash

set -e 

echo "Building Dump Module"
cd ./Packet_Dump/dma_dump/
make
echo "Dump Module Built"

cd -

echo "Building Parser Module"
cd ./Packet_Parser/cpp_decode_inbound
make
cd -
cd ./Packet_Parser/cpp_decode_outbound
make
echo "Parser Module Built"

cd -

echo "Building Extractor Module"
cd ./Data_Extractor
make
echo "Extractor Module Built"

cd -
echo "Building Updator Module"
cd ./Cython_Updater
sh build.sh
echo "Updator Module Built"

echo "Building Done."