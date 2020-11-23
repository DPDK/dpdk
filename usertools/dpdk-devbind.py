#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
#

import sys
import os
import subprocess
import argparse
import platform

from glob import glob
from os.path import exists, basename
from os.path import join as path_join

# The PCI base class for all devices
network_class = {'Class': '02', 'Vendor': None, 'Device': None,
                 'SVendor': None, 'SDevice': None}
acceleration_class = {'Class': '12', 'Vendor': None, 'Device': None,
                      'SVendor': None, 'SDevice': None}
ifpga_class = {'Class': '12', 'Vendor': '8086', 'Device': '0b30',
               'SVendor': None, 'SDevice': None}
encryption_class = {'Class': '10', 'Vendor': None, 'Device': None,
                    'SVendor': None, 'SDevice': None}
intel_processor_class = {'Class': '0b', 'Vendor': '8086', 'Device': None,
                         'SVendor': None, 'SDevice': None}
cavium_sso = {'Class': '08', 'Vendor': '177d', 'Device': 'a04b,a04d',
              'SVendor': None, 'SDevice': None}
cavium_fpa = {'Class': '08', 'Vendor': '177d', 'Device': 'a053',
              'SVendor': None, 'SDevice': None}
cavium_pkx = {'Class': '08', 'Vendor': '177d', 'Device': 'a0dd,a049',
              'SVendor': None, 'SDevice': None}
cavium_tim = {'Class': '08', 'Vendor': '177d', 'Device': 'a051',
              'SVendor': None, 'SDevice': None}
cavium_zip = {'Class': '12', 'Vendor': '177d', 'Device': 'a037',
              'SVendor': None, 'SDevice': None}
avp_vnic = {'Class': '05', 'Vendor': '1af4', 'Device': '1110',
            'SVendor': None, 'SDevice': None}

octeontx2_sso = {'Class': '08', 'Vendor': '177d', 'Device': 'a0f9,a0fa',
                 'SVendor': None, 'SDevice': None}
octeontx2_npa = {'Class': '08', 'Vendor': '177d', 'Device': 'a0fb,a0fc',
                 'SVendor': None, 'SDevice': None}
octeontx2_dma = {'Class': '08', 'Vendor': '177d', 'Device': 'a081',
                 'SVendor': None, 'SDevice': None}
octeontx2_ree = {'Class': '08', 'Vendor': '177d', 'Device': 'a0f4',
                 'SVendor': None, 'SDevice': None}

intel_ioat_bdw = {'Class': '08', 'Vendor': '8086',
                  'Device': '6f20,6f21,6f22,6f23,6f24,6f25,6f26,6f27,6f2e,6f2f',
                  'SVendor': None, 'SDevice': None}
intel_ioat_skx = {'Class': '08', 'Vendor': '8086', 'Device': '2021',
                  'SVendor': None, 'SDevice': None}
intel_ioat_icx = {'Class': '08', 'Vendor': '8086', 'Device': '0b00',
                  'SVendor': None, 'SDevice': None}
intel_idxd_spr = {'Class': '08', 'Vendor': '8086', 'Device': '0b25',
                  'SVendor': None, 'SDevice': None}
intel_ntb_skx = {'Class': '06', 'Vendor': '8086', 'Device': '201c',
                 'SVendor': None, 'SDevice': None}
intel_ntb_icx = {'Class': '06', 'Vendor': '8086', 'Device': '347e',
                 'SVendor': None, 'SDevice': None}

network_devices = [network_class, cavium_pkx, avp_vnic, ifpga_class]
baseband_devices = [acceleration_class]
crypto_devices = [encryption_class, intel_processor_class]
eventdev_devices = [cavium_sso, cavium_tim, octeontx2_sso]
mempool_devices = [cavium_fpa, octeontx2_npa]
compress_devices = [cavium_zip]
regex_devices = [octeontx2_ree]
misc_devices = [intel_ioat_bdw, intel_ioat_skx, intel_ioat_icx, intel_idxd_spr,
                intel_ntb_skx, intel_ntb_icx,
                octeontx2_dma]

# global dict ethernet devices present. Dictionary indexed by PCI address.
# Each device within this is itself a dictionary of device properties
devices = {}
# list of supported DPDK drivers
dpdk_drivers = ["igb_uio", "vfio-pci", "uio_pci_generic"]
# list of currently loaded kernel modules
loaded_modules = None

# command-line arg flags
b_flag = None
status_flag = False
force_flag = False
args = []

