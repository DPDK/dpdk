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
from enum import auto, unique
from typing import Any, TypedDict, Union

import warlock  # type: ignore[import]
import yaml

from framework.settings import SETTINGS
from framework.utils import StrEnum


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


@unique
class TrafficGeneratorType(StrEnum):
    SCAPY = auto()


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
class PortConfig:
    node: str
    pci: str
    os_driver_for_dpdk: str
    os_driver: str
    peer_node: str
    peer_pci: str

    @staticmethod
    def from_dict(node: str, d: dict) -> "PortConfig":
        return PortConfig(node=node, **d)


@dataclass(slots=True, frozen=True)
class TrafficGeneratorConfig:
    traffic_generator_type: TrafficGeneratorType

    @staticmethod
    def from_dict(d: dict):
        # This looks useless now, but is designed to allow expansion to traffic
        # generators that require more configuration later.
        match TrafficGeneratorType(d["type"]):
            case TrafficGeneratorType.SCAPY:
                return ScapyTrafficGeneratorConfig(
                    traffic_generator_type=TrafficGeneratorType.SCAPY
                )


@dataclass(slots=True, frozen=True)
class ScapyTrafficGeneratorConfig(TrafficGeneratorConfig):
    pass


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
    hugepages: HugepageConfiguration | None
    ports: list[PortConfig]

    @staticmethod
    def from_dict(d: dict) -> Union["SutNodeConfiguration", "TGNodeConfiguration"]:
        hugepage_config = d.get("hugepages")
        if hugepage_config:
            if "force_first_numa" not in hugepage_config:
                hugepage_config["force_first_numa"] = False
            hugepage_config = HugepageConfiguration(**hugepage_config)

        common_config = {
            "name": d["name"],
            "hostname": d["hostname"],
            "user": d["user"],
            "password": d.get("password"),
            "arch": Architecture(d["arch"]),
            "os": OS(d["os"]),
            "lcores": d.get("lcores", "1"),
            "use_first_core": d.get("use_first_core", False),
            "hugepages": hugepage_config,
            "ports": [PortConfig.from_dict(d["name"], port) for port in d["ports"]],
        }

        if "traffic_generator" in d:
            return TGNodeConfiguration(
                traffic_generator=TrafficGeneratorConfig.from_dict(
                    d["traffic_generator"]
                ),
                **common_config,
            )
        else:
            return SutNodeConfiguration(
                memory_channels=d.get("memory_channels", 1), **common_config
            )


@dataclass(slots=True, frozen=True)
class SutNodeConfiguration(NodeConfiguration):
    memory_channels: int


@dataclass(slots=True, frozen=True)
class TGNodeConfiguration(NodeConfiguration):
    traffic_generator: ScapyTrafficGeneratorConfig


@dataclass(slots=True, frozen=True)
class NodeInfo:
    """Class to hold important versions within the node.

    This class, unlike the NodeConfiguration class, cannot be generated at the start.
    This is because we need to initialize a connection with the node before we can
    collect the information needed in this class. Therefore, it cannot be a part of
    the configuration class above.
    """

    os_name: str
    os_version: str
    kernel_version: str


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


@dataclass(slots=True, frozen=True)
class BuildTargetInfo:
    """Class to hold important versions within the build target.

    This is very similar to the NodeInfo class, it just instead holds information
    for the build target.
    """

    dpdk_version: str
    compiler_version: str


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
    system_under_test_node: SutNodeConfiguration
    traffic_generator_node: TGNodeConfiguration
    vdevs: list[str]
    skip_smoke_tests: bool

    @staticmethod
    def from_dict(
        d: dict, node_map: dict[str, Union[SutNodeConfiguration | TGNodeConfiguration]]
    ) -> "ExecutionConfiguration":
        build_targets: list[BuildTargetConfiguration] = list(
            map(BuildTargetConfiguration.from_dict, d["build_targets"])
        )
        test_suites: list[TestSuiteConfig] = list(
            map(TestSuiteConfig.from_dict, d["test_suites"])
        )
        sut_name = d["system_under_test_node"]["node_name"]
        skip_smoke_tests = d.get("skip_smoke_tests", False)
        assert sut_name in node_map, f"Unknown SUT {sut_name} in execution {d}"
        system_under_test_node = node_map[sut_name]
        assert isinstance(
            system_under_test_node, SutNodeConfiguration
        ), f"Invalid SUT configuration {system_under_test_node}"

        tg_name = d["traffic_generator_node"]
        assert tg_name in node_map, f"Unknown TG {tg_name} in execution {d}"
        traffic_generator_node = node_map[tg_name]
        assert isinstance(
            traffic_generator_node, TGNodeConfiguration
        ), f"Invalid TG configuration {traffic_generator_node}"

        vdevs = (
            d["system_under_test_node"]["vdevs"]
            if "vdevs" in d["system_under_test_node"]
            else []
        )
        return ExecutionConfiguration(
            build_targets=build_targets,
            perf=d["perf"],
            func=d["func"],
            skip_smoke_tests=skip_smoke_tests,
            test_suites=test_suites,
            system_under_test_node=system_under_test_node,
            traffic_generator_node=traffic_generator_node,
            vdevs=vdevs,
        )


@dataclass(slots=True, frozen=True)
class Configuration:
    executions: list[ExecutionConfiguration]

    @staticmethod
    def from_dict(d: dict) -> "Configuration":
        nodes: list[Union[SutNodeConfiguration | TGNodeConfiguration]] = list(
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
