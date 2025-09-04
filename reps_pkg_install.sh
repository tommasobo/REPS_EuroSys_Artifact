#!/bin/bash

sudo apt update
sudo apt install -y libgraphviz-dev
sudo apt install -y python3-pip
python3 -m pip install -r requirements.txt
