#!/bin/bash

git clone https://github.com/libfuse/libfuse
cd libfuse/
./makeconf.sh
./configure && make -j 8

