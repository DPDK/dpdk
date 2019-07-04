#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

from scapy.all import *
import unittest
import pkttest

#{ipv4{ipv4}} test
SRC_ADDR_IPV4_1 = "192.168.1.1"
DST_ADDR_IPV4_1 = "192.168.2.1"

#{ipv6{ipv6}} test
SRC_ADDR_IPV6_1 = "1111:0000:0000:0000:0000:0000:0000:0001"
DST_ADDR_IPV6_1 = "2222:0000:0000:0000:0000:0000:0000:0001"

#{ipv4{ipv6}} test
SRC_ADDR_IPV4_2 = "192.168.11.1"
DST_ADDR_IPV4_2 = "192.168.12.1"
SRC_ADDR_IPV6_2 = "1111:0000:0000:0000:0000:0000:0001:0001"
DST_ADDR_IPV6_2 = "2222:0000:0000:0000:0000:0000:0001:0001"

#{ipv6{ipv4}} test
SRC_ADDR_IPV4_3 = "192.168.21.1"
DST_ADDR_IPV4_3 = "192.168.22.1"
SRC_ADDR_IPV6_3 = "1111:0000:0000:0000:0000:0001:0001:0001"
DST_ADDR_IPV6_3 = "2222:0000:0000:0000:0000:0001:0001:0001"

def config():
    return """
#outter-ipv4 inner-ipv4 tunnel mode test
sp ipv4 out esp protect 5 pri 1 \\
src {0}/32 \\
dst {1}/32 \\
sport 0:65535 dport 0:65535

sp ipv4 in esp protect 6 pri 1 \\
src {1}/32 \\
dst {0}/32 \\
sport 0:65535 dport 0:65535

sa out 5 cipher_algo null auth_algo null mode ipv4-tunnel \\
src {0} dst {1}
sa in 6 cipher_algo null auth_algo null mode ipv4-tunnel \\
src {1} dst {0}

rt ipv4 dst {0}/32 port 1
rt ipv4 dst {1}/32 port 0

#outter-ipv6 inner-ipv6 tunnel mode test
sp ipv6 out esp protect 7 pri 1 \\
src {2}/128 \\
dst {3}/128 \\
sport 0:65535 dport 0:65535

sp ipv6 in esp protect 8 pri 1 \\
src {3}/128 \\
dst {2}/128 \\
sport 0:65535 dport 0:65535

sa out 7 cipher_algo null auth_algo null mode ipv6-tunnel \\
src {2} dst {3}
sa in 8 cipher_algo null auth_algo null mode ipv6-tunnel \\
src {3} dst {2}

rt ipv6 dst {2}/128 port 1
rt ipv6 dst {3}/128 port 0

#outter-ipv4 inner-ipv6 tunnel mode test
sp ipv6 out esp protect 9 pri 1 \\
src {4}/128 \\
dst {5}/128 \\
sport 0:65535 dport 0:65535

sp ipv6 in esp protect 10 pri 1 \\
src {5}/128 \\
dst {4}/128 \\
sport 0:65535 dport 0:65535

sa out 9 cipher_algo null auth_algo null mode ipv4-tunnel \\
src {6} dst {7}
sa in 10 cipher_algo null auth_algo null mode ipv4-tunnel \\
src {7} dst {6}

rt ipv6 dst {4}/128 port 1
rt ipv4 dst {7}/32 port 0

#outter-ipv6 inner-ipv4 tunnel mode test
sp ipv4 out esp protect 11 pri 1 \\
src {8}/32 \\
dst {9}/32 \\
sport 0:65535 dport 0:65535

sp ipv4 in esp protect 12 pri 1 \\
src {9}/32 \\
dst {8}/32 \\
sport 0:65535 dport 0:65535

sa out 11 cipher_algo null auth_algo null mode ipv6-tunnel \\
src {10} dst {11}
sa in 12 cipher_algo null auth_algo null mode ipv6-tunnel \\
src {11} dst {10}

rt ipv4 dst {8}/32 port 1
rt ipv6 dst {11}/128 port 0
""".format(SRC_ADDR_IPV4_1, DST_ADDR_IPV4_1,
           SRC_ADDR_IPV6_1, DST_ADDR_IPV6_1,
           SRC_ADDR_IPV6_2, DST_ADDR_IPV6_2, SRC_ADDR_IPV4_2, DST_ADDR_IPV4_2,
           SRC_ADDR_IPV4_3, DST_ADDR_IPV4_3, SRC_ADDR_IPV6_3, DST_ADDR_IPV6_3)

