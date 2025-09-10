# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 Arm Limited

"""Softnic test suite.

Create a softnic virtual device and verify it successfully forwards packets.
"""

from api.capabilities import (
    LinkTopology,
    NicCapability,
    requires_link_topology,
    requires_nic_capability,
)
from api.testpmd import TestPmd
from api.testpmd.config import EthPeer
from framework.test_suite import TestSuite, func_test
from framework.testbed_model.artifact import Artifact
from framework.testbed_model.virtual_device import VirtualDevice
from framework.utils import generate_random_packets


@requires_nic_capability(NicCapability.PHYSICAL_FUNCTION)
@requires_link_topology(LinkTopology.TWO_LINKS)
class TestSoftnic(TestSuite):
    """Softnic test suite."""

    #: The total number of packets to generate and send for forwarding.
    NUMBER_OF_PACKETS_TO_SEND = 10
    #: The payload size to use for the generated packets in bytes.
    PAYLOAD_SIZE = 100

    def set_up_suite(self) -> None:
        """Set up the test suite.

        Setup:
            Generate the random packets that will be sent and create the softnic config files.
        """
        self.sut_node = self._ctx.sut_node  # FIXME: accessing the context should be forbidden
        self.packets = generate_random_packets(self.NUMBER_OF_PACKETS_TO_SEND, self.PAYLOAD_SIZE)

        self.cli_file = Artifact("sut", "rx_tx.cli")
        self.spec_file = Artifact("sut", "rx_tx.spec")
        self.rx_tx_1_file = Artifact("sut", "rx_tx_1.io")
        self.rx_tx_2_file = Artifact("sut", "rx_tx_2.io")
        self.firmware_c_file = Artifact("sut", "firmware.c")
        self.firmware_so_file = Artifact("sut", "firmware.so")

        with self.cli_file.open("w+") as fh:
            fh.write(
                f"pipeline codegen {self.spec_file} {self.firmware_c_file}\n"
                f"pipeline libbuild {self.firmware_c_file} {self.firmware_so_file}\n"
                f"pipeline RX build lib {self.firmware_so_file} io {self.rx_tx_1_file} numa 0\n"
                f"pipeline TX build lib {self.firmware_so_file} io {self.rx_tx_2_file} numa 0\n"
                "thread 2 pipeline RX enable\n"
                "thread 2 pipeline TX enable\n"
            )
        with self.spec_file.open("w+") as fh:
            fh.write(
                "struct metadata_t {\n"
                "	bit<32> port\n"
                "}\n"
                "metadata instanceof metadata_t\n"
                "apply {\n"
                "	rx m.port\n"
                "	tx m.port\n"
                "}\n"
            )
        with self.rx_tx_1_file.open("w+") as fh:
            fh.write(
                f"port in 0 ethdev {self.sut_node.config.ports[0].pci} rxq 0 bsz 32\n"
                "port out 0 ring RXQ0 bsz 32\n"
            )
        with self.rx_tx_2_file.open("w+") as fh:
            fh.write(
                "port in 0 ring TXQ0 bsz 32\n"
                f"port out 1 ethdev {self.sut_node.config.ports[1].pci} txq 0 bsz 32\n"
            )

    @func_test
    def softnic(self) -> None:
        """Softnic test.

        Steps:
            * Start Testpmd with a softnic vdev using the provided config files.
            * Testpmd forwarding is disabled, instead configure softnic to forward packets
            * from port 0 to port 1 of the physical device.
            * Send generated packets from the TG.

        Verify:
            * The packets that are received are the same as the packets sent.
        """
        with TestPmd(
            vdevs=[VirtualDevice(f"net_softnic0,firmware={self.cli_file},cpu_id=1,conn_port=8086")],
            eth_peer=[EthPeer(1, self.topology.tg_port_ingress.mac_address)],
            port_topology=None,
        ) as shell:
            shell.start()
            received_packets = self.send_packets_and_capture(self.packets)
            # packets are being forwarded without addresses being amended so
            # we get the address as it would be expected to come from TG
            expected_packets = self.get_expected_packets(self.packets, sent_from_tg=True)

            self.match_all_packets(expected_packets, received_packets)
