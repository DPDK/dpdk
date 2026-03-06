# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 University of New Hampshire

"""Cryptodev types module.

Exposes types used in the Cryptodev API.
"""

from dataclasses import dataclass, field

from framework.parser import TextParser


@dataclass
class CryptodevResults(TextParser):
    """Base class for all cryptodev results."""

    buffer_size: int

    def __iter__(self):
        """Iteration method to parse result objects.

        Yields:
            tuple[str, int | float]: a field name and its value.
        """
        for field_name in self.__dataclass_fields__:
            yield field_name, getattr(self, field_name)


@dataclass
class ThroughputResults(CryptodevResults):
    """A parser for throughput test output."""

    #:
    lcore_id: int = field(metadata=TextParser.find_int(r"\s*(\d+)"))
    #: buffer size used in the run
    buffer_size: int = field(
        metadata=TextParser.find_int(r"\s+(?:\d+\s+)(\d+)"),
    )
    #: burst size used in the run
    burst_size: int = field(
        metadata=TextParser.find_int(r"\s+(?:\d+\s+){2}(\d+)"),
    )
    #: total packets enqueued
    enqueued: int = field(metadata=TextParser.find_int(r"\s+(?:\d+\s+){3}(\d+)"))
    #: total packets dequeued
    dequeued: int = field(metadata=TextParser.find_int(r"\s+(?:\d+\s+){4}(\d+)"))
    #: packets that failed enqueue
    failed_enqueue: int = field(metadata=TextParser.find_int(r"\s+(?:\d+\s+){5}(\d+)"))
    #: packets that failed dequeue
    failed_dequeue: int = field(metadata=TextParser.find_int(r"\s+(?:\d+\s+){6}(\d+)"))
    #: mega operations per second
    mops: float = field(metadata=TextParser.find_float(r"\s+(?:\d+\s+){7}([\d.]+)"))
    #: gigabits per second
    gbps: float = field(metadata=TextParser.find_float(r"\s+(?:\d+\s+){7}(?:[\d.]+\s+)([\d.]+)"))
    #: cpu cycles per buffer
    cycles_per_buffer: float = field(
        metadata=TextParser.find_float(r"\s+(?:\d+\s+){7}(?:[\d.]+\s+){2}([\d.]+)")
    )


@dataclass
class LatencyResults(CryptodevResults):
    """A parser for latency test output."""

    #: buffer size ran with app
    buffer_size: int = field(
        metadata=TextParser.find_int(r"Buf(?:.*\n\s+\d+\s+)?(?:fer size:\s+)?(\d+)"),
    )
    #: burst size ran with app
    burst_size: int = field(
        metadata=TextParser.find_int(rf"Burst(?:.*\n\s+\d+\s+){2}?(?: size:\s+)?(\d+)"),
    )
    #: total operations ran
    total_ops: int = field(metadata=TextParser.find_int(r"total operations:\s+(\d+)"))
    #: number of bursts
    num_of_bursts: int = field(metadata=TextParser.find_int(r"Number of bursts:\s+(\d+)"))
    #: minimum enqueued packets
    min_enqueued: int = field(metadata=TextParser.find_int(r"enqueued\s+(?:\d+\s+){2}(\d+)"))
    #: maximum enqueued packets
    max_enqueued: int = field(metadata=TextParser.find_int(r"enqueued\s+(?:\d+\s+){3}(\d+)"))
    #: average enqueued packets
    avg_enqueued: int = field(metadata=TextParser.find_int(r"enqueued\s+(?:\d+\s+)(\d+)"))
    #: total enqueued packets
    total_enqueued: int = field(metadata=TextParser.find_int(r"enqueued\s+(\d+)"))
    #: minimum dequeued packets
    min_dequeued: int = field(metadata=TextParser.find_int(r"dequeued\s+(?:\d+\s+){2}(\d+)"))
    #: maximum dequeued packets
    max_dequeued: int = field(metadata=TextParser.find_int(r"dequeued\s+(?:\d+\s+){3}(\d+)"))
    #: average dequeued packets
    avg_dequeued: int = field(metadata=TextParser.find_int(r"dequeued\s+(?:\d+\s+)(\d+)"))
    #: total dequeued packets
    total_dequeued: int = field(metadata=TextParser.find_int(r"dequeued\s+(\d+)"))
    #: minimum cycles per buffer
    min_cycles: float = field(metadata=TextParser.find_float(r"cycles\s+(?:[\d.]+\s+){3}([\d.]+)"))
    #: maximum cycles per buffer
    max_cycles: float = field(metadata=TextParser.find_float(r"cycles\s+(?:[\d.]+\s+){2}([\d.]+)"))
    #: average cycles per buffer
    avg_cycles: float = field(metadata=TextParser.find_float(r"cycles\s+(?:[\d.]+\s+)([\d.]+)"))
    #: total cycles per buffer
    total_cycles: float = field(metadata=TextParser.find_float(r"cycles\s+([\d.]+)"))
    #: mimum time in microseconds
    min_time_us: float = field(
        metadata=TextParser.find_float(r"time \[us\]\s+(?:[\d.]+\s+){3}([\d.]+)")
    )
    #: maximum time in microseconds
    max_time_us: float = field(
        metadata=TextParser.find_float(r"time \[us\]\s+(?:[\d.]+\s+){2}([\d.]+)")
    )
    #: average time in microseconds
    avg_time_us: float = field(
        metadata=TextParser.find_float(r"time \[us\]\s+(?:[\d.]+\s+)([\d.]+)")
    )
    #: total time in microseconds
    total_time_us: float = field(metadata=TextParser.find_float(r"time \[us\]\s+([\d.]+)"))


