#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016  Neil Horman <nhorman@tuxdriver.com>

# -------------------------------------------------------------------------
#
# Utility to dump PMD_INFO_STRING support from an object file
#
# -------------------------------------------------------------------------
from __future__ import print_function
import json
import os
import platform
import string
import sys
from elftools.common.exceptions import ELFError
from elftools.common.py3compat import (byte2int, bytes2str, str2bytes)
from elftools.elf.elffile import ELFFile
from optparse import OptionParser

# For running from development directory. It should take precedence over the
# installed pyelftools.
sys.path.insert(0, '.')

raw_output = False
pcidb = None

# ===========================================


class Vendor:
    """
    Class for vendors. This is the top level class
    for the devices belong to a specific vendor.
    self.devices is the device dictionary
    subdevices are in each device.
    """

    def __init__(self, vendorStr):
        """
        Class initializes with the raw line from pci.ids
        Parsing takes place inside __init__
        """
        self.ID = vendorStr.split()[0]
        self.name = vendorStr.replace("%s " % self.ID, "").rstrip()
        self.devices = {}

    def addDevice(self, deviceStr):
        """
        Adds a device to self.devices
        takes the raw line from pci.ids
        """
        s = deviceStr.strip()
        devID = s.split()[0]
        if devID in self.devices:
            pass
        else:
            self.devices[devID] = Device(deviceStr)

    def report(self):
        print(self.ID, self.name)
        for id, dev in self.devices.items():
            dev.report()

    def find_device(self, devid):
        # convert to a hex string and remove 0x
        devid = hex(devid)[2:]
        try:
            return self.devices[devid]
        except:
            return Device("%s  Unknown Device" % devid)


class Device:

    def __init__(self, deviceStr):
        """
        Class for each device.
        Each vendor has its own devices dictionary.
        """
        s = deviceStr.strip()
        self.ID = s.split()[0]
        self.name = s.replace("%s  " % self.ID, "")
        self.subdevices = {}

    def report(self):
        print("\t%s\t%s" % (self.ID, self.name))
        for subID, subdev in self.subdevices.items():
            subdev.report()

    def addSubDevice(self, subDeviceStr):
        """
        Adds a subvendor, subdevice to device.
        Uses raw line from pci.ids
        """
        s = subDeviceStr.strip()
        spl = s.split()
        subVendorID = spl[0]
        subDeviceID = spl[1]
        subDeviceName = s.split("  ")[-1]
        devID = "%s:%s" % (subVendorID, subDeviceID)
        self.subdevices[devID] = SubDevice(
            subVendorID, subDeviceID, subDeviceName)

    def find_subid(self, subven, subdev):
        subven = hex(subven)[2:]
        subdev = hex(subdev)[2:]
        devid = "%s:%s" % (subven, subdev)

        try:
            return self.subdevices[devid]
        except:
            if (subven == "ffff" and subdev == "ffff"):
                return SubDevice("ffff", "ffff", "(All Subdevices)")
            else:
                return SubDevice(subven, subdev, "(Unknown Subdevice)")


class SubDevice:
    """
    Class for subdevices.
    """

    def __init__(self, vendor, device, name):
        """
        Class initializes with vendorid, deviceid and name
        """
        self.vendorID = vendor
        self.deviceID = device
        self.name = name

    def report(self):
        print("\t\t%s\t%s\t%s" % (self.vendorID, self.deviceID, self.name))