# check if a specific kernel module is loaded
def module_is_loaded(module):
    global loaded_modules

    if module == 'vfio_pci':
        module = 'vfio-pci'

    if loaded_modules:
        return module in loaded_modules

    # Get list of sysfs modules (both built-in and dynamically loaded)
    sysfs_path = '/sys/module/'

    # Get the list of directories in sysfs_path
    sysfs_mods = [m for m in os.listdir(sysfs_path)
                  if os.path.isdir(os.path.join(sysfs_path, m))]

    # special case for vfio_pci (module is named vfio-pci,
    # but its .ko is named vfio_pci)
    sysfs_mods = [a if a != 'vfio_pci' else 'vfio-pci' for a in sysfs_mods]

    loaded_modules = sysfs_mods

    # add built-in modules as loaded
    release = platform.uname().release
    filename = os.path.join("/lib/modules/", release, "modules.builtin")
    if os.path.exists(filename):
        try:
            with open(filename) as f:
                loaded_modules += [os.path.splitext(os.path.basename(mod))[0] for mod in f]
        except IOError:
            print("Warning: cannot read list of built-in kernel modules")

    return module in loaded_modules


def check_modules():
    '''Checks that igb_uio is loaded'''
    global dpdk_drivers

    # list of supported modules
    mods = [{"Name": driver, "Found": False} for driver in dpdk_drivers]

    # first check if module is loaded
    for mod in mods:
        if module_is_loaded(mod["Name"]):
            mod["Found"] = True

    # check if we have at least one loaded module
    if True not in [mod["Found"] for mod in mods] and b_flag is not None:
        print("Warning: no supported DPDK kernel modules are loaded", file=sys.stderr)

    # change DPDK driver list to only contain drivers that are loaded
    dpdk_drivers = [mod["Name"] for mod in mods if mod["Found"]]


def has_driver(dev_id):
    '''return true if a device is assigned to a driver. False otherwise'''
    return "Driver_str" in devices[dev_id]


def get_pci_device_details(dev_id, probe_lspci):
    '''This function gets additional details for a PCI device'''
    device = {}

    if probe_lspci:
        extra_info = subprocess.check_output(["lspci", "-vmmks", dev_id]).splitlines()
        # parse lspci details
        for line in extra_info:
            if not line:
                continue
            name, value = line.decode("utf8").split("\t", 1)
            name = name.strip(":") + "_str"
            device[name] = value
    # check for a unix interface name
    device["Interface"] = ""
    for base, dirs, _ in os.walk("/sys/bus/pci/devices/%s/" % dev_id):
        if "net" in dirs:
            device["Interface"] = \
                ",".join(os.listdir(os.path.join(base, "net")))
            break
    # check if a port is used for ssh connection
    device["Ssh_if"] = False
    device["Active"] = ""

    return device

def clear_data():
    '''This function clears any old data'''
    global devices
    devices = {}

def get_device_details(devices_type):
    '''This function populates the "devices" dictionary. The keys used are
    the pci addresses (domain:bus:slot.func). The values are themselves
    dictionaries - one for each NIC.'''
    global devices
    global dpdk_drivers

    # first loop through and read details for all devices
    # request machine readable format, with numeric IDs and String
    dev = {}
    dev_lines = subprocess.check_output(["lspci", "-Dvmmnnk"]).splitlines()
    for dev_line in dev_lines:
        if not dev_line:
            if device_type_match(dev, devices_type):
                # Replace "Driver" with "Driver_str" to have consistency of
                # of dictionary key names
                if "Driver" in dev.keys():
                    dev["Driver_str"] = dev.pop("Driver")
                if "Module" in dev.keys():
                    dev["Module_str"] = dev.pop("Module")
                # use dict to make copy of dev
                devices[dev["Slot"]] = dict(dev)
            # Clear previous device's data
            dev = {}
        else:
            name, value = dev_line.decode("utf8").split("\t", 1)
            value_list = value.rsplit(' ', 1)
            if value_list:
                # String stored in <name>_str
                dev[name.rstrip(":") + '_str'] = value_list[0]
            # Numeric IDs
            dev[name.rstrip(":")] = value_list[len(value_list) - 1] \
                .rstrip("]").lstrip("[")

    if devices_type == network_devices:
        # check what is the interface if any for an ssh connection if
        # any to this host, so we can mark it later.
        ssh_if = []
        route = subprocess.check_output(["ip", "-o", "route"])
        # filter out all lines for 169.254 routes
        route = "\n".join(filter(lambda ln: not ln.startswith("169.254"),
                                 route.decode().splitlines()))
        rt_info = route.split()
        for i in range(len(rt_info) - 1):
            if rt_info[i] == "dev":
                ssh_if.append(rt_info[i+1])

    # based on the basic info, get extended text details
    for d in devices.keys():
        if not device_type_match(devices[d], devices_type):
            continue

        # get additional info and add it to existing data
        devices[d] = devices[d].copy()
        # No need to probe lspci
        devices[d].update(get_pci_device_details(d, False).items())

        if devices_type == network_devices:
            for _if in ssh_if:
                if _if in devices[d]["Interface"].split(","):
                    devices[d]["Ssh_if"] = True
                    devices[d]["Active"] = "*Active*"
                    break

        # add igb_uio to list of supporting modules if needed
        if "Module_str" in devices[d]:
            for driver in dpdk_drivers:
                if driver not in devices[d]["Module_str"]:
                    devices[d]["Module_str"] = \
                        devices[d]["Module_str"] + ",%s" % driver
        else:
            devices[d]["Module_str"] = ",".join(dpdk_drivers)

        # make sure the driver and module strings do not have any duplicates
        if has_driver(d):
            modules = devices[d]["Module_str"].split(",")
            if devices[d]["Driver_str"] in modules:
                modules.remove(devices[d]["Driver_str"])
                devices[d]["Module_str"] = ",".join(modules)


