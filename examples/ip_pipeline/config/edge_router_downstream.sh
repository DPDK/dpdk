#
# run ./config/edge_router_downstream.sh
#

################################################################################
# Routing: Ether QinQ, ARP off
################################################################################
p 1 route add default 4 #SINK0
p 1 route add 0.0.0.0 10 port 0 ether a0:b0:c0:d0:e0:f0 qinq 256 257
p 1 route add 0.64.0.0 10 port 1 ether a1:b1:c1:d1:e1:f1 qinq 258 259
p 1 route add 0.128.0.0 10 port 2 ether a2:b2:c2:d2:e2:f2 qinq 260 261
p 1 route add 0.192.0.0 10 port 3 ether a3:b3:c3:d3:e3:f3 qinq 262 263
#p 1 route ls