class PCIIds:
    """
    Top class for all pci.ids entries.
    All queries will be asked to this class.
    PCIIds.vendors["0e11"].devices["0046"].\
    subdevices["0e11:4091"].name  =  "Smart Array 6i"
    """

    def __init__(self, filename):
        """
        Prepares the directories.
        Checks local data file.
        Tries to load from local, if not found, downloads from web
        """
        self.version = ""
        self.date = ""
        self.vendors = {}
        self.contents = None
        self.readLocal(filename)
        self.parse()

    def reportVendors(self):
        """Reports the vendors
        """
        for vid, v in self.vendors.items():
            print(v.ID, v.name)

    def report(self, vendor=None):
        """
        Reports everything for all vendors or a specific vendor
        PCIIds.report()  reports everything
        PCIIDs.report("0e11") reports only "Compaq Computer Corporation"
        """
        if vendor is not None:
            self.vendors[vendor].report()
        else:
            for vID, v in self.vendors.items():
                v.report()

    def find_vendor(self, vid):
        # convert vid to a hex string and remove the 0x
        vid = hex(vid)[2:]

        try:
            return self.vendors[vid]
        except:
            return Vendor("%s Unknown Vendor" % (vid))

    def findDate(self, content):
        for l in content:
            if l.find("Date:") > -1:
                return l.split()[-2].replace("-", "")
        return None

    def parse(self):
        if len(self.contents) < 1:
            print("data/%s-pci.ids not found" % self.date)
        else:
            vendorID = ""
            deviceID = ""
            for l in self.contents:
                if l[0] == "#":
                    continue
                elif len(l.strip()) == 0:
                    continue
                else:
                    if l.find("\t\t") == 0:
                        self.vendors[vendorID].devices[
                            deviceID].addSubDevice(l)
                    elif l.find("\t") == 0:
                        deviceID = l.strip().split()[0]
                        self.vendors[vendorID].addDevice(l)
                    else:
                        vendorID = l.split()[0]
                        self.vendors[vendorID] = Vendor(l)

    def readLocal(self, filename):
        """
        Reads the local file
        """
        self.contents = open(filename).readlines()
        self.date = self.findDate(self.contents)

    def loadLocal(self):
        """
        Loads database from local. If there is no file,
        it creates a new one from web
        """
        self.date = idsfile[0].split("/")[1].split("-")[0]
        self.readLocal()


# =======================================

def search_file(filename, search_path):
    """ Given a search path, find file with requested name """
    for path in string.split(search_path, ":"):
        candidate = os.path.join(path, filename)
        if os.path.exists(candidate):
            return os.path.abspath(candidate)
    return None


