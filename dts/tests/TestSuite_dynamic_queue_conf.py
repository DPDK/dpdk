# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 University of New Hampshire

"""Dynamic configuration of port queues test suite.

This test suite tests the support of being able to either stop or reconfigure port queues at
runtime without stopping the entire device. Previously, to configure a DPDK ethdev, the application
first specifies how many Tx and Rx queues to include in the ethdev and then application sets up
each queue individually. Only once all the queues have been set up can the application then start
the device, and at this point traffic can flow. If device stops, this halts the flow of traffic on
all queues in the ethdev completely. Dynamic queue is a capability present on some NICs that
specifies whether the NIC is able to delay the configuration of queues on its port. This capability
allows for the support of stopping and reconfiguring queues on a port at runtime without stopping
the entire device.

Support of this capability is shown by starting the Poll Mode Driver with multiple Rx and Tx queues
configured and stopping some prior to forwarding packets, then examining whether or not the stopped
ports and the unmodified ports were able to handle traffic. In addition to just stopping the ports,
the ports must also show that they support configuration changes on their queues at runtime without
stopping the entire device. This is shown by changing the ring size of the queues.

If the Poll Mode Driver is able to stop some queues on a port and modify them then handle traffic
on the unmodified queues while the others are stopped, then it is the case that the device properly
supports dynamic configuration of its queues.
"""

import random
from typing import Callable, ClassVar, MutableSet

from scapy.layers.inet import IP
from scapy.layers.l2 import Ether
from scapy.packet import Raw

from api.capabilities import (
    NicCapability,
    requires_nic_capability,
)
from api.testpmd import TestPmd
from api.testpmd.config import PortTopology, SimpleForwardingModes
from framework.exception import InteractiveCommandExecutionError
from framework.test_suite import TestSuite, func_test


def setup_and_teardown_test(
    test_meth: Callable[["TestDynamicQueueConf", int, MutableSet, MutableSet, TestPmd, bool], None],
) -> Callable[["TestDynamicQueueConf", bool], None]:
    """Decorator that provides a setup and teardown for testing methods.

    This decorator provides a method that sets up the environment for testing, runs the test
    method, and then does a clean-up verification step after the queues are started again. The
    decorated method will be provided with all the variables it should need to run testing
    including: The ID of the port where the queues for testing reside, disjoint sets of IDs for
    queues that are/aren't modified, a testpmd session to run testing with, and a flag that
    indicates whether or not testing should be done on Rx or Tx queues.

    Args:
        test_meth: The decorated method that tests configuration of port queues at runtime.
            This method must have the following parameters in order: An int that represents a
            port ID, a set of queues for testing, a set of unmodified queues, a testpmd
            interactive shell, and a boolean that, when :data:`True`, does Rx testing,
            otherwise does Tx testing. This method must also be a member of the
            :class:`TestDynamicQueueConf` class.

    Returns:
        A method that sets up the environment, runs the decorated method, then re-enables all
        queues and validates they can still handle traffic.
    """

    def wrap(self: "TestDynamicQueueConf", is_rx_testing: bool) -> None:
        """Setup environment, run test function, then cleanup.

        Start a testpmd shell and stop ports for testing, then call the decorated function that
        performs the testing. After the decorated function is finished running its testing,
        start the stopped queues and send packets to validate that these ports can properly
        handle traffic after being started again.

        Args:
            self: Instance of :class:`TestDynamicQueueConf` `test_meth` belongs to.
            is_rx_testing: If :data:`True` then Rx queues will be the ones modified throughout
                the test, otherwise Tx queues will be modified.
        """
        port_id = self.rx_port_num if is_rx_testing else self.tx_port_num
        queues_to_config: set[int] = set()
        while len(queues_to_config) < self.num_ports_to_modify:
            queues_to_config.add(random.randint(1, self.number_of_queues - 1))
        unchanged_queues = set(range(self.number_of_queues)) - queues_to_config
        with TestPmd(
            port_topology=PortTopology.chained,
            rx_queues=self.number_of_queues,
            tx_queues=self.number_of_queues,
        ) as testpmd:
            for q in queues_to_config:
                testpmd.stop_port_queue(port_id, q, is_rx_testing)
            testpmd.set_forward_mode(SimpleForwardingModes.mac)

            test_meth(
                self,
                port_id,
                queues_to_config,
                unchanged_queues,
                testpmd,
                is_rx_testing,
            )

            for queue_id in queues_to_config:
                testpmd.start_port_queue(port_id, queue_id, is_rx_testing)

            testpmd.start()
            self.send_packets_with_different_addresses(self.number_of_packets_to_send)
            forwarding_stats = testpmd.stop()
            for queue_id in queues_to_config:
                self.verify(
                    self.port_queue_in_stats(port_id, is_rx_testing, queue_id, forwarding_stats),
                    f"Modified queue {queue_id} on port {port_id} failed to receive traffic after"
                    "being started again.",
                )

    return wrap


