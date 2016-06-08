#
# run ./config/firewall.sh
#

p 1 firewall add default 4 #SINK0
p 1 firewall add priority 1 ipv4 0.0.0.0 0 100.0.0.0 10 0 65535 0 65535 6 0xF port 0
p 1 firewall add priority 1 ipv4 0.0.0.0 0 100.64.0.0 10 0 65535 0 65535 6 0xF port 1
p 1 firewall add priority 1 ipv4 0.0.0.0 0 100.128.0.0 10 0 65535 0 65535 6 0xF port 2
p 1 firewall add priority 1 ipv4 0.0.0.0 0 100.192.0.0 10 0 65535 0 65535 6 0xF port 3

#p 1 firewall add bulk ./config/firewall.txt

p 1 firewall ls
