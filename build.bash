#!/bin/bash

echo "Building cefore_custom"
cefnetdstop

# make clean
autoreconf
autoconf
automake
./configure --enable-cache --enable-csmgr
make 
sudo make install
sudo ldconfig

echo "Finished building cefore_custom"

cefnetdstart
cefstatus