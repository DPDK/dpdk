# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.

from framework.config import ScapyTrafficGeneratorConfig, TrafficGeneratorType
from framework.exception import ConfigurationError
from framework.testbed_model.node import Node

from .capturing_traffic_generator import CapturingTrafficGenerator
from .scapy import ScapyTrafficGenerator


def create_traffic_generator(
    tg_node: Node, traffic_generator_config: ScapyTrafficGeneratorConfig
) -> CapturingTrafficGenerator:
    """A factory function for creating traffic generator object from user config."""

    match traffic_generator_config.traffic_generator_type:
        case TrafficGeneratorType.SCAPY:
            return ScapyTrafficGenerator(tg_node, traffic_generator_config)
        case _:
            raise ConfigurationError(
                "Unknown traffic generator: {traffic_generator_config.traffic_generator_type}"
            )
