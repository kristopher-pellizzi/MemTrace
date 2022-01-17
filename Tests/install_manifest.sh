#!/bin/bash

IFS=$'\n'
install_list=$(cat focal-server-cloudimg-amd64-root.manifest | sed 's/\t/ /g' | tr -s ' ' | sed '/snap:/d' | sed 's/ /=/g') 

for line in $install_list 
do
        echo "Installing $line..."
        apt-get install -y $line
done
