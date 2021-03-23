#!/bin/sh -xe

# need to install as 'root' since some of the unit tests won't run without it
sudo python3 -m pip install --upgrade 'meson==0.49.2'

# setup hugepages. error ignored because having hugepage is not mandatory.
cat /proc/meminfo
sudo sh -c 'echo 1024 > /proc/sys/vm/nr_hugepages' || true
cat /proc/meminfo
