#!/bin/bash

cd $(dirname $0)

[ -d build ] && rm -rf build

./deploy.sh
