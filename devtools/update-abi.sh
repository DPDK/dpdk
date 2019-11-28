#!/bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

abi_version=$1
abi_version_file="./ABI_VERSION"
update_path="lib drivers"

# check ABI version format string
check_abi_version() {
      echo $1 | grep -q -e "^[[:digit:]]\{1,2\}\.[[:digit:]]\{1,2\}$"
}

if [ -z "$1" ]; then
      # output to stderr
      >&2 echo "Please provide ABI version"
      exit 1
fi

# check version string format
if ! check_abi_version $abi_version ; then
      # output to stderr
      >&2 echo "ABI version must be formatted as MAJOR.MINOR version"
      exit 1
fi

if [ -n "$2" ]; then
      abi_version_file=$2
fi

if [ -n "$3" ]; then
      # drop $1 and $2
      shift 2
      # assign all other arguments as update paths
      update_path=$@
fi

echo "New ABI version:" $abi_version
echo "ABI_VERSION path:" $abi_version_file
echo "Path to update:" $update_path

echo $abi_version > $abi_version_file

find $update_path -name  \*version.map -exec \
      devtools/update_version_map_abi.py {} \
      $abi_version \; -print
