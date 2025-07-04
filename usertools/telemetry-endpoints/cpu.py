# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023 Robin Jarry

CPU_TOTAL = "total_cycles"
CPU_BUSY = "busy_cycles"


def info() -> "dict[Name, tuple[Description, Type]]":
    return {
        CPU_TOTAL: ("Total number of CPU cycles.", "counter"),
        CPU_BUSY: ("Number of busy CPU cycles.", "counter"),
    }


def metrics(sock: "TelemetrySocket") -> "list[tuple[Name, Value, Labels]]":
    out = []
    for lcore_id in sock.cmd("/eal/lcore/list"):
        lcore = sock.cmd("/eal/lcore/info", lcore_id)
        cpu = ",".join(str(c) for c in lcore.get("cpuset", []))
        total = lcore.get("total_cycles")
        busy = lcore.get("busy_cycles", 0)
        if not (cpu and total):
            continue
        labels = {"cpu": cpu, "numa": lcore.get("socket", 0)}
        out += [
            (CPU_TOTAL, total, labels),
            (CPU_BUSY, busy, labels),
        ]
    return out
