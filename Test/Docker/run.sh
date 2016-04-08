#!/usr/bin/env bash

script_dir="`cd $(dirname $0); pwd`"

data_container=$(docker ps --filter 'name=dream3d-data' -q -a | head -n 1)
if test -z "$data_container"; then
  data_container=$(docker create --name dream3d-data dream3d/data)
fi

docker run \
  --rm \
  --volumes-from $data_container \
  -v $script_dir/../..:/usr/src/DREAM3D/DREAM3D/Source/Plugins/ITKImageProcessing \
  dream3d/dream3d \
    /usr/src/DREAM3D/DREAM3D/Source/Plugins/ITKImageProcessing/Test/Docker/test.sh
