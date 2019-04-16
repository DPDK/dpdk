#! /usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
#

import sys
import os
import getopt
import subprocess
from os.path import exists, abspath, dirname, basename

# The PCI base class for all devices
network_class = {'Class': '02', 'Vendor': None, 'Device': None,
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

network_devices = [network_class, cavium_pkx, avp_vnic, ifpga_class]
crypto_devices = [encryption_class, intel_processor_class]
eventdev_devices = [cavium_sso, cavium_tim, octeontx2_sso]
mempool_devices = [cavium_fpa, octeontx2_npa]
compress_devices = [cavium_zip]

# global dict ethernet devices present. Dictionary indexed by PCI address.
# Each device within this is itself a dictionary of device properties
devices = {}
# list of supported DPDK drivers
dpdk_drivers = ["igb_uio", "vfio-pci", "uio_pci_generic"]

# command-line arg flags
b_flag = None
status_flag = False
force_flag = False
args = []


def usage():
    '''Print usage information for the program'''
    argv0 = basename(sys.argv[0])
    print("""
Usage:
------

     %(argv0)s [options] DEVICE1 DEVICE2 ....

where DEVICE1, DEVICE2 etc, are specified via PCI "domain:bus:slot.func" syntax
or "bus:slot.func" syntax. For devices bound to Linux kernel drivers, they may
also be referred to by Linux interface name e.g. eth0, eth1, em0, em1, etc.

Options:
    --help, --usage:
        Display usage information and quit

    -s, --status:
        Print the current status of all known network, crypto, event
        and mempool devices.
        For each device, it displays the PCI domain, bus, slot and function,
        along with a text description of the device. Depending upon whether the
        device is being used by a kernel driver, the igb_uio driver, or no
        driver, other relevant information will be displayed:
        * the Linux interface name e.g. if=eth0
        * the driver being used e.g. drv=igb_uio
        * any suitable drivers not currently using that device
            e.g. unused=igb_uio
        NOTE: if this flag is passed along with a bind/unbind option, the
        status display will always occur after the other operations have taken
        place.

    --status-dev:
        Print the status of given device group. Supported device groups are:
        "net", "crypto", "event", "mempool" and "compress"

    -b driver, --bind=driver:
        Select the driver to use or \"none\" to unbind the device

    -u, --unbind:
        Unbind a device (Equivalent to \"-b none\")

    --force:
        By default, network devices which are used by Linux - as indicated by
        having routes in the routing table - cannot be modified. Using the
        --force flag overrides this behavior, allowing active links to be
        forcibly unbound.
        WARNING: This can lead to loss of network connection and should be used
        with caution.

Examples:
---------

To display current device status:
        %(argv0)s --status

To display current network device status:
        %(argv0)s --status-dev net

To bind eth1 from the current driver and move to use igb_uio
        %(argv0)s --bind=igb_uio eth1

To unbind 0000:01:00.0 from using any driver
        %(argv0)s -u 0000:01:00.0

To bind 0000:02:00.0 and 0000:02:00.1 to the ixgbe kernel driver
        %(argv0)s -b ixgbe 02:00.0 02:00.1

    """ % locals())  # replace items from local variables


# This is roughly compatible with check_output function in subprocess module
# which is only available in python 2.7.
def check_output(args, stderr=None):
    '''Run a command and capture its output'''
    return subprocess.Popen(args, stdout=subprocess.PIPE,
                            stderr=stderr).communicate()[0]


def check_modules():
    '''Checks that igb_uio is loaded'''
    global dpdk_drivers

    # list of supported modules
    mods = [{"Name": driver, "Found": False} for driver in dpdk_drivers]

    # first check if module is loaded
    try:
        # Get list of sysfs modules (both built-in and dynamically loaded)
        sysfs_path = '/sys/module/'

        # Get the list of directories in sysfs_path
        sysfs_mods = [os.path.join(sysfs_path, o) for o
                      in os.listdir(sysfs_path)
                      if os.path.isdir(os.path.join(sysfs_path, o))]

        # Extract the last element of '/sys/module/abc' in the array
        sysfs_mods = [a.split('/')[-1] for a in sysfs_mods]

        # special case for vfio_pci (module is named vfio-pci,
        # but its .ko is named vfio_pci)
        sysfs_mods = [a if a != 'vfio_pci' else 'vfio-pci' for a in sysfs_mods]

        for mod in mods:
            if mod["Name"] in sysfs_mods:
                mod["Found"] = True
    except:
        pass

    # check if we have at least one loaded module
    if True not in [mod["Found"] for mod in mods] and b_flag is not None:
        if b_flag in dpdk_drivers:
            print("Error - no supported modules(DPDK driver) are loaded")
            sys.exit(1)
        else:
            print("Warning - no supported modules(DPDK driver) are loaded")

    # change DPDK driver list to only contain drivers that are loaded
    dpdk_drivers = [mod["Name"] for mod in mods if mod["Found"]]


def has_driver(dev_id):
    '''return true if a device is assigned to a driver. False otherwise'''
    return "Driver_str" in devices[dev_id]


def get_pci_device_details(dev_id, probe_lspci):
    '''This function gets additional details for a PCI device'''
    device = {}

    if probe_lspci:
        extra_info = check_output(["lspci", "-vmmks", dev_id]).splitlines()

        # parse lspci details
        for line in extra_info:
            if len(line) == 0:
                continue
            name, value = line.decode().split("\t", 1)
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
    dev_lines = check_output(["lspci", "-Dvmmnnk"]).splitlines()
    for dev_line in dev_lines:
        if len(dev_line) == 0:
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
            name, value = dev_line.decode().split("\t", 1)
            value_list = value.rsplit(' ', 1)
            if len(value_list) > 1:
                # String stored in <name>_str
                dev[name.rstrip(":") + '_str'] = value_list[0]
            # Numeric IDs
            dev[name.rstrip(":")] = value_list[len(value_list) - 1] \
                .rstrip("]").lstrip("[")

    if devices_type == network_devices:
        # check what is the interface if any for an ssh connection if
        # any to this host, so we can mark it later.
        ssh_if = []
        route = check_output(["ip", "-o", "route"])
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
    elif "0000:" + dev_name in devices:
        return "0000:" + dev_name
    else:
        # check if it's an interface name, e.g. eth1
        for d in devices.keys():
            if dev_name in devices[d]["Interface"].split(","):
                return devices[d]["Slot"]
    # if nothing else matches - error
    print("Unknown device: %s. "
          "Please specify device in \"bus:slot.func\" format" % dev_name)
    sys.exit(1)


def unbind_one(dev_id, force):
    '''Unbind the device identified by "dev_id" from its current driver'''
    dev = devices[dev_id]
    if not has_driver(dev_id):
        print("%s %s %s is not currently managed by any driver\n" %
              (dev["Slot"], dev["Device_str"], dev["Interface"]))
        return

    # prevent us disconnecting ourselves
    if dev["Ssh_if"] and not force:
        print("Routing table indicates that interface %s is active. "
              "Skipping unbind" % (dev_id))
        return

    # write to /sys to unbind
    filename = "/sys/bus/pci/drivers/%s/unbind" % dev["Driver_str"]
    try:
        f = open(filename, "a")
    except:
        print("Error: unbind failed for %s - Cannot open %s"
              % (dev_id, filename))
        sys.exit(1)
    f.write(dev_id)
    f.close()


def bind_one(dev_id, driver, force):
    '''Bind the device given by "dev_id" to the driver "driver". If the device
    is already bound to a different driver, it will be unbound first'''
    dev = devices[dev_id]
    saved_driver = None  # used to rollback any unbind in case of failure

    # prevent disconnection of our ssh session
    if dev["Ssh_if"] and not force:
        print("Routing table indicates that interface %s is active. "
              "Not modifying" % (dev_id))
        return

    # unbind any existing drivers we don't want
    if has_driver(dev_id):
        if dev["Driver_str"] == driver:
            print("%s already bound to driver %s, skipping\n"
                  % (dev_id, driver))
            return
        else:
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
        if os.path.exists(filename):
            try:
                f = open(filename, "w")
            except:
                print("Error: bind failed for %s - Cannot open %s"
                      % (dev_id, filename))
                return
            try:
                f.write("%s" % driver)
                f.close()
            except:
                print("Error: bind failed for %s - Cannot write driver %s to "
                      "PCI ID " % (dev_id, driver))
                return
        # For kernels < 3.15 use new_id to add PCI id's to the driver
        else:
            filename = "/sys/bus/pci/drivers/%s/new_id" % driver
            try:
                f = open(filename, "w")
            except:
                print("Error: bind failed for %s - Cannot open %s"
                      % (dev_id, filename))
                return
            try:
                # Convert Device and Vendor Id to int to write to new_id
                f.write("%04x %04x" % (int(dev["Vendor"],16),
                        int(dev["Device"], 16)))
                f.close()
            except:
                print("Error: bind failed for %s - Cannot write new PCI ID to "
                      "driver %s" % (dev_id, driver))
                return

    # do the bind by writing to /sys
    filename = "/sys/bus/pci/drivers/%s/bind" % driver
    try:
        f = open(filename, "a")
    except:
        print("Error: bind failed for %s - Cannot open %s"
              % (dev_id, filename))
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
              % (dev_id, driver))
        if saved_driver is not None:  # restore any previous driver
            bind_one(dev_id, saved_driver, force)
        return

    # For kernels > 3.15 driver_override is used to bind a device to a driver.
    # Before unbinding it, overwrite driver_override with empty string so that
    # the device can be bound to any other driver
    filename = "/sys/bus/pci/devices/%s/driver_override" % dev_id
    if os.path.exists(filename):
        try:
            f = open(filename, "w")
        except:
            print("Error: unbind failed for %s - Cannot open %s"
                  % (dev_id, filename))
            sys.exit(1)
        try:
            f.write("\00")
            f.close()
        except:
            print("Error: unbind failed for %s - Cannot open %s"
                  % (dev_id, filename))
            sys.exit(1)