def device_type_match(dev, devices_type):
    for i in range(len(devices_type)):
        param_count = len(
            [x for x in devices_type[i].values() if x is not None])
        match_count = 0
        if dev["Class"][0:2] == devices_type[i]["Class"]:
            match_count = match_count + 1
            for key in devices_type[i].keys():
                if key != 'Class' and devices_type[i][key]:
                    value_list = devices_type[i][key].split(',')
                    for value in value_list:
                        if value.strip(' ') == dev[key]:
                            match_count = match_count + 1
            # count must be the number of non None parameters to match
            if match_count == param_count:
                return True
    return False

def dev_id_from_dev_name(dev_name):
    '''Take a device "name" - a string passed in by user to identify a NIC
    device, and determine the device id - i.e. the domain:bus:slot.func - for
    it, which can then be used to index into the devices array'''

    # check if it's already a suitable index
    if dev_name in devices:
        return dev_name
    # check if it's an index just missing the domain part
    if "0000:" + dev_name in devices:
        return "0000:" + dev_name

    # check if it's an interface name, e.g. eth1
    for d in devices.keys():
        if dev_name in devices[d]["Interface"].split(","):
            return devices[d]["Slot"]
    # if nothing else matches - error
    raise ValueError("Unknown device: %s. "
                     "Please specify device in \"bus:slot.func\" format" % dev_name)


def unbind_one(dev_id, force):
    '''Unbind the device identified by "dev_id" from its current driver'''
    dev = devices[dev_id]
    if not has_driver(dev_id):
        print("Notice: %s %s %s is not currently managed by any driver" %
              (dev["Slot"], dev["Device_str"], dev["Interface"]), file=sys.stderr)
        return

    # prevent us disconnecting ourselves
    if dev["Ssh_if"] and not force:
        print("Warning: routing table indicates that interface %s is active. "
              "Skipping unbind" % dev_id, file=sys.stderr)
        return

    # write to /sys to unbind
    filename = "/sys/bus/pci/drivers/%s/unbind" % dev["Driver_str"]
    try:
        f = open(filename, "a")
    except:
        sys.exit("Error: unbind failed for %s - Cannot open %s" %
                 (dev_id, filename))
    f.write(dev_id)
    f.close()


