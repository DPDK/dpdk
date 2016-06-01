#
# run ./config/network_layers.sh
#

################################################################################
# Link configuration
################################################################################
# Routes added implicitly when links are brought UP:
# IP Prefix = 10.0.0.1/16 => (Port 0, Local)
# IP Prefix = 10.0.0.1/32 => (Port 4, Local)
# IP Prefix = 10.1.0.1/16 => (Port 1, Local)
# IP Prefix = 10.1.0.1/32 => (Port 4, Local)
# IP Prefix = 10.2.0.1/16 => (Port 2, Local)
# IP Prefix = 10.2.0.1/32 => (Port 4, Local)
# IP Prefix = 10.3.0.1/16 => (Port 3, Local)
# IP Prefix = 10.3.0.1/32 => (Port 4, Local)
link 0 down
link 1 down
link 2 down
link 3 down
link 0 config 10.0.0.1 16
link 1 config 10.1.0.1 16
link 2 config 10.2.0.1 16
link 3 config 10.3.0.1 16
link 0 up
link 1 up
link 2 up
link 3 up
#link ls

################################################################################
# Static ARP
################################################################################
p 1 arp add default 5 #SINK3
p 1 arp add 0 10.0.0.2 a0:b0:c0:d0:e0:f0
p 1 arp add 1 10.1.0.2 a1:b1:c1:d1:e1:f1
p 1 arp add 2 10.2.0.2 a2:b2:c2:d2:e2:f2
p 1 arp add 3 10.3.0.2 a3:b3:c3:d3:e3:f3
#p 1 arp ls

################################################################################
# Routes
################################################################################
p 1 route add default 4 #SINK2
p 1 route add 100.0.0.0 16 port 0 ether 10.0.0.2
p 1 route add 100.1.0.0 16 port 1 ether 10.1.0.2
p 1 route add 100.2.0.0 16 port 2 ether 10.2.0.2
p 1 route add 100.3.0.0 16 port 3 ether 10.3.0.2
#p 1 route ls

################################################################################
# Local destination UDP traffic
################################################################################
# Prio = Lowest: [SA = ANY, DA = ANY, SP = ANY, DP = ANY, PROTO = ANY] => Drop
# Prio = 1 (High): [SA = ANY, DA = 10.0.0.1, SP = ANY, DP = 1000, PROTO = UDP] => Allow
# Prio = 1 (High): [SA = ANY, DA = 10.1.0.1, SP = ANY, DP = 1001, PROTO = UDP] => Allow
# Prio = 1 (High): [SA = ANY, DA = 10.2.0.1, SP = ANY, DP = 1002, PROTO = UDP] => Allow
# Prio = 1 (High): [SA = ANY, DA = 10.3.0.1, SP = ANY, DP = 1003, PROTO = UDP] => Allow
p 1 firewall add default 1 #SINK0
p 2 firewall add priority 1 ipv4 0.0.0.0 0 10.0.0.1 32 0 65535 1000 1000 17 0xF port 0
p 2 firewall add priority 1 ipv4 0.0.0.0 0 10.1.0.1 32 0 65535 1001 1001 17 0xF port 0
p 2 firewall add priority 1 ipv4 0.0.0.0 0 10.2.0.1 32 0 65535 1002 1002 17 0xF port 0
p 2 firewall add priority 1 ipv4 0.0.0.0 0 10.3.0.1 32 0 65535 1003 1003 17 0xF port 0
#p 2 firewall ls

################################################################################
# Local destination TCP traffic
################################################################################
# Unknown connection => Drop
# TCP [SA = 100.0.0.10, DA = 10.0.0.1, SP = 1000, DP = 80] => socket ID = 0
# TCP [SA = 100.1.0.10, DA = 10.1.0.1, SP = 1001, DP = 80] => socket ID = 1
# TCP [SA = 100.2.0.10, DA = 10.2.0.1, SP = 1002, DP = 80] => socket ID = 2
# TCP [SA = 100.3.0.10, DA = 10.3.0.1, SP = 1003, DP = 80] => socket ID = 3
p 3 flow add default 1 #SINK1
p 3 flow add ipv4 100.0.0.10 10.0.0.1 1000 80 6 port 1 id 0
p 3 flow add ipv4 100.1.0.10 10.1.0.1 1001 80 6 port 1 id 1
p 3 flow add ipv4 100.2.0.10 10.2.0.1 1002 80 6 port 1 id 2
p 3 flow add ipv4 100.3.0.10 10.3.0.1 1003 80 6 port 1 id 3
#p 3 flow ls
