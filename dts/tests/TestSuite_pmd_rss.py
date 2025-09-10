# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Arm Limited

"""RSS testing suite.

Tests different hashing algorithms by checking if packets are routed to correct queues.
Tests updating the RETA (Redirection Table) key to verify it takes effect and follows
set size constraints.
Tests RETA behavior under changing number of queues.
"""

import random

from scapy.layers.inet import IP, UDP
from scapy.layers.l2 import Ether

from api.capabilities import (
    LinkTopology,
    NicCapability,
    requires_link_topology,
    requires_nic_capability,
)
from api.test import verify
from api.packet import send_packets_and_capture
from api.testpmd import TestPmd
from api.testpmd.config import SimpleForwardingModes
from api.testpmd.types import (
    FlowRule,
    RSSOffloadTypesFlag,
    TestPmdVerbosePacket,
)
from framework.exception import InteractiveCommandExecutionError
from framework.test_suite import BaseConfig, TestSuite, func_test
from framework.utils import StrEnum


class Config(BaseConfig):
    """Default configuration for Per Test Suite config."""

    NUM_QUEUES: int = 4

    ACTUAL_KEY_SIZE: int = 52

    ACTUAL_RETA_SIZE: int = 512


class HashAlgorithm(StrEnum):
    """Enum of hashing algorithms."""

    DEFAULT = "default"
    SIMPLE_XOR = "simple_xor"
    TOEPLITZ = "toeplitz"
    SYMMETRIC_TOEPLITZ = "symmetric_toeplitz"


