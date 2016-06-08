#
# run ./config/edge_router_upstream.sh
#

################################################################################
# Firewall
################################################################################
p 1 firewall add default 4 #SINK0
p 1 firewall add bulk ./config/edge_router_upstream_firewall.txt
#p 1 firewall ls

################################################################################
# Flow Classification
################################################################################
p 3 flow add default 4 #SINK1
p 3 flow add qinq bulk ./config/edge_router_upstream_flow.txt
#p 3 flow ls

################################################################################
# Flow Actions - Metering and Policing
################################################################################
p 4 action flow bulk ./config/edge_router_upstream_action.txt
#p 4 action flow ls

################################################################################
# Routing: Ether MPLS, ARP off
################################################################################
p 5 route add default 4 #SINK2
p 5 route add 0.0.0.0 10 port 0 ether a0:b0:c0:d0:e0:f0 mpls 0:1
p 5 route add 0.64.0.0 10 port 1 ether a1:b1:c1:d1:e1:f1 mpls 10:11
p 5 route add 0.128.0.0 10 port 2 ether a2:b2:c2:d2:e2:f2 mpls 20:21
p 5 route add 0.192.0.0 10 port 3 ether a3:b3:c3:d3:e3:f3 mpls 30:31
#p 5 route ls