def bind_one(dev_id, driver, force):
    '''Bind the device given by "dev_id" to the driver "driver". If the device
    is already bound to a different driver, it will be unbound first'''
    dev = devices[dev_id]
    saved_driver = None  # used to rollback any unbind in case of failure

    # prevent disconnection of our ssh session
    if dev["Ssh_if"] and not force:
        print("Warning: routing table indicates that interface %s is active. "
              "Not modifying" % dev_id, file=sys.stderr)
        return

    # unbind any existing drivers we don't want
    if has_driver(dev_id):
        if dev["Driver_str"] == driver:
            print("Notice: %s already bound to driver %s, skipping" %
                  (dev_id, driver), file=sys.stderr)
            return
        saved_driver = dev["Driver_str"]
        unbind_one(dev_id, force)
        dev["Driver_str"] = ""  # clear driver string

    # For kernels >= 3.15 driver_override can be used to specify the driver
    # for a device rather than relying on the driver to provide a positive
    # match of the device.  The existing process of looking up
    # the vendor and device ID, adding them to the driver new_id,
    # will erroneously bind other devices too which has the additional burden
    # of unbinding those devices
    if driver in dpdk_drivers:
        filename = "/sys/bus/pci/devices/%s/driver_override" % dev_id
        if exists(filename):
            try:
                f = open(filename, "w")
            except:
                print("Error: bind failed for %s - Cannot open %s"
                      % (dev_id, filename), file=sys.stderr)
                return
            try:
                f.write("%s" % driver)
                f.close()
            except:
                print("Error: bind failed for %s - Cannot write driver %s to "
                      "PCI ID " % (dev_id, driver), file=sys.stderr)
                return
        # For kernels < 3.15 use new_id to add PCI id's to the driver
        else:
            filename = "/sys/bus/pci/drivers/%s/new_id" % driver
            try:
                f = open(filename, "w")
            except:
                print("Error: bind failed for %s - Cannot open %s"
                      % (dev_id, filename), file=sys.stderr)
                return
            try:
                # Convert Device and Vendor Id to int to write to new_id
                f.write("%04x %04x" % (int(dev["Vendor"], 16),
                                       int(dev["Device"], 16)))
                f.close()
            except:
                print("Error: bind failed for %s - Cannot write new PCI ID to "
                      "driver %s" % (dev_id, driver), file=sys.stderr)
                return

    # do the bind by writing to /sys
    filename = "/sys/bus/pci/drivers/%s/bind" % driver
    try:
        f = open(filename, "a")
    except:
        print("Error: bind failed for %s - Cannot open %s"
              % (dev_id, filename), file=sys.stderr)
        if saved_driver is not None:  # restore any previous driver
            bind_one(dev_id, saved_driver, force)
        return
    try:
        f.write(dev_id)
        f.close()
    except:
        # for some reason, closing dev_id after adding a new PCI ID to new_id
        # results in IOError. however, if the device was successfully bound,
        # we don't care for any errors and can safely ignore IOError
        tmp = get_pci_device_details(dev_id, True)
        if "Driver_str" in tmp and tmp["Driver_str"] == driver:
            return
        print("Error: bind failed for %s - Cannot bind to driver %s"
              % (dev_id, driver), file=sys.stderr)
        if saved_driver is not None:  # restore any previous driver
            bind_one(dev_id, saved_driver, force)
        return

    # For kernels > 3.15 driver_override is used to bind a device to a driver.
    # Before unbinding it, overwrite driver_override with empty string so that
    # the device can be bound to any other driver
    filename = "/sys/bus/pci/devices/%s/driver_override" % dev_id
    if exists(filename):
        try:
            f = open(filename, "w")
        except:
            sys.exit("Error: unbind failed for %s - Cannot open %s"
                     % (dev_id, filename))
        try:
            f.write("\00")
            f.close()
        except:
            sys.exit("Error: unbind failed for %s - Cannot open %s"
                     % (dev_id, filename))


def unbind_all(dev_list, force=False):
    """Unbind method, takes a list of device locations"""

    if dev_list[0] == "dpdk":
        for d in devices.keys():
            if "Driver_str" in devices[d]:
                if devices[d]["Driver_str"] in dpdk_drivers:
                    unbind_one(devices[d]["Slot"], force)
        return

    try:
        dev_list = map(dev_id_from_dev_name, dev_list)
    except ValueError as ex:
        print(ex)
        sys.exit(1)

    for d in dev_list:
        unbind_one(d, force)


