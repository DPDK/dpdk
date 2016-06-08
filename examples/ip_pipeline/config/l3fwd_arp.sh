#
# run ./config/l3fwd_arp.sh
#

################################################################################
# ARP
################################################################################
p 1 arp add default 4 #SINK0
p 1 arp add 0 10.0.0.1 a0:b0:c0:d0:e0:f0
p 1 arp add 1 11.0.0.1 a1:b1:c1:d1:e1:f1
p 1 arp add 2 12.0.0.1 a2:b2:c2:d2:e2:f2
p 1 arp add 3 13.0.0.1 a3:b3:c3:d3:e3:f3
p 1 arp ls

################################################################################
# Routing: encap = ethernet, arp = on
################################################################################
p 1 route add default 4 #SINK0
p 1 route add 100.0.0.0 10 port 0 ether 10.0.0.1
p 1 route add 100.64.0.0 10 port 1 ether 11.0.0.1
p 1 route add 100.128.0.0 10 port 2 ether 12.0.0.1
p 1 route add 100.192.0.0 10 port 3 ether 13.0.0.1
p 1 route ls

################################################################################
# Routing: encap = ethernet_qinq, arp = on
################################################################################
#p 1 route add default 4 #SINK0
#p 1 route add 100.0.0.0 10 port 0 ether 10.0.0.1 qinq 1000 2000
#p 1 route add 100.64.0.0 10 port 1 ether 11.0.0.1 qinq 1001 2001
#p 1 route add 100.128.0.0 10 port 2 ether 12.0.0.1 qinq 1002 2002
#p 1 route add 100.192.0.0 10 port 3 ether 13.0.0.1 qinq 1003 2003
#p 1 route ls

################################################################################
# Routing: encap = ethernet_mpls, arp = on
################################################################################
#p 1 route add default 4 #SINK0
#p 1 route add 100.0.0.0 10 port 0 ether 10.0.0.1 mpls 1000:2000
#p 1 route add 100.64.0.0 10 port 1 ether 11.0.0.1 mpls 1001:2001
#p 1 route add 100.128.0.0 10 port 2 ether 12.0.0.1 mpls 1002:2002
#p 1 route add 100.192.0.0 10 port 3 ether 13.0.0.1 mpls 1003:2003
#p 1 route ls