ECN_ECT0    = 0x02
ECN_ECT1    = 0x01
ECN_CE      = 0x03
DSCP_1      = 0x04
DSCP_3F     = 0xFC

class TestTunnelHeaderReconstruct(unittest.TestCase):
    def setUp(self):
        self.px = pkttest.PacketXfer()
        th = IP(src=DST_ADDR_IPV4_1, dst=SRC_ADDR_IPV4_1)
        self.sa_ipv4v4 = SecurityAssociation(ESP, spi=6, tunnel_header = th)

        th = IPv6(src=DST_ADDR_IPV6_1, dst=SRC_ADDR_IPV6_1)
        self.sa_ipv6v6 = SecurityAssociation(ESP, spi=8, tunnel_header = th)

        th = IP(src=DST_ADDR_IPV4_2, dst=SRC_ADDR_IPV4_2)
        self.sa_ipv4v6 = SecurityAssociation(ESP, spi=10, tunnel_header = th)

        th = IPv6(src=DST_ADDR_IPV6_3, dst=SRC_ADDR_IPV6_3)
        self.sa_ipv6v4 = SecurityAssociation(ESP, spi=12, tunnel_header = th)

    def gen_pkt_plain_ipv4(self, src, dst, tos):
        pkt = IP(src=src, dst=dst, tos=tos)
        pkt /= UDP(sport=123,dport=456)/Raw(load="abc")
        return pkt

    def gen_pkt_plain_ipv6(self, src, dst, tc):
        pkt = IPv6(src=src, dst=dst, tc=tc)
        pkt /= UDP(sport=123,dport=456)/Raw(load="abc")
        return pkt

    def gen_pkt_tun_ipv4v4(self, tos_outter, tos_inner):
        pkt = self.gen_pkt_plain_ipv4(DST_ADDR_IPV4_1, SRC_ADDR_IPV4_1,
                                      tos_inner)
        pkt = self.sa_ipv4v4.encrypt(pkt)
        self.assertEqual(pkt[IP].proto, socket.IPPROTO_ESP)
        self.assertEqual(pkt[ESP].spi, 6)
        pkt[IP].tos = tos_outter
        return pkt

    def gen_pkt_tun_ipv6v6(self, tc_outter, tc_inner):
        pkt = self.gen_pkt_plain_ipv6(DST_ADDR_IPV6_1, SRC_ADDR_IPV6_1,
                                      tc_inner)
        pkt = self.sa_ipv6v6.encrypt(pkt)
        self.assertEqual(pkt[IPv6].nh, socket.IPPROTO_ESP)
        self.assertEqual(pkt[ESP].spi, 8)
        pkt[IPv6].tc = tc_outter
        return pkt

    def gen_pkt_tun_ipv4v6(self, tos_outter, tc_inner):
        pkt = self.gen_pkt_plain_ipv6(DST_ADDR_IPV6_2, SRC_ADDR_IPV6_2,
                                      tc_inner)
        pkt = self.sa_ipv4v6.encrypt(pkt)
        self.assertEqual(pkt[IP].proto, socket.IPPROTO_ESP)
        self.assertEqual(pkt[ESP].spi, 10)
        pkt[IP].tos = tos_outter
        return pkt

    def gen_pkt_tun_ipv6v4(self, tc_outter, tos_inner):
        pkt = self.gen_pkt_plain_ipv4(DST_ADDR_IPV4_3, SRC_ADDR_IPV4_3,
                                      tos_inner)
        pkt = self.sa_ipv6v4.encrypt(pkt)
        self.assertEqual(pkt[IPv6].nh, socket.IPPROTO_ESP)
        self.assertEqual(pkt[ESP].spi, 12)
        pkt[IPv6].tc = tc_outter
        return pkt