def bind_all(dev_list, driver, force=False):
    """Bind method, takes a list of device locations"""
    global devices

    # a common user error is to forget to specify the driver the devices need to
    # be bound to. check if the driver is a valid device, and if it is, show
    # a meaningful error.
    try:
        dev_id_from_dev_name(driver)
        # if we've made it this far, this means that the "driver" was a valid
        # device string, so it's probably not a valid driver name.
        sys.exit("Error: Driver '%s' does not look like a valid driver. " \
                 "Did you forget to specify the driver to bind devices to?" % driver)
    except ValueError:
        # driver generated error - it's not a valid device ID, so all is well
        pass

    # check if we're attempting to bind to a driver that isn't loaded
    if not module_is_loaded(driver.replace('-', '_')):
        sys.exit("Error: Driver '%s' is not loaded." % driver)

    try:
        dev_list = map(dev_id_from_dev_name, dev_list)
    except ValueError as ex:
        sys.exit(ex)

    for d in dev_list:
        bind_one(d, driver, force)

    # For kernels < 3.15 when binding devices to a generic driver
    # (i.e. one that doesn't have a PCI ID table) using new_id, some devices
    # that are not bound to any other driver could be bound even if no one has
    # asked them to. hence, we check the list of drivers again, and see if
    # some of the previously-unbound devices were erroneously bound.
    if not exists("/sys/bus/pci/devices/%s/driver_override" % d):
        for d in devices.keys():
            # skip devices that were already bound or that we know should be bound
            if "Driver_str" in devices[d] or d in dev_list:
                continue

            # update information about this device
            devices[d] = dict(devices[d].items() +
                              get_pci_device_details(d, True).items())

            # check if updated information indicates that the device was bound
            if "Driver_str" in devices[d]:
                unbind_one(d, force)


def display_devices(title, dev_list, extra_params=None):
    '''Displays to the user the details of a list of devices given in
    "dev_list". The "extra_params" parameter, if given, should contain a string
     with %()s fields in it for replacement by the named fields in each
     device's dictionary.'''
    strings = []  # this holds the strings to print. We sort before printing
    print("\n%s" % title)
    print("="*len(title))
    if not dev_list:
        strings.append("<none>")
    else:
        for dev in dev_list:
            if extra_params is not None:
                strings.append("%s '%s %s' %s" % (dev["Slot"],
                                                  dev["Device_str"],
                                                  dev["Device"],
                                                  extra_params % dev))
            else:
                strings.append("%s '%s'" % (dev["Slot"], dev["Device_str"]))
    # sort before printing, so that the entries appear in PCI order
    strings.sort()
    print("\n".join(strings))  # print one per line

def show_device_status(devices_type, device_name, if_field=False):
    global dpdk_drivers
    kernel_drv = []
    dpdk_drv = []
    no_drv = []

    # split our list of network devices into the three categories above
    for d in devices.keys():
        if device_type_match(devices[d], devices_type):
            if not has_driver(d):
                no_drv.append(devices[d])
                continue
            if devices[d]["Driver_str"] in dpdk_drivers:
                dpdk_drv.append(devices[d])
            else:
                kernel_drv.append(devices[d])

    n_devs = len(dpdk_drv) + len(kernel_drv) + len(no_drv)

    # don't bother displaying anything if there are no devices
    if n_devs == 0:
        msg = "No '%s' devices detected" % device_name
        print("")
        print(msg)
        print("".join('=' * len(msg)))
        return

    # print each category separately, so we can clearly see what's used by DPDK
    if dpdk_drv:
        display_devices("%s devices using DPDK-compatible driver" % device_name,
                        dpdk_drv, "drv=%(Driver_str)s unused=%(Module_str)s")
    if kernel_drv:
        if_text = ""
        if if_field:
            if_text = "if=%(Interface)s "
        display_devices("%s devices using kernel driver" % device_name, kernel_drv,
                        if_text + "drv=%(Driver_str)s "
                        "unused=%(Module_str)s %(Active)s")
    if no_drv:
        display_devices("Other %s devices" % device_name, no_drv,
                        "unused=%(Module_str)s")

def show_status():
    '''Function called when the script is passed the "--status" option.
    Displays to the user what devices are bound to the igb_uio driver, the
    kernel driver or to no driver'''

    if status_dev in ["net", "all"]:
        show_device_status(network_devices, "Network", if_field=True)

    if status_dev in ["baseband", "all"]:
        show_device_status(baseband_devices, "Baseband")

    if status_dev in ["crypto", "all"]:
        show_device_status(crypto_devices, "Crypto")

    if status_dev in ["event", "all"]:
        show_device_status(eventdev_devices, "Eventdev")

    if status_dev in ["mempool", "all"]:
        show_device_status(mempool_devices, "Mempool")

    if status_dev in ["compress", "all"]:
        show_device_status(compress_devices, "Compress")

    if status_dev in ["misc", "all"]:
        show_device_status(misc_devices, "Misc (rawdev)")

    if status_dev in ["regex", "all"]:
        show_device_status(regex_devices, "Regex")


