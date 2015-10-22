#! /bin/echo must be loaded with .

# Load DPDK devel config and allow override
# from system file
test ! -r /etc/dpdk/devel.config ||
        . /etc/dpdk/devel.config
# from user file
test ! -r ~/.config/dpdk/devel.config ||
        . ~/.config/dpdk/devel.config
# from local file
test ! -r $(dirname $(readlink -m $0))/../.develconfig ||
        . $(dirname $(readlink -m $0))/../.develconfig

# The config files must export variables in the shell style
