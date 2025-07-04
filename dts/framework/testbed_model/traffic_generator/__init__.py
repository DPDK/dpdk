# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.

"""DTS traffic generators.

A traffic generator is capable of generating traffic and then monitor returning traffic.
All traffic generators must count the number of received packets. Some may additionally capture
individual packets.

A traffic generator may be software running on generic hardware or it could be specialized hardware.

The traffic generators that only count the number of received packets are suitable only for
performance testing. In functional testing, we need to be able to dissect each arrived packet
and a capturing traffic generator is required.
"""

from framework.config.test_run import ScapyTrafficGeneratorConfig, TrafficGeneratorConfig
from framework.exception import ConfigurationError
from framework.testbed_model.node import Node

from .capturing_traffic_generator import CapturingTrafficGenerator
from .scapy import ScapyTrafficGenerator


def create_traffic_generator(
    traffic_generator_config: TrafficGeneratorConfig, node: Node
) -> CapturingTrafficGenerator:
    """The factory function for creating traffic generator objects from the test run configuration.

    Args:
        traffic_generator_config: The traffic generator config.
        node: The node where the created traffic generator will be running.

    Returns:
        A traffic generator capable of capturing received packets.

    Raises:
        ConfigurationError: If an unknown traffic generator has been setup.
    """
    match traffic_generator_config:
        case ScapyTrafficGeneratorConfig():
            return ScapyTrafficGenerator(node, traffic_generator_config, privileged=True)
        case _:
            raise ConfigurationError(f"Unknown traffic generator: {traffic_generator_config.type}")