def pci_glob(arg):
    '''Returns a list containing either:
    * List of PCI B:D:F matching arg, using shell wildcards e.g. 80:04.*
    * Only the passed arg if matching list is empty'''
    sysfs_path = "/sys/bus/pci/devices"
    for _glob in [arg, '0000:' + arg]:
        paths = [basename(path) for path in glob(path_join(sysfs_path, _glob))]
        if paths:
            return paths
    return [arg]


def parse_args():
    '''Parses the command-line arguments given by the user and takes the
    appropriate action for each'''
    global b_flag
    global status_flag
    global status_dev
    global force_flag
    global args

    parser = argparse.ArgumentParser(
        description='Utility to bind and unbind devices from Linux kernel',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
---------

To display current device status:
        %(prog)s --status

To display current network device status:
        %(prog)s --status-dev net

To bind eth1 from the current driver and move to use vfio-pci
        %(prog)s --bind=vfio-pci eth1

To unbind 0000:01:00.0 from using any driver
        %(prog)s -u 0000:01:00.0

To bind 0000:02:00.0 and 0000:02:00.1 to the ixgbe kernel driver
        %(prog)s -b ixgbe 02:00.0 02:00.1
""")

    parser.add_argument(
        '-s',
        '--status',
        action='store_true',
        help="Print the current status of all known devices.")
    parser.add_argument(
        '--status-dev',
        help="Print the status of given device group.",
        choices=['baseband', 'compress', 'crypto', 'event',
                'mempool', 'misc', 'net', 'regex'])
    bind_group = parser.add_mutually_exclusive_group()
    bind_group.add_argument(
        '-b',
        '--bind',
        metavar='DRIVER',
        help="Select the driver to use or \"none\" to unbind the device")
    bind_group.add_argument(
        '-u',
        '--unbind',
        action='store_true',
        help="Unbind a device (equivalent to \"-b none\")")
    parser.add_argument(
        '--force',
        action='store_true',
        help="""
Override restriction on binding devices in use by Linux"
WARNING: This can lead to loss of network connection and should be used with caution.
""")
    parser.add_argument(
        'devices',
        metavar='DEVICE',
        nargs='*',
        help="""
Device specified as PCI "domain:bus:slot.func" syntax or "bus:slot.func" syntax.
For devices bound to Linux kernel drivers, they may be referred to by interface name.
""")

    opt = parser.parse_args()

    if opt.status_dev:
        status_flag = True
        status_dev = opt.status_dev
    if opt.status:
        status_flag = True
        status_dev = "all"
    if opt.force:
        force_flag = True
    if opt.bind:
        b_flag = opt.bind
    elif opt.unbind:
        b_flag = "none"
    args = opt.devices

    if not b_flag and not status_flag:
        print("Error: No action specified for devices. "
              "Please give a --bind, --ubind or --status option",
              file=sys.stderr)
        parser.print_usage()
        sys.exit(1)

    if b_flag and not args:
        print("Error: No devices specified.", file=sys.stderr)
        parser.print_usage()
        sys.exit(1)

    # resolve any PCI globs in the args
    new_args = []
    for arg in args:
        new_args.extend(pci_glob(arg))
    args = new_args

def do_arg_actions():
    '''do the actual action requested by the user'''
    global b_flag
    global status_flag
    global force_flag
    global args

    if b_flag in ["none", "None"]:
        unbind_all(args, force_flag)
    elif b_flag is not None:
        bind_all(args, b_flag, force_flag)
    if status_flag:
        if b_flag is not None:
            clear_data()
            # refresh if we have changed anything
            get_device_details(network_devices)
            get_device_details(baseband_devices)
            get_device_details(crypto_devices)
            get_device_details(eventdev_devices)
            get_device_details(mempool_devices)
            get_device_details(compress_devices)
            get_device_details(regex_devices)
            get_device_details(misc_devices)
        show_status()


def main():
    '''program main function'''
    # check if lspci is installed, suppress any output
    with open(os.devnull, 'w') as devnull:
        ret = subprocess.call(['which', 'lspci'],
                              stdout=devnull, stderr=devnull)
        if ret != 0:
            sys.exit("'lspci' not found - please install 'pciutils'")
    parse_args()
    check_modules()
    clear_data()
    get_device_details(network_devices)
    get_device_details(baseband_devices)
    get_device_details(crypto_devices)
    get_device_details(eventdev_devices)
    get_device_details(mempool_devices)
    get_device_details(compress_devices)
    get_device_details(regex_devices)
    get_device_details(misc_devices)
    do_arg_actions()

if __name__ == "__main__":
    main()
