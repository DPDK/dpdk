# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023 Robin Jarry

RX_PACKETS = "rx_packets"
RX_BYTES = "rx_bytes"
RX_MISSED = "rx_missed"
RX_NOMBUF = "rx_nombuf"
RX_ERRORS = "rx_errors"
TX_PACKETS = "tx_packets"
TX_BYTES = "tx_bytes"
TX_ERRORS = "tx_errors"


def info() -> "dict[Name, tuple[Description, Type]]":
    return {
        RX_PACKETS: ("Number of successfully received packets.", "counter"),
        RX_BYTES: ("Number of successfully received bytes.", "counter"),
        RX_MISSED: (
            "Number of packets dropped by the HW because Rx queues are full.",
            "counter",
        ),
        RX_NOMBUF: ("Number of Rx mbuf allocation failures.", "counter"),
        RX_ERRORS: ("Number of erroneous received packets.", "counter"),
        TX_PACKETS: ("Number of successfully transmitted packets.", "counter"),
        TX_BYTES: ("Number of successfully transmitted bytes.", "counter"),
        TX_ERRORS: ("Number of packet transmission failures.", "counter"),
    }


def metrics(sock: "TelemetrySocket") -> "list[tuple[Name, Value, Labels]]":
    out = []
    for port_id in sock.cmd("/ethdev/list"):
        port = sock.cmd("/ethdev/info", port_id)
        stats = sock.cmd("/ethdev/stats", port_id)
        labels = {"port": port["name"]}
        out += [
            (RX_PACKETS, stats["ipackets"], labels),
            (RX_PACKETS, stats["ipackets"], labels),
            (RX_BYTES, stats["ibytes"], labels),
            (RX_MISSED, stats["imissed"], labels),
            (RX_NOMBUF, stats["rx_nombuf"], labels),
            (RX_ERRORS, stats["ierrors"], labels),
            (TX_PACKETS, stats["opackets"], labels),
            (TX_BYTES, stats["obytes"], labels),
            (TX_ERRORS, stats["oerrors"], labels),
        ]
    return out
