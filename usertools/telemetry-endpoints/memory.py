# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023 Robin Jarry

MEM_TOTAL = "total_bytes"
MEM_USED = "used_bytes"


def info() -> "dict[Name, tuple[Description, Type]]":
    return {
        MEM_TOTAL: ("The total size of reserved memory in bytes.", "gauge"),
        MEM_USED: ("The currently used memory in bytes.", "gauge"),
    }


def metrics(sock: "TelemetrySocket") -> "list[tuple[Name, Value, Labels]]":
    zones = {}
    used = 0
    for zone in sock.cmd("/eal/memzone_list") or []:
        z = sock.cmd("/eal/memzone_info", zone)
        start = int(z["Hugepage_base"], 16)
        end = start + (z["Hugepage_size"] * z["Hugepage_used"])
        used += z["Length"]
        for s, e in list(zones.items()):
            if s < start < e < end:
                zones[s] = end
                break
            if start < s < end < e:
                del zones[s]
                zones[start] = e
                break
        else:
            zones[start] = end

    return [
        (MEM_TOTAL, sum(end - start for (start, end) in zones.items()), {}),
        (MEM_USED, max(0, used), {}),
    ]
