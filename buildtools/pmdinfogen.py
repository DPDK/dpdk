#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2016 Neil Horman <nhorman@tuxdriver.com>
# Copyright (c) 2020 Dmitry Kozlyuk <dmitry.kozliuk@gmail.com>

import argparse
import ctypes
import json
import sys
import tempfile

from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection


class ELFSymbol:
    def __init__(self, image, symbol):
        self._image = image
        self._symbol = symbol

    @property
    def size(self):
        return self._symbol["st_size"]

    @property
    def value(self):
        data = self._image.get_section_data(self._symbol["st_shndx"])
        base = self._symbol["st_value"]
        return data[base:base + self.size]

    @property
    def string_value(self):
        value = self.value
        return value[:-1].decode() if value else ""


class ELFImage:
    def __init__(self, data):
        self._image = ELFFile(data)
        self._symtab = self._image.get_section_by_name(".symtab")
        if not isinstance(self._symtab, SymbolTableSection):
            raise Exception(".symtab section is not a symbol table")

    @property
    def is_big_endian(self):
        return not self._image.little_endian

    def get_section_data(self, name):
        return self._image.get_section(name).data()

    def find_by_name(self, name):
        symbol = self._symtab.get_symbol_by_name(name)
        return ELFSymbol(self, symbol[0]) if symbol else None

    def find_by_prefix(self, prefix):
        for i in range(self._symtab.num_symbols()):
            symbol = self._symtab.get_symbol(i)
            if symbol.name.startswith(prefix):
                yield ELFSymbol(self, symbol)


def define_rte_pci_id(is_big_endian):
    base_type = ctypes.LittleEndianStructure
    if is_big_endian:
        base_type = ctypes.BigEndianStructure

    class rte_pci_id(base_type):
        _pack_ = True
        _fields_ = [
            ("class_id", ctypes.c_uint32),
            ("vendor_id", ctypes.c_uint16),
            ("device_id", ctypes.c_uint16),
            ("subsystem_vendor_id", ctypes.c_uint16),
            ("subsystem_device_id", ctypes.c_uint16),
        ]

    return rte_pci_id


class Driver:
    OPTIONS = [
        ("params", "_param_string_export"),
        ("kmod", "_kmod_dep_export"),
    ]

    def __init__(self, name, options):
        self.name = name
        for key, value in options.items():
            setattr(self, key, value)
        self.pci_ids = []

    @classmethod
    def load(cls, image, symbol):
        name = symbol.string_value

        options = {}
        for key, suffix in cls.OPTIONS:
            option_symbol = image.find_by_name("__%s%s" % (name, suffix))
            if option_symbol:
                value = option_symbol.string_value
                options[key] = value

        driver = cls(name, options)

        pci_table_name_symbol = image.find_by_name("__%s_pci_tbl_export" % name)
        if pci_table_name_symbol:
            driver.pci_ids = cls._load_pci_ids(image, pci_table_name_symbol)

        return driver

    @staticmethod
    def _load_pci_ids(image, table_name_symbol):
        table_name = table_name_symbol.string_value
        table_symbol = image.find_by_name(table_name)
        if not table_symbol:
            raise Exception("PCI table declared but not defined: %d" % table_name)

        rte_pci_id = define_rte_pci_id(image.is_big_endian)

        pci_id_size = ctypes.sizeof(rte_pci_id)
        pci_ids_desc = rte_pci_id * (table_symbol.size // pci_id_size)
        pci_ids = pci_ids_desc.from_buffer_copy(table_symbol.value)
        result = []
        for pci_id in pci_ids:
            if not pci_id.device_id:
                break
            result.append([
                pci_id.vendor_id,
                pci_id.device_id,
                pci_id.subsystem_vendor_id,
                pci_id.subsystem_device_id,
                ])
        return result

    def dump(self, file):
        dumped = json.dumps(self.__dict__)
        escaped = dumped.replace('"', '\\"')
        print(
            'const char %s_pmd_info[] __attribute__((used)) = "PMD_INFO_STRING= %s";'
            % (self.name, escaped),
            file=file,
        )


def load_drivers(image):
    drivers = []
    for symbol in image.find_by_prefix("this_pmd_name"):
        drivers.append(Driver.load(image, symbol))
    return drivers


def dump_drivers(drivers, file):
    # Keep legacy order of definitions.
    for driver in reversed(drivers):
        driver.dump(file)


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", help="input object file path or '-' for stdin")
    parser.add_argument("output", help="output C file path or '-' for stdout")
    return parser.parse_args()


def open_input(path):
    if path == "-":
        temp = tempfile.TemporaryFile()
        temp.write(sys.stdin.buffer.read())
        return temp
    return open(path, "rb")


def open_output(path):
    if path == "-":
        return sys.stdout
    return open(path, "w")


def main():
    args = parse_args()
    infile = open_input(args.input)
    image = ELFImage(infile)
    drivers = load_drivers(image)
    output = open_output(args.output)
    dump_drivers(drivers, output)


if __name__ == "__main__":
    main()
