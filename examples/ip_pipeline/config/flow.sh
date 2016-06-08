#
# run ./config/flow.sh
#

################################################################################
# Flow classification (QinQ)
################################################################################
#p 1 flow add default 4 #SINK0
#p 1 flow add qinq 100 200 port 0 id 0
#p 1 flow add qinq 101 201 port 1 id 1
#p 1 flow add qinq 102 202 port 2 id 2
#p 1 flow add qinq 103 203 port 3 id 3

#p 1 flow add qinq bulk ./config/flow.txt

################################################################################
# Flow classification (IPv4 5-tuple)
################################################################################
p 1 flow add default 4 #SINK0
p 1 flow add ipv4 100.0.0.10 200.0.0.10 100 200 6 port 0 id 0
p 1 flow add ipv4 100.0.0.11 200.0.0.11 101 201 6 port 1 id 1
p 1 flow add ipv4 100.0.0.12 200.0.0.12 102 202 6 port 2 id 2
p 1 flow add ipv4 100.0.0.13 200.0.0.13 103 203 6 port 3 id 3

#p 1 flow add ipv4 bulk ./config/flow.txt
