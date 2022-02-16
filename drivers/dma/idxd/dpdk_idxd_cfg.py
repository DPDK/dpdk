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


def get_drv_dir(dtype):
    "Get the sysfs path for the driver, either 'idxd' or 'user'"
    drv_dir = "/sys/bus/dsa/drivers/" + dtype
    if not os.path.exists(drv_dir):
        return "/sys/bus/dsa/drivers/dsa"
    return drv_dir


def reset_device(dsa_id):
    "Reset the DSA device and all its queues"
    drv_dir = SysfsDir(get_drv_dir("idxd"))
    drv_dir.write_values({"unbind": f"dsa{dsa_id}"})


def get_pci_dir(pci):
    "Search for the sysfs directory of the PCI device"
    base_dir = '/sys/bus/pci/devices/'
    for path, dirs, files in os.walk(base_dir):
        for dir in dirs:
            if pci in dir:
                return os.path.join(base_dir, dir)
    sys.exit(f"Could not find sysfs directory for device {pci}")


def get_dsa_id(pci):
    "Get the DSA instance ID using the PCI address of the device"
    pci_dir = get_pci_dir(pci)
    for path, dirs, files in os.walk(pci_dir):
        for dir in dirs:
            if dir.startswith('dsa') and 'wq' not in dir:
                return int(dir[3:])
    sys.exit(f"Could not get device ID for device {pci}")


def configure_dsa(dsa_id, queues, prefix):
    "Configure the DSA instance with appropriate number of queues"
    dsa_dir = SysfsDir(f"/sys/bus/dsa/devices/dsa{dsa_id}")

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
                             "name": f"{prefix}_wq{dsa_id}.{q}",
                             "priority": 1,
                             "max_batch_size": 1024,
                             "size": int(max_work_queues_size / nb_queues)})

    # enable device and then queues
    idxd_dir = SysfsDir(get_drv_dir("idxd"))
    idxd_dir.write_values({"bind": f"dsa{dsa_id}"})

    user_dir = SysfsDir(get_drv_dir("user"))
    for q in range(nb_queues):
        user_dir.write_values({"bind": f"wq{dsa_id}.{q}"})


def main(args):
    "Main function, does arg parsing and calls config function"
    arg_p = argparse.ArgumentParser(
        description="Configure whole DSA device instance for DPDK use")
    arg_p.add_argument('dsa_id',
                       help="Specify DSA instance either via DSA instance number or PCI address")
    arg_p.add_argument('-q', metavar='queues', type=int, default=255,
                       help="Number of queues to set up")
    arg_p.add_argument('--name-prefix', metavar='prefix', dest='prefix',
                       default="dpdk",
                       help="Prefix for workqueue name to mark for DPDK use [default: 'dpdk']")
    arg_p.add_argument('--reset', action='store_true',
                       help="Reset DSA device and its queues")
    parsed_args = arg_p.parse_args(args[1:])

    dsa_id = parsed_args.dsa_id
    dsa_id = get_dsa_id(dsa_id) if ':' in dsa_id else dsa_id
    if parsed_args.reset:
        reset_device(dsa_id)
    else:
        configure_dsa(dsa_id, parsed_args.q, parsed_args.prefix)


if __name__ == "__main__":
    main(sys.argv)
