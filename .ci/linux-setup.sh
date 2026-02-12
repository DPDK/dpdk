#!/bin/sh -xe

# Builds are run as root in containers, no need for sudo
[ "$(id -u)" != '0' ] || alias sudo=

# determine minimum Meson version
ci_dir=$(dirname $(readlink -f $0))
meson_ver=$(python3 $ci_dir/../buildtools/get-min-meson-version.py)

# need to install as 'root' since some of the unit tests won't run without it
sudo python3 -m pip install --upgrade "meson==$meson_ver"

# setup hugepages. error ignored because having hugepage is not mandatory.
cat /proc/meminfo
sudo sh -c 'echo 1024 > /proc/sys/vm/nr_hugepages' || true
cat /proc/meminfo