@requires_link_topology(LinkTopology.ONE_LINK)
@requires_nic_capability(NicCapability.PORT_RX_OFFLOAD_RSS_HASH)
class TestPmdRss(TestSuite):
    """PMD RSS test suite."""

    config: Config

    def _verify_hash_queue(
        self,
        reta: list[int],
        received_packets: list[TestPmdVerbosePacket],
        verify_packet_pairs: bool,
    ) -> None:
        """Verifies the packet hash corresponds to the packet queue.

        Given the received packets in the verbose output, iterate through each packet.
        Use the hash to index into RETA and get its intended queue.
        Verify the intended queue is the same as the actual queue the packet was received in.
        If the hash algorithm is symmetric, verify that pairs of packets have the same hash,
        as the pairs of packets sent have "mirrored" L4 ports.
        e.g. received_packets[0, 1, 2, 3, ...] hash(0) = hash(1), hash(2) = hash(3), ...

        Args:
            reta: Used to get predicted queue based on hash.
            received_packets: Packets received in the verbose output of testpmd.
            verify_packet_pairs: Verify pairs of packets have the same hash.

        Raises:
            InteractiveCommandExecutionError: If packet_hash is None.
        """
        # List of packet hashes, used for symmetric algorithms
        hash_list = []
        for packet in received_packets:
            # Ignore stray packets
            if packet.port_id != 0 or packet.src_mac != "02:00:00:00:00:00":
                continue
            # Get packet hash
            packet_hash = packet.rss_hash
            if packet_hash is None:
                raise InteractiveCommandExecutionError(
                    "Packet sent by the Traffic Generator has no RSS hash attribute."
                )

            packet_queue = packet.rss_queue

            # Calculate the predicted packet queue
            predicted_queue = reta[packet_hash % len(reta)]
            verify(
                predicted_queue == packet_queue,
                "Packet sent by the Traffic Generator assigned to incorrect queue by the RSS.",
            )

            if verify_packet_pairs:
                hash_list.append(packet_hash)

        if verify_packet_pairs:
            # Go through pairs of hashes in list and verify they are the same
            for odd_hash, even_hash in zip(hash_list[0::2], hash_list[1::2]):
                verify(
                    odd_hash == even_hash,
                    "Packet pair do not have same hash. Hash algorithm is not symmetric.",
                )

    def _send_test_packets(
        self,
        testpmd: TestPmd,
        send_additional_mirrored_packet: bool = False,
    ) -> list[TestPmdVerbosePacket]:
        """Sends test packets.

        Send 10 packets from the TG to SUT, parsing the verbose output and returning it.
        If the algorithm chosen is symmetric, send an additional packet for each initial
        packet sent, which has the L4 src and dst swapped.

        Args:
            testpmd: Used to send packets and send commands to testpmd.
            send_additional_mirrored_packet: Send an additional mirrored packet for each packet
            sent.

        Returns:
            TestPmdVerbosePacket: List of packets.
        """
        # Create test packets
        packets = []
        for i in range(10):
            packets.append(
                Ether(src="02:00:00:00:00:00", dst="11:00:00:00:00:00")
                / IP()
                / UDP(sport=i, dport=i + 1),
            )
            if send_additional_mirrored_packet:  # If symmetric, send the inverse packets
                packets.append(
                    Ether(src="02:00:00:00:00:00", dst="11:00:00:00:00:00")
                    / IP()
                    / UDP(sport=i + 1, dport=i),
                )

        # Set verbose packet information and start packet capture
        testpmd.set_verbose(level=3)
        testpmd.start()
        testpmd.start_all_ports()
        send_packets_and_capture(packets)

        # Stop packet capture and revert verbose packet information
        testpmd_shell_out = testpmd.stop()
        testpmd.set_verbose(level=0)
        # Parse the packets and return them
        return testpmd.extract_verbose_output(testpmd_shell_out)

    def _setup_rss_environment(
        self,
        testpmd: TestPmd,
    ) -> None:
        """Sets up the testpmd environment for RSS test suites.

        Sets the testpmd forward mode to rx_only and RSS on the NIC to UDP.

        Args:
            testpmd: Where the environment will be set.
        """
        # Set forward mode to receive only, to remove forwarded packets from verbose output
        testpmd.set_forward_mode(SimpleForwardingModes.rxonly)

        # Reset RSS settings and only RSS udp packets
        testpmd.port_config_all_rss_offload_type(RSSOffloadTypesFlag.udp)

    def _configure_reta_presets(self, testpmd: TestPmd, queue_number: int) -> list[list[int]]:
        """Build deterministic RETA presets.

          Sequential pattern [0,1,2,...] repeated to RETA size,
          reversed pattern [...,2,1,0] repeated to RETA size,
          and constant pattern of queue 0.

        Args:
            testpmd: The testpmd instance that will be used to set the RSS environment.
            queue_number: Number of queues available.

        Returns:
            List of RETA tables.

        Raises:
            InteractiveCommandExecutionError: If size of RETA table for driver is None.
        """
        reta_size = testpmd.show_port_info(port_id=0).redirection_table_size
        if reta_size is None:
            raise InteractiveCommandExecutionError("Size of RETA table for driver is None.")

        seq = [(i % queue_number) for i in range(reta_size)]
        rev = [queue_number - 1 - (i % queue_number) for i in range(reta_size)]
        const0 = [0 for _ in range(reta_size)]
        return [seq, rev, const0]

    def _apply_reta(self, testpmd: TestPmd, reta: list[int]) -> None:
        """Program a RETA into testpmd."""
        for i, q in enumerate(reta):
            testpmd.port_config_rss_reta(port_id=0, hash_index=i, queue_id=q)

    def _verify_rss_hash_function(
        self,
        testpmd: TestPmd,
        hash_algorithm: HashAlgorithm,
        flow_rule: FlowRule,
        reta: list[int],
    ) -> None:
        """Verifies hash function are working by sending test packets and checking the packet queue.

        Args:
            testpmd: The testpmd instance that will be used to set the rss environment.
            hash_algorithm: The hash algorithm to be tested.
            flow_rule: The flow rule that is to be validated and then created.
            reta: Will be used to calculate the predicted packet queues.
        """
        is_symmetric = hash_algorithm == HashAlgorithm.SYMMETRIC_TOEPLITZ
        self._setup_rss_environment(testpmd)
        testpmd.flow_create(flow_rule, port_id=0)
        # Send udp packets and ensure hash corresponds with queue
        parsed_output = self._send_test_packets(
            testpmd, send_additional_mirrored_packet=is_symmetric
        )
        self._verify_hash_queue(reta, parsed_output, is_symmetric)

    @func_test
    def key_hash_algorithm(self) -> None:
        """Hashing algorithm test.

        Steps:
            Setup RSS environment using the chosen algorithm.
            Send test packets for each flow rule.

        Verify:
            Packet hash corresponds to the packet queue.

        Raises:
            InteractiveCommandExecutionError: If size of RETA table for driver is None.
            InteractiveCommandExecutionError: If there are no valid flow rules that can be created.
        """
        failed_attempts: int = 0
        for algorithm in HashAlgorithm:
            flow_rule = FlowRule(
                group_id=0,
                direction="ingress",
                pattern=["eth / ipv4 / udp"],
                actions=[f"rss types ipv4-udp end queues end func {algorithm.name.lower()}"],
            )
            with TestPmd(
                rx_queues=self.config.NUM_QUEUES,
                tx_queues=self.config.NUM_QUEUES,
            ) as testpmd:
                reta_tables = self._configure_reta_presets(testpmd, self.config.NUM_QUEUES)
                for reta in reta_tables:
                    self._apply_reta(testpmd, reta)
                    if not testpmd.flow_validate(flow_rule, port_id=0):
                        # Queues need to be specified in the flow rule on some NICs
                        queue_ids = " ".join([str(x) for x in reta])
                        flow_rule.actions = [
                            f"rss types ipv4-udp end queues {queue_ids} end func "
                            + algorithm.name.lower()
                        ]

                        if not testpmd.flow_validate(flow_rule, port_id=0):
                            failed_attempts += 1
                            if failed_attempts == len(HashAlgorithm):
                                raise InteractiveCommandExecutionError(
                                    "No Valid flow rule could be created."
                                )
                            # if neither rule format is valid then the algorithm is not supported,
                            # move to next one
                            continue
                    self._verify_rss_hash_function(testpmd, algorithm, flow_rule, reta)

    @func_test
    def update_key_set_hash_key_short_long(self) -> None:
        """Set hash key short long test.

        Steps:
            Fetch the hash key size.
            Create two random hash keys one key too short and one too long.

        Verify:
            Verify that it is not possible to set the shorter hash key.
            Verify that it is not possible to set the longer hash key.

        Raises:
            InteractiveCommandExecutionError: If port info dose not contain hash key size.
        """
        with TestPmd(
            memory_channels=4,
            rx_queues=self.config.NUM_QUEUES,
            tx_queues=self.config.NUM_QUEUES,
        ) as testpmd:
            # Get RETA and key size
            port_info = testpmd.show_port_info(port_id=0)

            # Get hash key size
            key_size = port_info.hash_key_size
            if key_size is None:
                raise InteractiveCommandExecutionError("Port info does not contain hash key size.")

            # Create 2 hash keys based on the NIC capabilities
            short_key = "".join(
                [random.choice("0123456789ABCDEF") for n in range(key_size * 2 - 2)]
            )
            long_key = "".join([random.choice("0123456789ABCDEF") for n in range(key_size * 2 + 2)])

            # Verify a short key cannot be set
            short_key_out = testpmd.port_config_rss_hash_key(
                0, RSSOffloadTypesFlag.ipv4_udp, short_key, False
            )
            verify(
                "invalid" in short_key_out,
                "Able to set hash key shorter than specified.",
            )

            # Verify a long key cannot be set
            long_key_out = testpmd.port_config_rss_hash_key(
                0, RSSOffloadTypesFlag.ipv4_udp, long_key, False
            )
            verify("invalid" in long_key_out, "Able to set hash key longer than specified.")

    @func_test
    def update_key_reported_key_size(self) -> None:
        """Verify reported hash key size is the same as the NIC capabilities.

        Steps:
            Fetch the hash key size and compare to the actual key size.

        Verify:
            Reported key size is the same as the actual key size.
        """
        with TestPmd() as testpmd:
            reported_key_size = testpmd.show_port_info(port_id=0).hash_key_size
            verify(
                reported_key_size == self.config.ACTUAL_KEY_SIZE,
                "Reported key size is not the same as the config file.",
            )

    @func_test
    def reta_key_reta_queues(self) -> None:
        """RETA rx/tx queues test.

        Steps:
            For each queue size setup RSS environment and send Test packets.

        Verify:
            Packet hash corresponds to hash queue.

        Raises:
            InteractiveCommandExecutionError: If size of RETA table for driver is None.
        """
        queues_numbers = [2, 9, 16]
        for queue_number in queues_numbers:
            with TestPmd(
                rx_queues=queue_number,
                tx_queues=queue_number,
            ) as testpmd:
                reta_tables = self._configure_reta_presets(testpmd, queue_number)
                for reta in reta_tables:
                    self._apply_reta(testpmd, reta)
                    self._setup_rss_environment(testpmd)

                    # Send UDP packets and ensure hash corresponds with queue
                    parsed_output = self._send_test_packets(testpmd)
                    self._verify_hash_queue(reta, parsed_output, False)

    @func_test
    def reta_key_reported_reta_size(self) -> None:
        """Reported RETA size test.

        Steps:
            Fetch reported reta size.

        Verify:
            Reported RETA size is equal to the actual RETA size.
        """
        with TestPmd(
            rx_queues=self.config.NUM_QUEUES,
            tx_queues=self.config.NUM_QUEUES,
        ) as testpmd:
            reported_reta_size = testpmd.show_port_info(port_id=0).redirection_table_size
            verify(
                reported_reta_size == self.config.ACTUAL_RETA_SIZE,
                "Reported RETA size is not the same as the config file.",
            )
