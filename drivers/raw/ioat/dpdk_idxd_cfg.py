#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2020 Intel Corporation

"""
Configure an entire Intel DSA instance, using idxd kernel driver, for DPDK use
"""

import sys
import argparse
import os
import os.path


class SysfsDir:
    "Used to read/write paths in a sysfs directory"
    def __init__(self, path):
        self.path = path

    def read_int(self, filename):
        "Return a value from sysfs file"
        with open(os.path.join(self.path, filename)) as f:
            return int(f.readline())

    def write_values(self, values):
        "write dictionary, where key is filename and value is value to write"
        for filename, contents in values.items():
            with open(os.path.join(self.path, filename), "w") as f:
                f.write(str(contents))


def configure_dsa(dsa_id, queues):
    "Configure the DSA instance with appropriate number of queues"
    dsa_dir = SysfsDir(f"/sys/bus/dsa/devices/dsa{dsa_id}")
    drv_dir = SysfsDir("/sys/bus/dsa/drivers/dsa")

    max_groups = dsa_dir.read_int("max_groups")
    max_engines = dsa_dir.read_int("max_engines")
    max_queues = dsa_dir.read_int("max_work_queues")
    max_work_queues_size = dsa_dir.read_int("max_work_queues_size")

    nb_queues = min(queues, max_queues)
    if queues > nb_queues:
        print(f"Setting number of queues to max supported value: {max_queues}")

    # we want one engine per group, and no more engines than queues
    nb_groups = min(max_engines, max_groups, nb_queues)
    for grp in range(nb_groups):
        dsa_dir.write_values({f"engine{dsa_id}.{grp}/group_id": grp})

    # configure each queue
    for q in range(nb_queues):
        wq_dir = SysfsDir(os.path.join(dsa_dir.path, f"wq{dsa_id}.{q}"))
        wq_dir.write_values({"group_id": q % nb_groups,
                             "type": "user",
                             "mode": "dedicated",
                             "name": f"dpdk_wq{dsa_id}.{q}",
                             "priority": 1,
                             "size": int(max_work_queues_size / nb_queues)})

    # enable device and then queues
    drv_dir.write_values({"bind": f"dsa{dsa_id}"})
    for q in range(nb_queues):
        drv_dir.write_values({"bind": f"wq{dsa_id}.{q}"})


def main(args):
    "Main function, does arg parsing and calls config function"
    arg_p = argparse.ArgumentParser(
        description="Configure whole DSA device instance for DPDK use")
    arg_p.add_argument('dsa_id', type=int, help="DSA instance number")
    arg_p.add_argument('-q', metavar='queues', type=int, default=255,
                       help="Number of queues to set up")
    parsed_args = arg_p.parse_args(args[1:])
    configure_dsa(parsed_args.dsa_id, parsed_args.q)


if __name__ == "__main__":
    main(sys.argv)