#RFC4301 5.1.2.1 & 5.1.2.2, outbound packets shall be copied ECN field
    def test_outb_ipv4v4_ecn(self):
        pkt = self.gen_pkt_plain_ipv4(SRC_ADDR_IPV4_1, DST_ADDR_IPV4_1,
                                      ECN_ECT1)
        resp = self.px.xfer_unprotected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_ESP)
        self.assertEqual(resp[ESP].spi, 5)
        self.assertEqual(resp[IP].tos, ECN_ECT1)

        pkt = self.gen_pkt_plain_ipv4(SRC_ADDR_IPV4_1, DST_ADDR_IPV4_1,
                                      ECN_ECT0)
        resp = self.px.xfer_unprotected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_ESP)
        self.assertEqual(resp[ESP].spi, 5)
        self.assertEqual(resp[IP].tos, ECN_ECT0)

        pkt = self.gen_pkt_plain_ipv4(SRC_ADDR_IPV4_1, DST_ADDR_IPV4_1,
                                      ECN_CE)
        resp = self.px.xfer_unprotected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_ESP)
        self.assertEqual(resp[ESP].spi, 5)
        self.assertEqual(resp[IP].tos, ECN_CE)

    def test_outb_ipv6v6_ecn(self):
        pkt = self.gen_pkt_plain_ipv6(SRC_ADDR_IPV6_1, DST_ADDR_IPV6_1,
                                      ECN_ECT1)
        resp = self.px.xfer_unprotected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_ESP)
        self.assertEqual(resp[IPv6].tc, ECN_ECT1)

        pkt = self.gen_pkt_plain_ipv6(SRC_ADDR_IPV6_1, DST_ADDR_IPV6_1,
                                      ECN_ECT0)
        resp = self.px.xfer_unprotected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_ESP)
        self.assertEqual(resp[ESP].spi, 7)
        self.assertEqual(resp[IPv6].tc, ECN_ECT0)

        pkt = self.gen_pkt_plain_ipv6(SRC_ADDR_IPV6_1, DST_ADDR_IPV6_1,
                                      ECN_CE)
        resp = self.px.xfer_unprotected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_ESP)
        self.assertEqual(resp[ESP].spi, 7)
        self.assertEqual(resp[IPv6].tc, ECN_CE)

    def test_outb_ipv4v6_ecn(self):
        pkt = self.gen_pkt_plain_ipv6(SRC_ADDR_IPV6_2, DST_ADDR_IPV6_2,
                                      ECN_ECT1)
        resp = self.px.xfer_unprotected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_ESP)
        self.assertEqual(resp[IP].tos, ECN_ECT1)

        pkt = self.gen_pkt_plain_ipv6(SRC_ADDR_IPV6_2, DST_ADDR_IPV6_2,
                                      ECN_ECT0)
        resp = self.px.xfer_unprotected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_ESP)
        self.assertEqual(resp[IP].tos, ECN_ECT0)

        pkt = self.gen_pkt_plain_ipv6(SRC_ADDR_IPV6_2, DST_ADDR_IPV6_2,
                                      ECN_CE)
        resp = self.px.xfer_unprotected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_ESP)
        self.assertEqual(resp[IP].tos, ECN_CE)

    def test_outb_ipv6v4_ecn(self):
        pkt = self.gen_pkt_plain_ipv4(SRC_ADDR_IPV4_3, DST_ADDR_IPV4_3,
                                      ECN_ECT1)
        resp = self.px.xfer_unprotected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_ESP)
        self.assertEqual(resp[IPv6].tc, ECN_ECT1)

        pkt = self.gen_pkt_plain_ipv4(SRC_ADDR_IPV4_3, DST_ADDR_IPV4_3,
                                      ECN_ECT0)
        resp = self.px.xfer_unprotected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_ESP)
        self.assertEqual(resp[IPv6].tc, ECN_ECT0)

        pkt = self.gen_pkt_plain_ipv4(SRC_ADDR_IPV4_3, DST_ADDR_IPV4_3,
                                      ECN_CE)
        resp = self.px.xfer_unprotected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_ESP)
        self.assertEqual(resp[IPv6].tc, ECN_CE)

#RFC4301 5.1.2.1 & 5.1.2.2, if outbound packets ECN is CE (0x3), inbound packets
#ECN is overwritten to CE, otherwise no change