@requires_nic_capability(NicCapability.PHYSICAL_FUNCTION)
class TestDynamicQueueConf(TestSuite):
    """DPDK dynamic queue configuration test suite.

    Testing for the support of dynamic queue configuration is done by splitting testing by the type
    of queue (either Rx or Tx) and the type of testing (testing for stopping a port at runtime vs
    testing configuration changes at runtime). Testing is done by first stopping a finite number of
    port queues (3 is sufficient) and either modifying the configuration or sending packets to
    verify that the unmodified queues can handle traffic. Specifically, the following cases are
    tested:

    1. The application should be able to start the device with only some of the
       queues set up.
    2. The application should be able to reconfigure existing queues at runtime
       without calling dev_stop().
    """

    #:
    num_ports_to_modify: ClassVar[int] = 3
    #: Source IP address to use when sending packets.
    src_addr: ClassVar[str] = "192.168.0.1"
    #: Subnet to use for all of the destination addresses of the packets being sent.
    dst_address_subnet: ClassVar[str] = "192.168.1"
    #: ID of the port to modify Rx queues on.
    rx_port_num: ClassVar[int] = 0
    #: ID of the port to modify Tx queues on.
    tx_port_num: ClassVar[int] = 1
    #: Number of queues to start testpmd with. There will be the same number of Rx and Tx queues.
    #: 8 was chosen as a number that is low enough for most NICs to accommodate while also being
    #: enough to validate the usage of the queues.
    number_of_queues: ClassVar[int] = 8
    #: The number of packets to send while testing. The test calls for well over the ring size - 1
    #: packets in the modification test case and the only options for ring size are 256 or 512,
    #: therefore 1024 will be more than enough.
    number_of_packets_to_send: ClassVar[int] = 1024

    def send_packets_with_different_addresses(self, number_of_packets: int) -> None:
        """Send a set number of packets each with different dst addresses.

        Different destination addresses are required to ensure that each queue is used. If every
        packet had the same address, then they would all be processed by the same queue. Note that
        this means the current implementation of this method is limited to only work for up to 254
        queues. A smaller subnet would be required to handle an increased number of queues.

        Args:
            number_of_packets: The number of packets to generate and then send using the traffic
                generator.
        """
        packets_to_send = [
            Ether()
            / IP(src=self.src_addr, dst=f"{self.dst_address_subnet}.{(i % 254) + 1}")
            / Raw()
            for i in range(number_of_packets)
        ]
        self.send_packets(packets_to_send)

    def port_queue_in_stats(
        self, port_id: int, is_rx_queue: bool, queue_id: int, stats: str
    ) -> bool:
        """Verify if stats for a queue are in the provided output.

        Args:
            port_id: ID of the port that the queue resides on.
            is_rx_queue: Type of queue to scan for, if :data:`True` then search for an Rx queue,
                otherwise search for a Tx queue.
            queue_id: ID of the queue.
            stats: Testpmd forwarding statistics to scan for the given queue.

        Returns:
            If the queue appeared in the forwarding statistics.
        """
        type_of_queue = "RX" if is_rx_queue else "TX"
        return f"{type_of_queue} Port= {port_id}/Queue={queue_id:2d}" in stats

    @setup_and_teardown_test
    def modify_ring_size(
        self,
        port_id: int,
        queues_to_modify: MutableSet[int],
        unchanged_queues: MutableSet[int],
        testpmd: TestPmd,
        is_rx_testing: bool,
    ) -> None:
        """Verify ring size of port queues can be configured at runtime.

        Ring size of queues in `queues_to_modify` are set to 512 unless that is already their
        configured size, in which case they are instead set to 256. Queues in `queues_to_modify`
        are expected to already be stopped before calling this method. `testpmd` is also expected
        to already be started.

        Args:
            port_id: Port where the queues reside.
            queues_to_modify: IDs of stopped queues to configure in the test.
            unchanged_queues: IDs of running, unmodified queues.
            testpmd: Running interactive testpmd application.
            is_rx_testing: If :data:`True` Rx queues will be modified in the test, otherwise Tx
                queues will be modified.
        """
        for queue_id in queues_to_modify:
            curr_ring_size = testpmd.get_queue_ring_size(port_id, queue_id, is_rx_testing)
            new_ring_size = 256 if curr_ring_size == 512 else 512
            try:
                testpmd.set_queue_ring_size(
                    port_id, queue_id, new_ring_size, is_rx_testing, verify=True
                )
            # The testpmd method verifies that the modification worked, so we catch that error
            # and just re-raise it as a test case failure
            except InteractiveCommandExecutionError:
                self.verify(
                    False,
                    f"Failed to update the ring size of queue {queue_id} on port "
                    f"{port_id} at runtime",
                )

    @setup_and_teardown_test
    def stop_queues(
        self,
        port_id: int,
        queues_to_modify: MutableSet[int],
        unchanged_queues: MutableSet[int],
        testpmd: TestPmd,
        is_rx_testing: bool,
    ) -> None:
        """Verify stopped queues do not handle traffic and do not block traffic on other queues.

        Queues in `queues_to_modify` are expected to already be stopped before calling this method.
        `testpmd` is also expected to already be started.

        Args:
            port_id: Port where the queues reside.
            queues_to_modify: IDs of stopped queues to configure in the test.
            unchanged_queues: IDs of running, unmodified queues.
            testpmd: Running interactive testpmd application.
            is_rx_testing: If :data:`True` Rx queues will be modified in the test, otherwise Tx
                queues will be modified.
        """
        testpmd.start()
        self.send_packets_with_different_addresses(self.number_of_packets_to_send)
        forwarding_stats = testpmd.stop()

        # Checking that all unmodified queues handled some packets is important because this
        # test case checks for the absence of stopped queues to validate that they cannot
        # receive traffic. If there are some unchanged queues that also didn't receive traffic,
        # it means there could be another reason for the packets not transmitting and,
        # therefore, a false positive result.
        for unchanged_q_id in unchanged_queues:
            self.verify(
                self.port_queue_in_stats(port_id, is_rx_testing, unchanged_q_id, forwarding_stats),
                f"Queue {unchanged_q_id} failed to receive traffic.",
            )
        for stopped_q_id in queues_to_modify:
            self.verify(
                not self.port_queue_in_stats(
                    port_id, is_rx_testing, stopped_q_id, forwarding_stats
                ),
                f"Queue {stopped_q_id} should be stopped but still received traffic.",
            )

    @requires_nic_capability(NicCapability.RUNTIME_RX_QUEUE_SETUP)
    @func_test
    def rx_queue_stop(self) -> None:
        """Run method for stopping queues with flag for Rx testing set to :data:`True`."""
        self.stop_queues(True)

    @requires_nic_capability(NicCapability.RUNTIME_RX_QUEUE_SETUP)
    @func_test
    def rx_queue_configuration(self) -> None:
        """Run method for configuring queues with flag for Rx testing set to :data:`True`."""
        self.modify_ring_size(True)

    @requires_nic_capability(NicCapability.RUNTIME_TX_QUEUE_SETUP)
    @func_test
    def tx_queue_stop(self) -> None:
        """Run method for stopping queues with flag for Rx testing set to :data:`False`."""
        self.stop_queues(False)

    @requires_nic_capability(NicCapability.RUNTIME_TX_QUEUE_SETUP)
    @func_test
    def tx_queue_configuration(self) -> None:
        """Run method for configuring queues with flag for Rx testing set to :data:`False`."""
        self.modify_ring_size(False)