class ReadElf(object):
    """ display_* methods are used to emit output into the output stream
    """

    def __init__(self, file, output):
        """ file:
                stream object with the ELF file to read

            output:
                output stream to write to
        """
        self.elffile = ELFFile(file)
        self.output = output

        # Lazily initialized if a debug dump is requested
        self._dwarfinfo = None

        self._versioninfo = None

    def _section_from_spec(self, spec):
        """ Retrieve a section given a "spec" (either number or name).
            Return None if no such section exists in the file.
        """
        try:
            num = int(spec)
            if num < self.elffile.num_sections():
                return self.elffile.get_section(num)
            else:
                return None
        except ValueError:
            # Not a number. Must be a name then
            return self.elffile.get_section_by_name(str2bytes(spec))

    def pretty_print_pmdinfo(self, pmdinfo):
        global pcidb

        for i in pmdinfo["pci_ids"]:
            vendor = pcidb.find_vendor(i[0])
            device = vendor.find_device(i[1])
            subdev = device.find_subid(i[2], i[3])
            print("%s (%s) : %s (%s) %s" %
                  (vendor.name, vendor.ID, device.name,
                   device.ID, subdev.name))

    def parse_pmd_info_string(self, mystring):
        global raw_output
        global pcidb

        optional_pmd_info = [
            {'id': 'params', 'tag': 'PMD PARAMETERS'},
            {'id': 'kmod', 'tag': 'PMD KMOD DEPENDENCIES'}
        ]

        i = mystring.index("=")
        mystring = mystring[i + 2:]
        pmdinfo = json.loads(mystring)

        if raw_output:
            print(json.dumps(pmdinfo))
            return

        print("PMD NAME: " + pmdinfo["name"])
        for i in optional_pmd_info:
            try:
                print("%s: %s" % (i['tag'], pmdinfo[i['id']]))
            except KeyError:
                continue

        if (len(pmdinfo["pci_ids"]) != 0):
            print("PMD HW SUPPORT:")
            if pcidb is not None:
                self.pretty_print_pmdinfo(pmdinfo)
            else:
                print("VENDOR\t DEVICE\t SUBVENDOR\t SUBDEVICE")
                for i in pmdinfo["pci_ids"]:
                    print("0x%04x\t 0x%04x\t 0x%04x\t\t 0x%04x" %
                          (i[0], i[1], i[2], i[3]))

        print("")

    def display_pmd_info_strings(self, section_spec):
        """ Display a strings dump of a section. section_spec is either a
            section number or a name.
        """
        section = self._section_from_spec(section_spec)
        if section is None:
            return

        data = section.data()
        dataptr = 0

        while dataptr < len(data):
            while (dataptr < len(data) and
                    not (32 <= byte2int(data[dataptr]) <= 127)):
                dataptr += 1

            if dataptr >= len(data):
                break

            endptr = dataptr
            while endptr < len(data) and byte2int(data[endptr]) != 0:
                endptr += 1

            mystring = bytes2str(data[dataptr:endptr])
            rc = mystring.find("PMD_INFO_STRING")
            if (rc != -1):
                self.parse_pmd_info_string(mystring)

            dataptr = endptr

    def find_librte_eal(self, section):
        for tag in section.iter_tags():
            if tag.entry.d_tag == 'DT_NEEDED':
                if "librte_eal" in tag.needed:
                    return tag.needed
        return None

    def search_for_autoload_path(self):
        scanelf = self
        scanfile = None
        library = None

        section = self._section_from_spec(".dynamic")
        try:
            eallib = self.find_librte_eal(section)
            if eallib is not None:
                ldlibpath = os.environ.get('LD_LIBRARY_PATH')
                if ldlibpath is None:
                    ldlibpath = ""
                dtr = self.get_dt_runpath(section)
                library = search_file(eallib,
                                      dtr + ":" + ldlibpath +
                                      ":/usr/lib64:/lib64:/usr/lib:/lib")
                if library is None:
                    return (None, None)
                if raw_output is False:
                    print("Scanning for autoload path in %s" % library)
                scanfile = open(library, 'rb')
                scanelf = ReadElf(scanfile, sys.stdout)
        except AttributeError:
            # Not a dynamic binary
            pass
        except ELFError:
            scanfile.close()
            return (None, None)

        section = scanelf._section_from_spec(".rodata")
        if section is None:
            if scanfile is not None:
                scanfile.close()
            return (None, None)

        data = section.data()
        dataptr = 0

        while dataptr < len(data):
            while (dataptr < len(data) and
                    not (32 <= byte2int(data[dataptr]) <= 127)):
                dataptr += 1

            if dataptr >= len(data):
                break

            endptr = dataptr
            while endptr < len(data) and byte2int(data[endptr]) != 0:
                endptr += 1

            mystring = bytes2str(data[dataptr:endptr])
            rc = mystring.find("DPDK_PLUGIN_PATH")
            if (rc != -1):
                rc = mystring.find("=")
                return (mystring[rc + 1:], library)

            dataptr = endptr
        if scanfile is not None:
            scanfile.close()
        return (None, None)

    def get_dt_runpath(self, dynsec):
        for tag in dynsec.iter_tags():
            if tag.entry.d_tag == 'DT_RUNPATH':
                return tag.runpath
        return ""

    def process_dt_needed_entries(self):
        """ Look to see if there are any DT_NEEDED entries in the binary
            And process those if there are
        """
        global raw_output
        runpath = ""
        ldlibpath = os.environ.get('LD_LIBRARY_PATH')
        if ldlibpath is None:
            ldlibpath = ""

        dynsec = self._section_from_spec(".dynamic")
        try:
            runpath = self.get_dt_runpath(dynsec)
        except AttributeError:
            # dynsec is None, just return
            return

        for tag in dynsec.iter_tags():
            if tag.entry.d_tag == 'DT_NEEDED':
                rc = tag.needed.find(b"librte_pmd")
                if (rc != -1):
                    library = search_file(tag.needed,
                                          runpath + ":" + ldlibpath +
                                          ":/usr/lib64:/lib64:/usr/lib:/lib")
                    if library is not None:
                        if raw_output is False:
                            print("Scanning %s for pmd information" % library)
                        with open(library, 'rb') as file:
                            try:
                                libelf = ReadElf(file, sys.stdout)
                            except ELFError:
                                print("%s is no an ELF file" % library)
                                continue
                            libelf.process_dt_needed_entries()
                            libelf.display_pmd_info_strings(".rodata")
                            file.close()


