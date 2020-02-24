#!/bin/sh -xe

# need to install as 'root' since some of the unit tests won't run without it
sudo python3 -m pip install --upgrade 'meson==0.47.1'

# skip hugepage settings if tests will not run
if [ "$RUN_TESTS" = "1" ]; then
    # setup hugepages
    cat /proc/meminfo
    sudo sh -c 'echo 1024 > /proc/sys/vm/nr_hugepages'
    cat /proc/meminfo
fi
