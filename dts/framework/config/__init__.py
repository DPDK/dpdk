# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2021 Intel Corporation
# Copyright(c) 2022 University of New Hampshire

"""
Generic port and topology nodes configuration file load function
"""

import json
import os.path
import pathlib
from dataclasses import dataclass
from typing import Any

import warlock  # type: ignore
import yaml

from framework.settings import SETTINGS


# Slots enables some optimizations, by pre-allocating space for the defined
# attributes in the underlying data structure.
#
# Frozen makes the object immutable. This enables further optimizations,
# and makes it thread safe should we every want to move in that direction.
@dataclass(slots=True, frozen=True)
class NodeConfiguration:
    name: str
    hostname: str
    user: str
    password: str | None

    @staticmethod
    def from_dict(d: dict) -> "NodeConfiguration":
        return NodeConfiguration(
            name=d["name"],
            hostname=d["hostname"],
            user=d["user"],
            password=d.get("password"),
        )


@dataclass(slots=True, frozen=True)
class ExecutionConfiguration:
    system_under_test: NodeConfiguration

    @staticmethod
    def from_dict(d: dict, node_map: dict) -> "ExecutionConfiguration":
        sut_name = d["system_under_test"]
        assert sut_name in node_map, f"Unknown SUT {sut_name} in execution {d}"

        return ExecutionConfiguration(
            system_under_test=node_map[sut_name],
        )


@dataclass(slots=True, frozen=True)
class Configuration:
    executions: list[ExecutionConfiguration]

    @staticmethod
    def from_dict(d: dict) -> "Configuration":
        nodes: list[NodeConfiguration] = list(
            map(NodeConfiguration.from_dict, d["nodes"])
        )
        assert len(nodes) > 0, "There must be a node to test"

        node_map = {node.name: node for node in nodes}
        assert len(nodes) == len(node_map), "Duplicate node names are not allowed"

        executions: list[ExecutionConfiguration] = list(
            map(
                ExecutionConfiguration.from_dict, d["executions"], [node_map for _ in d]
            )
        )

        return Configuration(executions=executions)


def load_config() -> Configuration:
    """
    Loads the configuration file and the configuration file schema,
    validates the configuration file, and creates a configuration object.
    """
    with open(SETTINGS.config_file_path, "r") as f:
        config_data = yaml.safe_load(f)

    schema_path = os.path.join(
        pathlib.Path(__file__).parent.resolve(), "conf_yaml_schema.json"
    )

    with open(schema_path, "r") as f:
        schema = json.load(f)
    config: dict[str, Any] = warlock.model_factory(schema, name="_Config")(config_data)
    config_obj: Configuration = Configuration.from_dict(dict(config))
    return config_obj


CONFIGURATION = load_config()
