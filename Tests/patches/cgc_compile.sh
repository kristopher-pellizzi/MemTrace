#! /usr/bin/env bash

script_path=$(readlink -f $(dirname $0))

cp ../patches/cgc_Makefile ./Makefile;
export PATH=$PATH:$script_path/bin;
make