def scan_autoload_path(autoload_path):
    global raw_output

    if os.path.exists(autoload_path) is False:
        return

    try:
        dirs = os.listdir(autoload_path)
    except OSError:
        # Couldn't read the directory, give up
        return

    for d in dirs:
        dpath = os.path.join(autoload_path, d)
        if os.path.isdir(dpath):
            scan_autoload_path(dpath)
        if os.path.isfile(dpath):
            try:
                file = open(dpath, 'rb')
                readelf = ReadElf(file, sys.stdout)
            except ELFError:
                # this is likely not an elf file, skip it
                continue
            except IOError:
                # No permission to read the file, skip it
                continue

            if raw_output is False:
                print("Hw Support for library %s" % d)
            readelf.display_pmd_info_strings(".rodata")
            file.close()


def scan_for_autoload_pmds(dpdk_path):
    """
    search the specified application or path for a pmd autoload path
    then scan said path for pmds and report hw support
    """
    global raw_output

    if (os.path.isfile(dpdk_path) is False):
        if raw_output is False:
            print("Must specify a file name")
        return

    file = open(dpdk_path, 'rb')
    try:
        readelf = ReadElf(file, sys.stdout)
    except ElfError:
        if raw_output is False:
            print("Unable to parse %s" % file)
        return

    (autoload_path, scannedfile) = readelf.search_for_autoload_path()
    if (autoload_path is None or autoload_path is ""):
        if (raw_output is False):
            print("No autoload path configured in %s" % dpdk_path)
        return
    if (raw_output is False):
        if (scannedfile is None):
            scannedfile = dpdk_path
        print("Found autoload path %s in %s" % (autoload_path, scannedfile))

    file.close()
    if (raw_output is False):
        print("Discovered Autoload HW Support:")
    scan_autoload_path(autoload_path)
    return


def main(stream=None):
    global raw_output
    global pcidb

    pcifile_default = "./pci.ids"  # For unknown OS's assume local file
    if platform.system() == 'Linux':
        pcifile_default = "/usr/share/hwdata/pci.ids"
    elif platform.system() == 'FreeBSD':
        pcifile_default = "/usr/local/share/pciids/pci.ids"
        if not os.path.exists(pcifile_default):
            pcifile_default = "/usr/share/misc/pci_vendors"

    optparser = OptionParser(
        usage='usage: %prog [-hrtp] [-d <pci id file] <elf-file>',
        description="Dump pmd hardware support info",
        add_help_option=True)
    optparser.add_option('-r', '--raw',
                         action='store_true', dest='raw_output',
                         help='Dump raw json strings')
    optparser.add_option("-d", "--pcidb", dest="pcifile",
                         help="specify a pci database "
                              "to get vendor names from",
                         default=pcifile_default, metavar="FILE")
    optparser.add_option("-t", "--table", dest="tblout",
                         help="output information on hw support as a "
                              "hex table",
                         action='store_true')
    optparser.add_option("-p", "--plugindir", dest="pdir",
                         help="scan dpdk for autoload plugins",
                         action='store_true')

    options, args = optparser.parse_args()

    if options.raw_output:
        raw_output = True

    if options.pcifile:
        pcidb = PCIIds(options.pcifile)
        if pcidb is None:
            print("Pci DB file not found")
            exit(1)

    if options.tblout:
        options.pcifile = None
        pcidb = None

    if (len(args) == 0):
        optparser.print_usage()
        exit(1)

    if options.pdir is True:
        exit(scan_for_autoload_pmds(args[0]))

    ldlibpath = os.environ.get('LD_LIBRARY_PATH')
    if (ldlibpath is None):
        ldlibpath = ""

    if (os.path.exists(args[0]) is True):
        myelffile = args[0]
    else:
        myelffile = search_file(
            args[0], ldlibpath + ":/usr/lib64:/lib64:/usr/lib:/lib")

    if (myelffile is None):
        print("File not found")
        sys.exit(1)

    with open(myelffile, 'rb') as file:
        try:
            readelf = ReadElf(file, sys.stdout)
            readelf.process_dt_needed_entries()
            readelf.display_pmd_info_strings(".rodata")
            sys.exit(0)

        except ELFError as ex:
            sys.stderr.write('ELF error: %s\n' % ex)
            sys.exit(1)


# -------------------------------------------------------------------------
if __name__ == '__main__':
    main()
