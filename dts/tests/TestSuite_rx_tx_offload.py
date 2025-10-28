# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 University of New Hampshire

"""RX TX offload test suite.

Test the testpmd feature of configuring RX and TX offloads.
"""

from api.capabilities import (
    LinkTopology,
    NicCapability,
    requires_link_topology,
    requires_nic_capability,
)
from api.test import verify
from api.testpmd import TestPmd
from api.testpmd.types import (
    OffloadConfiguration,
    RxTxLiteralSwitch,
)
from framework.test_suite import TestSuite, func_test


@requires_link_topology(LinkTopology.ONE_LINK)
class TestRxTxOffload(TestSuite):
    """RX/TX offload test suite."""

    def _check_config(
        self,
        testpmd: TestPmd,
        port_id: int,
        port_offload: str | None,
        rxtx: RxTxLiteralSwitch,
        /,
        queue_offload: list[str | None] | None = None,
    ) -> bool:
        config: OffloadConfiguration = testpmd.get_offload_config(port_id, rxtx)
        if config.port_config.name != port_offload:
            return False

        if queue_offload:
            for i, q in enumerate(config.queue_configs):
                if q.name != queue_offload[i]:
                    return False
        return True

    def _set_all_queues_mbuf_fast_free(
        self,
        testpmd: TestPmd,
        port_id: int,
        on: bool,
        num_queues: int,
    ) -> None:
        for i in range(num_queues):
            testpmd.set_queue_mbuf_fast_free(port_id, on, i)

    @requires_nic_capability(NicCapability.PORT_TX_OFFLOAD_MBUF_FAST_FREE)
    @func_test
    def test_mbuf_fast_free_configuration_per_port(self) -> None:
        """Ensure mbuf_fast_free can be configured with testpmd per port.

        Steps:
            * Start up testpmd shell.
            * Toggle mbuf_fast_free off per port.
            * Toggle mbuf_fast_free on per port.

        Verify:
            * Mbuf_fast_free starts enabled.
            * Mbuf_fast_free can be configured off per port.
            * Mbuf_fast_free can be configured on per port.
        """
        with TestPmd() as testpmd:
            port_id = 0
            testpmd.start_all_ports()

            # Ensure MBUF_FAST_FREE is enabled by default and verify
            verify(
                self._check_config(testpmd, port_id, "MBUF_FAST_FREE", "tx"),
                "MBUF_FAST_FREE disabled on port start.",
            )
            # disable MBUF_FAST_FREE per port and verify
            testpmd.set_port_mbuf_fast_free(port_id, False)
            verify(
                self._check_config(testpmd, port_id, None, "tx"),
                "Failed to enable MBUF_FAST_FREE on port.",
            )
            # Enable MBUF_FAST_FREE per port and verify
            testpmd.set_port_mbuf_fast_free(port_id, True)
            verify(
                self._check_config(testpmd, port_id, "MBUF_FAST_FREE", "tx"),
                "Failed to disable MBUF_FAST_FREE on port.",
            )

    @requires_nic_capability(NicCapability.QUEUE_TX_OFFLOAD_MBUF_FAST_FREE)
    @func_test
    def test_mbuf_fast_free_configuration_per_queue(self) -> None:
        """Ensure mbuf_fast_free can be configured with testpmd.

        Steps:
            * Start up testpmd shell.
            * Toggle mbuf_fast_free off per queue.
            * Toggle mbuf_fast_free on per queue.

        Verify:
            * Mbuf_fast_free starts disabled.
            * Mbuf_fast_free can be configured off per queue.
            * Mbuf_fast_free can be configured on per queue.
        """
        with TestPmd() as testpmd:
            port_id = 0
            num_queues = 4
            queue_off: list[str | None] | None = [None] * num_queues
            queue_on: list[str | None] | None = ["MBUF_FAST_FREE"] * num_queues

            testpmd.set_ports_queues(num_queues)
            testpmd.start_all_ports()

            # Ensure mbuf_fast_free is enabled by default on port and queues
            verify(
                self._check_config(testpmd, port_id, "MBUF_FAST_FREE", "tx", queue_on),
                "MBUF_FAST_FREE disabled on queue start.",
            )
            # Disable mbuf_fast_free per queue and verify
            self._set_all_queues_mbuf_fast_free(testpmd, port_id, False, num_queues)
            verify(
                self._check_config(testpmd, port_id, "MBUF_FAST_FREE", "tx", queue_off),
                "Failed to disable MBUF_FAST_FREE on all queues.",
            )
            # Disable mbuf_fast_free per queue and verify
            self._set_all_queues_mbuf_fast_free(testpmd, port_id, True, num_queues)
            verify(
                self._check_config(testpmd, port_id, "MBUF_FAST_FREE", "tx", queue_on),
                "Failed to enable MBUF_FAST_FREE on all queues.",
            )
