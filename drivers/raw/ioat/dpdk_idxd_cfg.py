#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2020 -2021 Intel Corporation

"""
Configure an entire Intel DSA instance, using idxd kernel driver, for DPDK use
"""

import sys
import argparse
import os
import os.path


class SysfsDir:
    """Used to read/write paths in a sysfs directory"""
    def __init__(self, path):
        self.path = path

    def read_int(self, filename):
        """Return an int value from sysfs file"""
        with open(os.path.join(self.path, filename)) as f:
            return int(f.readline())

    def write_values(self, values):
        """Write dictionary, where key is filename and value is value to write"""
        for filename, contents in values.items():
            with open(os.path.join(self.path, filename), "w") as f:
                f.write(str(contents))


def reset_device(dsa_id):
    """Reset the DSA device and all its queues"""
    drv_dir = SysfsDir("/sys/bus/dsa/drivers/dsa")
    drv_dir.write_values({"unbind": f"dsa{dsa_id}"})


def get_pci_dir(pci):
    """Search for the sysfs directory of the PCI device"""
    base_dir = '/sys/bus/pci/devices/'
    for path, dirs, files in os.walk(base_dir):
        for dir in dirs:
            if pci in dir:
                return os.path.join(base_dir, dir)
    sys.exit(f"Could not find sysfs directory for device {pci}")


def get_dsa_id(pci):
    """Get the DSA instance ID using the PCI address of the device"""
    pci_dir = get_pci_dir(pci)
    for path, dirs, files in os.walk(pci_dir):
        for dir in dirs:
            if dir.startswith('dsa') and 'wq' not in dir:
                return int(dir[3:])
    sys.exit(f"Could not get device ID for device {pci}")

def configure_dsa(dsa_id, queues_conf):
    """Configure the DSA instance with appropriate number of queues"""
    dsa_dir = SysfsDir(f"/sys/bus/dsa/devices/dsa{dsa_id}")
    drv_dir = SysfsDir("/sys/bus/dsa/drivers/dsa")

    max_groups = dsa_dir.read_int("max_groups")
    max_engines = dsa_dir.read_int("max_engines")
    max_queues = dsa_dir.read_int("max_work_queues")
    max_work_queues_size = dsa_dir.read_int("max_work_queues_size")
    pasid_enabled = dsa_dir.read_int("pasid_enabled")

    if queues_conf.mode == 'shared' and not pasid_enabled:
        sys.exit("""Could not configure Shared WQ - PASID is disabled.
        \rPlease make sure intel_iommu=on,sm_on in Grub GRUB_CMDLINE_LINUX""")

    nb_queues = min(queues_conf.q, max_queues)
    if queues_conf.q > nb_queues:
        print(f"Setting number of queues to max supported value: {max_queues}")

    available_max_work_queue_size = int(max_work_queues_size / nb_queues)
    if int(queues_conf.size) > available_max_work_queue_size:
        sys.exit(f"WQ size should be between 0 and {available_max_work_queue_size}")

    # we want one engine per group, and no more engines than queues 
    nb_groups = min(max_engines, max_groups, nb_queues)
    for grp in range(nb_groups):
        dsa_dir.write_values({f"engine{dsa_id}.{grp}/group_id": grp})

    # configure each queue
    for q in range(nb_queues):
        wq_dir = SysfsDir(os.path.join(dsa_dir.path, f"wq{dsa_id}.{q}"))

        # common settings for Shared WQ and Dedicated WQ
        values = {"group_id": q % nb_groups,
                  "type": queues_conf.type,
                  "name": f"{queues_conf.prefix}-{dsa_id}.{q}",
                  "priority": queues_conf.priority,
                  "size": queues_conf.size}
        if queues_conf.mode == 'shared':
            # threshold should be at least one less than wq size
            if int(queues_conf.threshold) >= values['size']:
                default_threshold = values['size'] - 1
                print(f"""Wrong threshold value.
                       \rValid range 0 to {default_threshold}.
                       \rSetting to {default_threshold}...\n""")
                values['threshold'] = default_threshold
            else:
                values['threshold'] = queues_conf.threshold
        else:
            values['mode'] = queues_conf.mode

        wq_dir.write_values(values)

    # enable device and then queues
    drv_dir.write_values({"bind": f"dsa{dsa_id}"})
    for q in range(nb_queues):
        drv_dir.write_values({"bind": f"wq{dsa_id}.{q}"})


def main(args):
    """Main function, does arg parsing and calls config function"""
    arg_p = argparse.ArgumentParser(
        description="Configure whole DSA device instance for DPDK use")
    arg_p.add_argument('dsa_id',
                       help="Specify DSA instance either via DSA instance number or PCI address")
    arg_p.add_argument('-q', metavar='queues', type=int, default=255,
                       help="Number of queues to set up [default: 255]")
    arg_p.add_argument('--name-prefix', metavar='prefix', dest='prefix',
                       default="wq",
                       help="Prefix for workqueue name [default: 'wq']")
    arg_p.add_argument('--mode', dest='mode', choices=['shared', 'dedicated'],
                       default="dedicated",
                       help="Work Queue mode [default: 'dedicated']")
    arg_p.add_argument('--size', metavar='wq-size', dest='size',
                       default=8,
                       help="WQ size [default: 8]")
    arg_p.add_argument('--threshold', metavar='threshold', dest='threshold',
                       default=5,
                       help="Threshold value for Shared WQ only.")
    arg_p.add_argument('--priority', dest='priority', choices=range(1,16),
                       default=1,
                       help="Work Queue priority [default: 1]")
    arg_p.add_argument('--type', dest='type', choices=['kernel', 'user'],
                       default='user',
                       help="Work Queue type [default: user]")
    arg_p.add_argument('--reset', action='store_true',
                       help="Reset DSA device and its queues")
    parsed_args = arg_p.parse_args(args[1:])

    dsa_id = parsed_args.dsa_id
    dsa_id = get_dsa_id(dsa_id) if ':' in dsa_id else dsa_id
    if parsed_args.reset:
        reset_device(dsa_id)
    else:
        configure_dsa(dsa_id, parsed_args)


if __name__ == "__main__":
    main(sys.argv)
