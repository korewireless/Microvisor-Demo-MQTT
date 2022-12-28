#!/bin/bash
set -e

cd $(dirname $0)

#DEPLOY_OPTS="${DEPLOY_OPTS} --buildonly"

#[ -d build ] && rm -rf build

./deploy.sh ${DEPLOY_OPTS}