def unbind_all(dev_list, force=False):
    """Unbind method, takes a list of device locations"""

    if dev_list[0] == "dpdk":
        for d in devices.keys():
            if "Driver_str" in devices[d]:
                if devices[d]["Driver_str"] in dpdk_drivers:
                    unbind_one(devices[d]["Slot"], force)
        return

    dev_list = map(dev_id_from_dev_name, dev_list)
    for d in dev_list:
        unbind_one(d, force)


def bind_all(dev_list, driver, force=False):
    """Bind method, takes a list of device locations"""
    global devices

    dev_list = map(dev_id_from_dev_name, dev_list)

    for d in dev_list:
        bind_one(d, driver, force)

    # For kernels < 3.15 when binding devices to a generic driver
    # (i.e. one that doesn't have a PCI ID table) using new_id, some devices
    # that are not bound to any other driver could be bound even if no one has
    # asked them to. hence, we check the list of drivers again, and see if
    # some of the previously-unbound devices were erroneously bound.
    if not os.path.exists("/sys/bus/pci/devices/%s/driver_override" % d):
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
    if len(dev_list) == 0:
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

def show_device_status(devices_type, device_name):
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
    if len(dpdk_drv) != 0:
        display_devices("%s devices using DPDK-compatible driver" % device_name,
                        dpdk_drv, "drv=%(Driver_str)s unused=%(Module_str)s")
    if len(kernel_drv) != 0:
        display_devices("%s devices using kernel driver" % device_name, kernel_drv,
                        "if=%(Interface)s drv=%(Driver_str)s "
                        "unused=%(Module_str)s %(Active)s")
    if len(no_drv) != 0:
        display_devices("Other %s devices" % device_name, no_drv,
                        "unused=%(Module_str)s")

