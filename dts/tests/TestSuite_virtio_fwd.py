# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 University of New Hampshire

"""Virtio forwarding test suite.

Verify vhost/virtio pvp and fully virtual functionalities.
"""

from scapy.layers.inet import IP
from scapy.layers.l2 import Ether

from api.capabilities import LinkTopology
from api.packet import send_packets_and_capture
from api.test import log, verify
from api.testpmd import TestPmd
from api.testpmd.config import PortTopology, SimpleForwardingModes
from framework.test_suite import TestSuite, func_test
from framework.testbed_model.capability import requires
from framework.testbed_model.linux_session import LinuxSession
from framework.testbed_model.virtual_device import VirtualDevice


class TestVirtioFwd(TestSuite):
    """Virtio forwarding test suite."""

    virtio_user_vdev = VirtualDevice(
        "net_virtio_user0,mac=00:01:02:03:04:05,path=/tmp/vhost-net,server=1"
    )
    vhost_user_vdev = VirtualDevice("eth_vhost0,iface=/tmp/vhost-net,client=1")

    @requires(topology_type=LinkTopology.NO_LINK)
    @func_test
    def virtio_server(self) -> None:
        """Test virtio server packet transmission.

        Steps:
            * Launch a testpmd session with a vhost-user virtual device (client side).
            * Launch a testpmd session with a virtio-user virtual device (server side).
            * Set the forwarding mode to mac in both sessions.
            * Start packet forwarding on vhost session.
            * Send a burst of packets from the virtio session.
            * Stop packet forwarding on vhost session and collect packet stats.

        Verify:
            * Vhost session receives packets from virtio session.
        """
        with (
            TestPmd(
                prefix="vhost",
                no_pci=True,
                memory_channels=4,
                vdevs=[self.vhost_user_vdev],
            ) as vhost,
            TestPmd(
                prefix="virtio",
                no_pci=True,
                memory_channels=4,
                vdevs=[self.virtio_user_vdev],
            ) as virtio,
        ):
            vhost.set_forward_mode(SimpleForwardingModes.mac)
            virtio.set_forward_mode(SimpleForwardingModes.mac)

            vhost.start()
            virtio.start_tx_first(burst_num=32)
            vhost.stop()

            vhost_forwarding_stats, vhost_raw_output = vhost.show_port_stats_all()

            rx_packets = vhost_forwarding_stats[0].rx_packets
            tx_packets = vhost_forwarding_stats[0].tx_packets

            log(f"Vhost forwarding statistics:\n{vhost_raw_output}")

            verify(
                rx_packets != 0 and tx_packets != 0,
                "Vhost session failed to receive packets from virtio session.",
            )

    @requires(topology_type=LinkTopology.NO_LINK)
    @func_test
    def virtio_server_reconnect(self) -> None:
        """Test virtio server reconnection.

        Steps:
            * Launch a testpmd session with a vhost-user virtual device (client side).
            * Launch a testpmd session with a virtio-user virtual device (server side).
            * Close the virtio session and relaunch it.
            * Start packet forwarding on vhost session.
            * Send a burst of packets from the virtio session.
            * Stop packet forwarding on vhost session and collect packet stats.

        Verify:
            * Vhost session receives packets from relaunched virtio session.
        """
        with TestPmd(
            prefix="vhost",
            no_pci=True,
            memory_channels=4,
            vdevs=[self.vhost_user_vdev],
        ) as vhost:
            with TestPmd(
                prefix="virtio",
                no_pci=True,
                memory_channels=4,
                vdevs=[self.virtio_user_vdev],
            ) as virtio:
                pass
            # end session and reconnect
            with TestPmd(
                prefix="virtio",
                no_pci=True,
                memory_channels=4,
                vdevs=[self.virtio_user_vdev],
            ) as virtio:
                virtio.set_forward_mode(SimpleForwardingModes.mac)
                vhost.set_forward_mode(SimpleForwardingModes.mac)

                vhost.start()
                virtio.start_tx_first(burst_num=32)
                vhost.stop()

                vhost_forwarding_stats, vhost_raw_output = vhost.show_port_stats_all()

                rx_packets = vhost_forwarding_stats[0].rx_packets
                tx_packets = vhost_forwarding_stats[0].tx_packets

                log(f"Vhost forwarding statistics:\n{vhost_raw_output}")

                verify(
                    rx_packets != 0 and tx_packets != 0,
                    "Vhost session failed to receive packets from virtio session.",
                )

    @requires(topology_type=LinkTopology.ONE_LINK)
    @func_test
    def pvp_loop(self) -> None:
        """Test vhost/virtio physical-virtual-physical topology.

        Steps:
            * Launch testpmd session with a physical NIC and virtio-user vdev
                connected to a vhost-net socket.
            * Configure the tap interface that is created with IP address and
                set link state to UP.
            * Launch second testpmd session with af_packet vdev connected to
                the tap interface.
            * Start packet forwarding on both testpmd sessions.
            * Send 100 packets to the physical interface from external tester.

        Verify:
            * Vhost session receives/forwards 100+ packets.
        """
        self.sut_node = self._ctx.sut_node
        if not isinstance(self.sut_node.main_session, LinuxSession):
            verify(False, "Must be running on a Linux environment.")
        # delete tap0 interface if it exists
        self.sut_node.main_session.delete_interface(name="tap0")
        with TestPmd(
            prefix="virtio",
            vdevs=[VirtualDevice("virtio_user0,path=/dev/vhost-net,queues=1,queue_size=1024")],
            port_topology=PortTopology.chained,
        ) as virtio:
            self.sut_node.main_session.set_interface_link_up(name="tap0")
            with TestPmd(
                prefix="vhost",
                no_pci=True,
                vdevs=[VirtualDevice("net_af_packet0,iface=tap0")],
                port_topology=PortTopology.loop,
            ) as vhost:
                virtio.set_forward_mode(SimpleForwardingModes.mac)
                vhost.set_forward_mode(SimpleForwardingModes.mac)

                portlist_order = [0, 2, 1] if len(virtio.ports) == 3 else [0, 1]
                virtio.set_portlist(order=portlist_order)

                vhost.start()
                virtio.start()

                packet = Ether() / IP()
                packets = [packet] * 100
                send_packets_and_capture(packets)

                vhost.stop()
                virtio.stop()

                vhost_forwarding_stats, vhost_raw_output = vhost.show_port_stats_all()

                rx_packets = vhost_forwarding_stats[0].rx_packets
                tx_packets = vhost_forwarding_stats[0].tx_packets

                log(f"Vhost forwarding statistics:\n{vhost_raw_output}")

                verify(
                    rx_packets >= 100 and tx_packets >= 100,
                    f"PVP loop forwarding verification failed: vhost interface RX={rx_packets},"
                    f" TX={tx_packets} (expected ≥100 each).",
                )