#Outter header not CE, Inner header should be no change
    def test_inb_ipv4v4_ecn_inner_no_change(self):
        pkt = self.gen_pkt_tun_ipv4v4(ECN_ECT1, ECN_ECT0)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_UDP)
        self.assertEqual(resp[IP].tos, ECN_ECT0)

        pkt = self.gen_pkt_tun_ipv4v4(ECN_ECT0, ECN_ECT1)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_UDP)
        self.assertEqual(resp[IP].tos, ECN_ECT1)

        pkt = self.gen_pkt_tun_ipv4v4(ECN_ECT1, ECN_CE)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_UDP)
        self.assertEqual(resp[IP].tos, ECN_CE)

    def test_inb_ipv6v6_ecn_inner_no_change(self):
        pkt = self.gen_pkt_tun_ipv6v6(ECN_ECT1, ECN_ECT0)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_UDP)
        self.assertEqual(resp[IPv6].tc, ECN_ECT0)

        pkt = self.gen_pkt_tun_ipv6v6(ECN_ECT0, ECN_ECT1)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_UDP)
        self.assertEqual(resp[IPv6].tc, ECN_ECT1)

        pkt = self.gen_pkt_tun_ipv6v6(ECN_ECT1, ECN_CE)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_UDP)
        self.assertEqual(resp[IPv6].tc, ECN_CE)

    def test_inb_ipv4v6_ecn_inner_no_change(self):
        pkt = self.gen_pkt_tun_ipv4v6(ECN_ECT1, ECN_ECT0)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_UDP)
        self.assertEqual(resp[IPv6].tc, ECN_ECT0)

        pkt = self.gen_pkt_tun_ipv4v6(ECN_ECT0, ECN_ECT1)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_UDP)
        self.assertEqual(resp[IPv6].tc, ECN_ECT1)

        pkt = self.gen_pkt_tun_ipv4v6(ECN_ECT1, ECN_CE)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_UDP)
        self.assertEqual(resp[IPv6].tc, ECN_CE)

    def test_inb_ipv6v4_ecn_inner_no_change(self):
        pkt = self.gen_pkt_tun_ipv6v4(ECN_ECT1, ECN_ECT0)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_UDP)
        self.assertEqual(resp[IP].tos, ECN_ECT0)

        pkt = self.gen_pkt_tun_ipv6v4(ECN_ECT0, ECN_ECT1)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_UDP)
        self.assertEqual(resp[IP].tos, ECN_ECT1)

        pkt = self.gen_pkt_tun_ipv6v4(ECN_ECT1, ECN_CE)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_UDP)
        self.assertEqual(resp[IP].tos, ECN_CE)

#Outter header CE, Inner header should be changed to CE
    def test_inb_ipv4v4_ecn_inner_change(self):
        pkt = self.gen_pkt_tun_ipv4v4(ECN_CE, ECN_ECT0)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_UDP)
        self.assertEqual(resp[IP].tos, ECN_CE)

        pkt = self.gen_pkt_tun_ipv4v4(ECN_CE, ECN_ECT1)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_UDP)
        self.assertEqual(resp[IP].tos, ECN_CE)

    def test_inb_ipv6v6_ecn_inner_change(self):
        pkt = self.gen_pkt_tun_ipv6v6(ECN_CE, ECN_ECT0)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_UDP)
        self.assertEqual(resp[IPv6].tc, ECN_CE)

        pkt = self.gen_pkt_tun_ipv6v6(ECN_CE, ECN_ECT1)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_UDP)
        self.assertEqual(resp[IPv6].tc, ECN_CE)

    def test_inb_ipv4v6_ecn_inner_change(self):
        pkt = self.gen_pkt_tun_ipv4v6(ECN_CE, ECN_ECT0)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_UDP)
        self.assertEqual(resp[IPv6].tc, ECN_CE)

        pkt = self.gen_pkt_tun_ipv4v6(ECN_CE, ECN_ECT1)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_UDP)
        self.assertEqual(resp[IPv6].tc, ECN_CE)

    def test_inb_ipv6v4_ecn_inner_change(self):
        pkt = self.gen_pkt_tun_ipv6v4(ECN_CE, ECN_ECT0)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_UDP)
        self.assertEqual(resp[IP].tos, ECN_CE)

        pkt = self.gen_pkt_tun_ipv6v4(ECN_CE, ECN_ECT1)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_UDP)
        self.assertEqual(resp[IP].tos, ECN_CE)