def show_status():
    '''Function called when the script is passed the "--status" option.
    Displays to the user what devices are bound to the igb_uio driver, the
    kernel driver or to no driver'''

    if status_dev == "net" or status_dev == "all":
        show_device_status(network_devices, "Network")

    if status_dev == "crypto" or status_dev == "all":
        show_device_status(crypto_devices, "Crypto")

    if status_dev == "event" or status_dev == "all":
        show_device_status(eventdev_devices, "Eventdev")

    if status_dev == "mempool" or status_dev == "all":
        show_device_status(mempool_devices, "Mempool")

    if status_dev == "compress" or status_dev == "all":
        show_device_status(compress_devices , "Compress")


def parse_args():
    '''Parses the command-line arguments given by the user and takes the
    appropriate action for each'''
    global b_flag
    global status_flag
    global status_dev
    global force_flag
    global args
    if len(sys.argv) <= 1:
        usage()
        sys.exit(0)

    try:
        opts, args = getopt.getopt(sys.argv[1:], "b:us",
                                   ["help", "usage", "status", "status-dev=",
                                    "force", "bind=", "unbind", ])
    except getopt.GetoptError as error:
        print(str(error))
        print("Run '%s --usage' for further information" % sys.argv[0])
        sys.exit(1)

    for opt, arg in opts:
        if opt == "--help" or opt == "--usage":
            usage()
            sys.exit(0)
        if opt == "--status-dev":
            status_flag = True
            status_dev = arg
        if opt == "--status" or opt == "-s":
            status_flag = True
            status_dev = "all"
        if opt == "--force":
            force_flag = True
        if opt == "-b" or opt == "-u" or opt == "--bind" or opt == "--unbind":
            if b_flag is not None:
                print("Error - Only one bind or unbind may be specified\n")
                sys.exit(1)
            if opt == "-u" or opt == "--unbind":
                b_flag = "none"
            else:
                b_flag = arg


def do_arg_actions():
    '''do the actual action requested by the user'''
    global b_flag
    global status_flag
    global force_flag
    global args

    if b_flag is None and not status_flag:
        print("Error: No action specified for devices."
              "Please give a -b or -u option")
        print("Run '%s --usage' for further information" % sys.argv[0])
        sys.exit(1)

    if b_flag is not None and len(args) == 0:
        print("Error: No devices specified.")
        print("Run '%s --usage' for further information" % sys.argv[0])
        sys.exit(1)

    if b_flag == "none" or b_flag == "None":
        unbind_all(args, force_flag)
    elif b_flag is not None:
        bind_all(args, b_flag, force_flag)
    if status_flag:
        if b_flag is not None:
            clear_data()
            # refresh if we have changed anything
            get_device_details(network_devices)
            get_device_details(crypto_devices)
            get_device_details(eventdev_devices)
            get_device_details(mempool_devices)
            get_device_details(compress_devices)
        show_status()


def main():
    '''program main function'''
    # check if lspci is installed, suppress any output
    with open(os.devnull, 'w') as devnull:
        ret = subprocess.call(['which', 'lspci'],
                              stdout=devnull, stderr=devnull)
        if ret != 0:
            print("'lspci' not found - please install 'pciutils'")
            sys.exit(1)
    parse_args()
    check_modules()
    clear_data()
    get_device_details(network_devices)
    get_device_details(crypto_devices)
    get_device_details(eventdev_devices)
    get_device_details(mempool_devices)
    get_device_details(compress_devices)
    do_arg_actions()

if __name__ == "__main__":
    main()
