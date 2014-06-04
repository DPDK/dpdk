#Address Resolution Protocol (ARP) Table
#arp add iface ipaddr macaddr
arp add 0 0.0.0.1 0a:0b:0c:0d:0e:0f
arp add 1 0.128.0.1 1a:1b:1c:1d:1e:1f

#Routing Table
#route add ipaddr prefixlen iface gateway
route add 0.0.0.0 9 0 0.0.0.1
route add 0.128.0.0 9 1 0.128.0.1

#Flow Table
flow add all
#flow add 0.0.0.0 1.2.3.4 0 0 6 0
#flow add 10.11.12.13 0.0.0.0 0 0 6 1

#Firewall
#firewall add 1 0.0.0.0 0 0.0.0.0 9 0 65535 0 65535 6 0xf 0
#firewall add 1 0.0.0.0 0 0.128.0.0 9 0 65535 0 65535 6 0xf 1
