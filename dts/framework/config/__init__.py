# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2021 Intel Corporation
# Copyright(c) 2022-2023 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.

"""
Yaml config parsing methods
"""

import json
import os.path
import pathlib
from dataclasses import dataclass
from enum import Enum, auto, unique
from typing import Any, TypedDict

import warlock  # type: ignore
import yaml

from framework.settings import SETTINGS


class StrEnum(Enum):
    @staticmethod
    def _generate_next_value_(
        name: str, start: int, count: int, last_values: object
    ) -> str:
        return name


@unique
class Architecture(StrEnum):
    i686 = auto()
    x86_64 = auto()
    x86_32 = auto()
    arm64 = auto()
    ppc64le = auto()


@unique
class OS(StrEnum):
    linux = auto()
    freebsd = auto()
    windows = auto()


@unique
class CPUType(StrEnum):
    native = auto()
    armv8a = auto()
    dpaa2 = auto()
    thunderx = auto()
    xgene1 = auto()


@unique
class Compiler(StrEnum):
    gcc = auto()
    clang = auto()
    icc = auto()
    msvc = auto()


# Slots enables some optimizations, by pre-allocating space for the defined
# attributes in the underlying data structure.
#
# Frozen makes the object immutable. This enables further optimizations,
# and makes it thread safe should we every want to move in that direction.
@dataclass(slots=True, frozen=True)
class HugepageConfiguration:
    amount: int
    force_first_numa: bool


@dataclass(slots=True, frozen=True)
class NodeConfiguration:
    name: str
    hostname: str
    user: str
    password: str | None
    arch: Architecture
    os: OS
    lcores: str
    use_first_core: bool
    memory_channels: int
    hugepages: HugepageConfiguration | None

    @staticmethod
    def from_dict(d: dict) -> "NodeConfiguration":
        hugepage_config = d.get("hugepages")
        if hugepage_config:
            if "force_first_numa" not in hugepage_config:
                hugepage_config["force_first_numa"] = False
            hugepage_config = HugepageConfiguration(**hugepage_config)

        return NodeConfiguration(
            name=d["name"],
            hostname=d["hostname"],
            user=d["user"],
            password=d.get("password"),
            arch=Architecture(d["arch"]),
            os=OS(d["os"]),
            lcores=d.get("lcores", "1"),
            use_first_core=d.get("use_first_core", False),
            memory_channels=d.get("memory_channels", 1),
            hugepages=hugepage_config,
        )


@dataclass(slots=True, frozen=True)
class BuildTargetConfiguration:
    arch: Architecture
    os: OS
    cpu: CPUType
    compiler: Compiler
    compiler_wrapper: str
    name: str

    @staticmethod
    def from_dict(d: dict) -> "BuildTargetConfiguration":
        return BuildTargetConfiguration(
            arch=Architecture(d["arch"]),
            os=OS(d["os"]),
            cpu=CPUType(d["cpu"]),
            compiler=Compiler(d["compiler"]),
            compiler_wrapper=d.get("compiler_wrapper", ""),
            name=f"{d['arch']}-{d['os']}-{d['cpu']}-{d['compiler']}",
        )


class TestSuiteConfigDict(TypedDict):
    suite: str
    cases: list[str]


@dataclass(slots=True, frozen=True)
class TestSuiteConfig:
    test_suite: str
    test_cases: list[str]

    @staticmethod
    def from_dict(
        entry: str | TestSuiteConfigDict,
    ) -> "TestSuiteConfig":
        if isinstance(entry, str):
            return TestSuiteConfig(test_suite=entry, test_cases=[])
        elif isinstance(entry, dict):
            return TestSuiteConfig(test_suite=entry["suite"], test_cases=entry["cases"])
        else:
            raise TypeError(f"{type(entry)} is not valid for a test suite config.")


@dataclass(slots=True, frozen=True)
class ExecutionConfiguration:
    build_targets: list[BuildTargetConfiguration]
    perf: bool
    func: bool
    test_suites: list[TestSuiteConfig]
    system_under_test: NodeConfiguration

    @staticmethod
    def from_dict(d: dict, node_map: dict) -> "ExecutionConfiguration":
        build_targets: list[BuildTargetConfiguration] = list(
            map(BuildTargetConfiguration.from_dict, d["build_targets"])
        )
        test_suites: list[TestSuiteConfig] = list(
            map(TestSuiteConfig.from_dict, d["test_suites"])
        )
        sut_name = d["system_under_test"]
        assert sut_name in node_map, f"Unknown SUT {sut_name} in execution {d}"

        return ExecutionConfiguration(
            build_targets=build_targets,
            perf=d["perf"],
            func=d["func"],
            test_suites=test_suites,
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
