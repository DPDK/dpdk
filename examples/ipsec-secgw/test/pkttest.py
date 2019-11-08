#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause

import fcntl
import pkg_resources
import socket
import struct
import sys
import unittest


if sys.version_info < (3, 0):
    print("Python3 is required to run this script")
    sys.exit(1)


try:
    from scapy.all import Ether
except ImportError:
    print("Scapy module is required")
    sys.exit(1)


PKTTEST_REQ = [
    "scapy>=2.4.3",
]


def assert_requirements(req):
    """
    assert requirement is met
    req can hold a string or a list of strings
    """
    try:
        pkg_resources.require(req)
    except (pkg_resources.DistributionNotFound, pkg_resources.VersionConflict) as e:
        print("Requirement assertion: " + str(e))
        sys.exit(1)


TAP_UNPROTECTED = "dtap1"
TAP_PROTECTED = "dtap0"


class Interface(object):
    ETH_P_ALL = 3
    MAX_PACKET_SIZE = 1280
    IOCTL_GET_INFO = 0x8927
    SOCKET_TIMEOUT = 0.5
    def __init__(self, ifname):
        self.name = ifname

        # create and bind socket to specified interface
        self.s = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(Interface.ETH_P_ALL))
        self.s.settimeout(Interface.SOCKET_TIMEOUT)
        self.s.bind((self.name, 0, socket.PACKET_OTHERHOST))

        # get interface MAC address
        info = fcntl.ioctl(self.s.fileno(), Interface.IOCTL_GET_INFO,  struct.pack('256s', bytes(ifname[:15], encoding='ascii')))
        self.mac = ':'.join(['%02x' % i for i in info[18:24]])

    def __del__(self):
        self.s.close()

    def send_l3packet(self, pkt, mac):
        e = Ether(src=self.mac, dst=mac)
        self.send_packet(e/pkt)

    def send_packet(self, pkt):
        self.send_bytes(bytes(pkt))

    def send_bytes(self, bytedata):
        self.s.send(bytedata)

    def recv_packet(self):
        return Ether(self.recv_bytes())

    def recv_bytes(self):
        return self.s.recv(Interface.MAX_PACKET_SIZE)

    def get_mac(self):
        return self.mac


class PacketXfer(object):
    def __init__(self, protected_iface=TAP_PROTECTED, unprotected_iface=TAP_UNPROTECTED):
        self.protected_port = Interface(protected_iface)
        self.unprotected_port = Interface(unprotected_iface)

    def send_to_protected_port(self, pkt, remote_mac=None):
        if remote_mac is None:
            remote_mac = self.unprotected_port.get_mac()
        self.protected_port.send_l3packet(pkt, remote_mac)

    def send_to_unprotected_port(self, pkt, remote_mac=None):
        if remote_mac is None:
            remote_mac = self.protected_port.get_mac()
        self.unprotected_port.send_l3packet(pkt, remote_mac)

    def xfer_unprotected(self, pkt):
        self.send_to_unprotected_port(pkt)
        return self.protected_port.recv_packet()

    def xfer_protected(self, pkt):
        self.send_to_protected_port(pkt)
        return self.unprotected_port.recv_packet()


def pkttest():
    if len(sys.argv) == 1:
        sys.exit(unittest.main(verbosity=2))
    elif len(sys.argv) == 2:
        if sys.argv[1] == "config":
            module = __import__('__main__')
            try:
                print(module.config())
            except AttributeError:
                sys.stderr.write("Cannot find \"config()\" in a test")
                sys.exit(1)
    else:
        sys.exit(1)


if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "check_reqs":
        assert_requirements(PKTTEST_REQ)
    else:
        print("Usage: " + sys.argv[0] + " check_reqs")