@dataclass
class PmdCyclecountResults(CryptodevResults):
    """A parser for PMD cycle count test output."""

    #:
    lcore_id: int = field(metadata=TextParser.find_int(r"lcore\s+(?:id.*\n\s+)?(\d+)"))
    #: buffer size used with app run
    buffer_size: int = field(
        metadata=TextParser.find_int(r"Buf(?:.*\n\s+(?:\d+\s+))?(?:fer size:\s+)?(\d+)"),
    )
    #: burst size used with app run
    burst_size: int = field(
        metadata=TextParser.find_int(r"Burst(?:.*\n\s+(?:\d+\s+){2})?(?: size:\s+)?(\d+)"),
    )
    #: packets enqueued
    enqueued: int = field(metadata=TextParser.find_int(r"Enqueued.*\n\s+(?:\d+\s+){3}(\d+)"))
    #: packets dequeued
    dequeued: int = field(metadata=TextParser.find_int(r"Dequeued.*\n\s+(?:\d+\s+){4}(\d+)"))
    #: number of enqueue packet retries
    enqueue_retries: int = field(
        metadata=TextParser.find_int(r"Enq Retries.*\n\s+(?:\d+\s+){5}(\d+)")
    )
    #: number of dequeue packet retries
    dequeue_retries: int = field(
        metadata=TextParser.find_int(r"Deq Retries.*\n\s+(?:\d+\s+){6}(\d+)")
    )
    #: number of cycles per operation
    cycles_per_operation: float = field(
        metadata=TextParser.find_float(r"Cycles/Op.*\n\s+(?:\d+\s+){7}([\d.]+)")
    )
    #: number of cycles per enqueue
    cycles_per_enqueue: float = field(
        metadata=TextParser.find_float(r"Cycles/Enq.*\n\s+(?:\d+\s+){7}(?:[\d.]+\s+)([\d.]+)")
    )
    #: number of cycles per dequeue
    cycles_per_dequeue: float = field(
        metadata=TextParser.find_float(r"Cycles/Deq.*\n\s+(?:\d+\s+){7}(?:[\d.]+\s+){2}([\d.]+)"),
    )


@dataclass
class VerifyResults(CryptodevResults):
    """A parser for verify test output."""

    #:
    lcore_id: int = field(metadata=TextParser.find_int(r"lcore\s+(?:id.*\n\s+)?(\d+)"))
    #: buffer size ran with app
    buffer_size: int = field(
        metadata=TextParser.find_int(r"Buf(?:.*\n\s+(?:\d+\s+))?(?:fer size:\s+)?(\d+)"),
    )
    #: burst size ran with app
    burst_size: int = field(
        metadata=TextParser.find_int(r"Burst(?:.*\n\s+(?:\d+\s+){2})?(?: size:\s+)?(\d+)"),
    )
    #: number of packets enqueued
    enqueued: int = field(metadata=TextParser.find_int(r"Enqueued.*\n\s+(?:\d+\s+){3}(\d+)"))
    #: number of packets dequeued
    dequeued: int = field(metadata=TextParser.find_int(r"Dequeued.*\n\s+(?:\d+\s+){4}(\d+)"))
    #: number of packets enqueue failed
    failed_enqueued: int = field(
        metadata=TextParser.find_int(r"Failed Enq.*\n\s+(?:\d+\s+){5}(\d+)")
    )
    #: number of packets dequeue failed
    failed_dequeued: int = field(
        metadata=TextParser.find_int(r"Failed Deq.*\n\s+(?:\d+\s+){6}(\d+)")
    )
    #: total number of failed operations
    failed_ops: int = field(metadata=TextParser.find_int(r"Failed Ops.*\n\s+(?:\d+\s+){7}(\d+)"))
