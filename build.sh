#!/bin/bash

make debug || exit 1

sudo chown 0:0 ./bin/server
sudo chown 0:0 ./bin/client32
sudo chown 0:0 ./bin/client64

sudo chmod u+s ./bin/server
sudo chmod u+s ./bin/client32
sudo chmod u+s ./bin/client64
