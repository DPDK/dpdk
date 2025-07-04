# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 Arm Limited

"""Softnic test suite.

Create a softnic virtual device and verify it successfully forwards packets.
"""

from pathlib import Path, PurePath

from framework.params.testpmd import EthPeer
from framework.remote_session.testpmd_shell import TestPmdShell
from framework.test_suite import TestSuite, func_test
from framework.testbed_model.capability import requires
from framework.testbed_model.topology import TopologyType
from framework.testbed_model.virtual_device import VirtualDevice
from framework.utils import generate_random_packets


@requires(topology_type=TopologyType.two_links)
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
        self.cli_file = self.prepare_softnic_files()

    def prepare_softnic_files(self) -> PurePath:
        """Creates the config files that are required for the creation of the softnic.

        The config files are created at runtime to accommodate paths and device addresses.
        """
        # paths of files needed by softnic
        cli_file = Path("rx_tx.cli")
        spec_file = Path("rx_tx.spec")
        rx_tx_1_file = Path("rx_tx_1.io")
        rx_tx_2_file = Path("rx_tx_2.io")
        path_sut = self._ctx.sut_node.tmp_dir
        cli_file_sut = self.sut_node.main_session.join_remote_path(path_sut, cli_file)
        spec_file_sut = self.sut_node.main_session.join_remote_path(path_sut, spec_file)
        rx_tx_1_file_sut = self.sut_node.main_session.join_remote_path(path_sut, rx_tx_1_file)
        rx_tx_2_file_sut = self.sut_node.main_session.join_remote_path(path_sut, rx_tx_2_file)
        firmware_c_file_sut = self.sut_node.main_session.join_remote_path(path_sut, "firmware.c")
        firmware_so_file_sut = self.sut_node.main_session.join_remote_path(path_sut, "firmware.so")

        # write correct remote paths to local files
        with open(cli_file, "w+") as fh:
            fh.write(f"pipeline codegen {spec_file_sut} {firmware_c_file_sut}\n")
            fh.write(f"pipeline libbuild {firmware_c_file_sut} {firmware_so_file_sut}\n")
            fh.write(f"pipeline RX build lib {firmware_so_file_sut} io {rx_tx_1_file_sut} numa 0\n")
            fh.write(f"pipeline TX build lib {firmware_so_file_sut} io {rx_tx_2_file_sut} numa 0\n")
            fh.write("thread 2 pipeline RX enable\n")
            fh.write("thread 2 pipeline TX enable\n")
        with open(spec_file, "w+") as fh:
            fh.write("struct metadata_t {{\n")
            fh.write("	bit<32> port\n")
            fh.write("}}\n")
            fh.write("metadata instanceof metadata_t\n")
            fh.write("apply {{\n")
            fh.write("	rx m.port\n")
            fh.write("	tx m.port\n")
            fh.write("}}\n")
        with open(rx_tx_1_file, "w+") as fh:
            fh.write(f"port in 0 ethdev {self.sut_node.config.ports[0].pci} rxq 0 bsz 32\n")
            fh.write("port out 0 ring RXQ0 bsz 32\n")
        with open(rx_tx_2_file, "w+") as fh:
            fh.write("port in 0 ring TXQ0 bsz 32\n")
            fh.write(f"port out 1 ethdev {self.sut_node.config.ports[1].pci} txq 0 bsz 32\n")

        # copy files over to SUT
        self.sut_node.main_session.copy_to(cli_file, cli_file_sut)
        self.sut_node.main_session.copy_to(spec_file, spec_file_sut)
        self.sut_node.main_session.copy_to(rx_tx_1_file, rx_tx_1_file_sut)
        self.sut_node.main_session.copy_to(rx_tx_2_file, rx_tx_2_file_sut)
        # and cleanup local files
        cli_file.unlink()
        spec_file.unlink()
        rx_tx_1_file.unlink()
        rx_tx_2_file.unlink()

        return cli_file_sut

    @func_test
    def softnic(self) -> None:
        """Softnic test.

        Steps:
            Start Testpmd with a softnic vdev using the provided config files.
            Testpmd forwarding is disabled, instead configure softnic to forward packets
            from port 0 to port 1 of the physical device.
            Send generated packets from the TG.

        Verify:
            The packets that are received are the same as the packets sent.

        """
        with TestPmdShell(
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