#RFC4301 5.1.2.1.5 Outer DS field should be copied from Inner DS field
    def test_outb_ipv4v4_dscp(self):
        pkt = self.gen_pkt_plain_ipv4(SRC_ADDR_IPV4_1, DST_ADDR_IPV4_1,
                                      DSCP_1)
        resp = self.px.xfer_unprotected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_ESP)
        self.assertEqual(resp[ESP].spi, 5)
        self.assertEqual(resp[IP].tos, DSCP_1)

        pkt = self.gen_pkt_plain_ipv4(SRC_ADDR_IPV4_1, DST_ADDR_IPV4_1,
                                      DSCP_3F)
        resp = self.px.xfer_unprotected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_ESP)
        self.assertEqual(resp[ESP].spi, 5)
        self.assertEqual(resp[IP].tos, DSCP_3F)

    def test_outb_ipv6v6_dscp(self):
        pkt = self.gen_pkt_plain_ipv6(SRC_ADDR_IPV6_1, DST_ADDR_IPV6_1,
                                      DSCP_1)
        resp = self.px.xfer_unprotected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_ESP)
        self.assertEqual(resp[ESP].spi, 7)
        self.assertEqual(resp[IPv6].tc, DSCP_1)

        pkt = self.gen_pkt_plain_ipv6(SRC_ADDR_IPV6_1, DST_ADDR_IPV6_1,
                                      DSCP_3F)
        resp = self.px.xfer_unprotected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_ESP)
        self.assertEqual(resp[ESP].spi, 7)
        self.assertEqual(resp[IPv6].tc, DSCP_3F)

    def test_outb_ipv4v6_dscp(self):
        pkt = self.gen_pkt_plain_ipv6(SRC_ADDR_IPV6_2, DST_ADDR_IPV6_2,
                                      DSCP_1)
        resp = self.px.xfer_unprotected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_ESP)
        self.assertEqual(resp[ESP].spi, 9)
        self.assertEqual(resp[IP].tos, DSCP_1)

        pkt = self.gen_pkt_plain_ipv6(SRC_ADDR_IPV6_2, DST_ADDR_IPV6_2,
                                      DSCP_3F)
        resp = self.px.xfer_unprotected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_ESP)
        self.assertEqual(resp[ESP].spi, 9)
        self.assertEqual(resp[IP].tos, DSCP_3F)

    def test_outb_ipv6v4_dscp(self):
        pkt = self.gen_pkt_plain_ipv4(SRC_ADDR_IPV4_3, DST_ADDR_IPV4_3,
                                      DSCP_1)
        resp = self.px.xfer_unprotected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_ESP)
        self.assertEqual(resp[ESP].spi, 11)
        self.assertEqual(resp[IPv6].tc, DSCP_1)

        pkt = self.gen_pkt_plain_ipv4(SRC_ADDR_IPV4_3, DST_ADDR_IPV4_3,
                                      DSCP_3F)
        resp = self.px.xfer_unprotected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_ESP)
        self.assertEqual(resp[ESP].spi, 11)
        self.assertEqual(resp[IPv6].tc, DSCP_3F)

#RFC4301 5.1.2.1.5 Inner DS field should not be affected by Outer DS field
    def test_inb_ipv4v4_dscp(self):
        pkt = self.gen_pkt_tun_ipv4v4(DSCP_3F, DSCP_1)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_UDP)
        self.assertEqual(resp[IP].tos, DSCP_1)

        pkt = self.gen_pkt_tun_ipv4v4(DSCP_1, DSCP_3F)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_UDP)
        self.assertEqual(resp[IP].tos, DSCP_3F)

    def test_inb_ipv6v6_dscp(self):
        pkt = self.gen_pkt_tun_ipv6v6(DSCP_3F, DSCP_1)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_UDP)
        self.assertEqual(resp[IPv6].tc, DSCP_1)

        pkt = self.gen_pkt_tun_ipv6v6(DSCP_1, DSCP_3F)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_UDP)
        self.assertEqual(resp[IPv6].tc, DSCP_3F)

    def test_inb_ipv4v6_dscp(self):
        pkt = self.gen_pkt_tun_ipv4v6(DSCP_3F, DSCP_1)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_UDP)
        self.assertEqual(resp[IPv6].tc, DSCP_1)

        pkt = self.gen_pkt_tun_ipv4v6(DSCP_1, DSCP_3F)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_UDP)
        self.assertEqual(resp[IPv6].tc, DSCP_3F)

    def test_inb_ipv6v4_dscp(self):
        pkt = self.gen_pkt_tun_ipv6v4(DSCP_3F, DSCP_1)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_UDP)
        self.assertEqual(resp[IP].tos, DSCP_1)

        pkt = self.gen_pkt_tun_ipv6v4(DSCP_1, DSCP_3F)
        resp = self.px.xfer_protected(pkt)
        self.assertEqual(resp[IP].proto, socket.IPPROTO_UDP)
        self.assertEqual(resp[IP].tos, DSCP_3F)

pkttest.pkttest()
