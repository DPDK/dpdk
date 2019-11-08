#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause

from scapy.all import *
import unittest
import pkttest


SRC_ADDR  = "1111:0000:0000:0000:0000:0000:0000:0001"
DST_ADDR  = "2222:0000:0000:0000:0000:0000:0000:0001"
SRC_NET   = "1111:0000:0000:0000:0000:0000:0000:0000/64"
DST_NET   = "2222:0000:0000:0000:0000:0000:0000:0000/64"


def config():
    return """
sp ipv6 out esp protect 5 pri 1 \\
src {0} \\
dst {1} \\
sport 0:65535 dport 0:65535

sp ipv6 in esp protect 6 pri 1 \\
src {1} \\
dst {0} \\
sport 0:65535 dport 0:65535

sa out 5 cipher_algo null auth_algo null mode transport
sa in 6 cipher_algo null auth_algo null mode transport

rt ipv6 dst {0} port 1
rt ipv6 dst {1} port 0
""".format(SRC_NET, DST_NET)


class TestTransportWithIPv6Ext(unittest.TestCase):
    # There is a bug in the IPsec Scapy implementation
    # which causes invalid packet reconstruction after
    # successful decryption. This method is a workaround.
    @staticmethod
    def decrypt(pkt, sa):
        esp = pkt[ESP]

        # decrypt dummy packet with no extensions
        d = sa.decrypt(IPv6()/esp)

        # fix 'next header' in the preceding header of the original
        # packet and remove ESP
        pkt[ESP].underlayer.nh = d[IPv6].nh
        pkt[ESP].underlayer.remove_payload()

        # combine L3 header with decrypted payload
        npkt = pkt/d[IPv6].payload

        # fix length
        npkt[IPv6].plen = d[IPv6].plen + len(pkt[IPv6].payload)

        return npkt

    def setUp(self):
        self.px = pkttest.PacketXfer()
        self.outb_sa = SecurityAssociation(ESP, spi=5)
        self.inb_sa = SecurityAssociation(ESP, spi=6)

    def test_outb_ipv6_noopt(self):
        pkt = IPv6(src=SRC_ADDR, dst=DST_ADDR)
        pkt /= UDP(sport=123,dport=456)/Raw(load="abc")

        # send and check response
        resp = self.px.xfer_unprotected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_ESP)
        self.assertEqual(resp[ESP].spi, 5)

        # decrypt response, check packet after decryption
        d = TestTransportWithIPv6Ext.decrypt(resp[IPv6], self.outb_sa)
        self.assertEqual(d[IPv6].nh, socket.IPPROTO_UDP)
        self.assertEqual(d[UDP].sport, 123)
        self.assertEqual(d[UDP].dport, 456)
        self.assertEqual(bytes(d[UDP].payload), b'abc')

    def test_outb_ipv6_opt(self):
        hoptions = []
        hoptions.append(RouterAlert(value=2))
        hoptions.append(Jumbo(jumboplen=5000))
        hoptions.append(Pad1())

        doptions = []
        doptions.append(HAO(hoa="1234::4321"))

        pkt = IPv6(src=SRC_ADDR, dst=DST_ADDR)
        pkt /= IPv6ExtHdrHopByHop(options=hoptions)
        pkt /= IPv6ExtHdrRouting(addresses=["3333::3","4444::4"])
        pkt /= IPv6ExtHdrDestOpt(options=doptions)
        pkt /= UDP(sport=123,dport=456)/Raw(load="abc")

        # send and check response
        resp = self.px.xfer_unprotected(pkt)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_HOPOPTS)

        # check extensions
        self.assertEqual(resp[IPv6ExtHdrHopByHop].nh, socket.IPPROTO_ROUTING)
        self.assertEqual(resp[IPv6ExtHdrRouting].nh, socket.IPPROTO_DSTOPTS)
        self.assertEqual(resp[IPv6ExtHdrDestOpt].nh, socket.IPPROTO_ESP)

        # check ESP
        self.assertEqual(resp[ESP].spi, 5)

        # decrypt response, check packet after decryption
        d = TestTransportWithIPv6Ext.decrypt(resp[IPv6], self.outb_sa)
        self.assertEqual(d[IPv6].nh, socket.IPPROTO_HOPOPTS)
        self.assertEqual(d[IPv6ExtHdrHopByHop].nh, socket.IPPROTO_ROUTING)
        self.assertEqual(d[IPv6ExtHdrRouting].nh, socket.IPPROTO_DSTOPTS)
        self.assertEqual(d[IPv6ExtHdrDestOpt].nh, socket.IPPROTO_UDP)

        # check UDP
        self.assertEqual(d[UDP].sport, 123)
        self.assertEqual(d[UDP].dport, 456)
        self.assertEqual(bytes(d[UDP].payload), b'abc')

    def test_inb_ipv6_noopt(self):
        # encrypt and send raw UDP packet
        pkt = IPv6(src=DST_ADDR, dst=SRC_ADDR)
        pkt /= UDP(sport=123,dport=456)/Raw(load="abc")
        e = self.inb_sa.encrypt(pkt)

        # send and check response
        resp = self.px.xfer_protected(e)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_UDP)

        # check UDP packet
        self.assertEqual(resp[UDP].sport, 123)
        self.assertEqual(resp[UDP].dport, 456)
        self.assertEqual(bytes(resp[UDP].payload), b'abc')

    def test_inb_ipv6_opt(self):
        hoptions = []
        hoptions.append(RouterAlert(value=2))
        hoptions.append(Jumbo(jumboplen=5000))
        hoptions.append(Pad1())

        doptions = []
        doptions.append(HAO(hoa="1234::4321"))

        # prepare packet with options
        pkt = IPv6(src=DST_ADDR, dst=SRC_ADDR)
        pkt /= IPv6ExtHdrHopByHop(options=hoptions)
        pkt /= IPv6ExtHdrRouting(addresses=["3333::3","4444::4"])
        pkt /= IPv6ExtHdrDestOpt(options=doptions)
        pkt /= UDP(sport=123,dport=456)/Raw(load="abc")
        e = self.inb_sa.encrypt(pkt)

        # self encrypted packet and check response
        resp = self.px.xfer_protected(e)
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_HOPOPTS)
        self.assertEqual(resp[IPv6ExtHdrHopByHop].nh, socket.IPPROTO_ROUTING)
        self.assertEqual(resp[IPv6ExtHdrRouting].nh, socket.IPPROTO_DSTOPTS)
        self.assertEqual(resp[IPv6ExtHdrDestOpt].nh, socket.IPPROTO_UDP)

        # check UDP
        self.assertEqual(resp[UDP].sport, 123)
        self.assertEqual(resp[UDP].dport, 456)
        self.assertEqual(bytes(resp[UDP].payload), b'abc')

    def test_inb_ipv6_frag(self):
        # prepare ESP payload
        pkt = IPv6()/UDP(sport=123,dport=456)/Raw(load="abc")
        e = self.inb_sa.encrypt(pkt)

        # craft and send inbound packet
        e = IPv6(src=DST_ADDR, dst=SRC_ADDR)/IPv6ExtHdrFragment()/e[IPv6].payload
        resp = self.px.xfer_protected(e)

        # check response
        self.assertEqual(resp[IPv6].nh, socket.IPPROTO_FRAGMENT)
        self.assertEqual(resp[IPv6ExtHdrFragment].nh, socket.IPPROTO_UDP)

        # check UDP
        self.assertEqual(resp[UDP].sport, 123)
        self.assertEqual(resp[UDP].dport, 456)
        self.assertEqual(bytes(resp[UDP].payload), b'abc')


pkttest.pkttest()
