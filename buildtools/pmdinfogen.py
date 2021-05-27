#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2016 Neil Horman <nhorman@tuxdriver.com>
# Copyright (c) 2020 Dmitry Kozlyuk <dmitry.kozliuk@gmail.com>

import argparse
import ctypes
import json
import sys
import tempfile

try:
    import elftools
    from elftools.elf.elffile import ELFFile
    from elftools.elf.sections import SymbolTableSection
except ImportError:
    pass

import coff


class ELFSymbol:
    def __init__(self, image, symbol):
        self._image = image
        self._symbol = symbol

    @property
    def string_value(self):
        size = self._symbol["st_size"]
        value = self.get_value(0, size)
        return coff.decode_asciiz(value)  # not COFF-specific

    def get_value(self, offset, size):
        section = self._symbol["st_shndx"]
        data = self._image.get_section(section).data()
        base = self._symbol["st_value"] + offset
        return data[base : base + size]


class ELFImage:
    def __init__(self, data):
        version = tuple(int(c) for c in elftools.__version__.split("."))
        self._legacy_elftools = version < (0, 24)

        self._image = ELFFile(data)

        section = b".symtab" if self._legacy_elftools else ".symtab"
        self._symtab = self._image.get_section_by_name(section)
        if not isinstance(self._symtab, SymbolTableSection):
            raise Exception(".symtab section is not a symbol table")

    @property
    def is_big_endian(self):
        return not self._image.little_endian

    def find_by_name(self, name):
        symbol = self._get_symbol_by_name(name)
        return ELFSymbol(self._image, symbol[0]) if symbol else None

    def _get_symbol_by_name(self, name):
        if not self._legacy_elftools:
            return self._symtab.get_symbol_by_name(name)
        name = name.encode("utf-8")
        for symbol in self._symtab.iter_symbols():
            if symbol.name == name:
                return [symbol]
        return None

    def find_by_prefix(self, prefix):
        prefix = prefix.encode("utf-8") if self._legacy_elftools else prefix
        for i in range(self._symtab.num_symbols()):
            symbol = self._symtab.get_symbol(i)
            if symbol.name.startswith(prefix):
                yield ELFSymbol(self._image, symbol)


class COFFSymbol:
    def __init__(self, image, symbol):
        self._image = image
        self._symbol = symbol

    def get_value(self, offset, size):
        value = self._symbol.get_value(offset)
        return value[:size] if value else value

    @property
    def string_value(self):
        value = self._symbol.get_value(0)
        return coff.decode_asciiz(value) if value else ''


class COFFImage:
    def __init__(self, data):
        self._image = coff.Image(data)

    @property
    def is_big_endian(self):
        return False

    def find_by_prefix(self, prefix):
        for symbol in self._image.symbols:
            if symbol.name.startswith(prefix):
                yield COFFSymbol(self._image, symbol)

    def find_by_name(self, name):
        for symbol in self._image.symbols:
            if symbol.name == name:
                return COFFSymbol(self._image, symbol)
        return None


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

        result = []
        while True:
            size = ctypes.sizeof(rte_pci_id)
            offset = size * len(result)
            data = table_symbol.get_value(offset, size)
            if not data:
                break
            pci_id = rte_pci_id.from_buffer_copy(data)
            if not pci_id.device_id:
                break
            result.append(
                [
                    pci_id.vendor_id,
                    pci_id.device_id,
                    pci_id.subsystem_vendor_id,
                    pci_id.subsystem_device_id,
                ]
            )
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
    parser.add_argument("format", help="object file format, 'elf' or 'coff'")
    parser.add_argument(
        "input", nargs='+', help="input object file path or '-' for stdin"
    )
    parser.add_argument("output", help="output C file path or '-' for stdout")
    return parser.parse_args()


def open_input(path):
    if path == "-":
        temp = tempfile.TemporaryFile()
        temp.write(sys.stdin.buffer.read())
        return temp
    return open(path, "rb")


def read_input(path):
    if path == "-":
        return sys.stdin.buffer.read()
    with open(path, "rb") as file:
        return file.read()


def load_image(fmt, path):
    if fmt == "elf":
        return ELFImage(open_input(path))
    if fmt == "coff":
        return COFFImage(read_input(path))
    raise Exception("unsupported object file format")


def open_output(path):
    if path == "-":
        return sys.stdout
    return open(path, "w")


def write_header(output):
    output.write(
        "static __attribute__((unused)) const char *generator = \"%s\";\n" % sys.argv[0]
    )


def main():
    args = parse_args()
    if args.input.count('-') > 1:
        raise Exception("'-' input cannot be used multiple times")
    if args.format == "elf" and "ELFFile" not in globals():
        raise Exception("elftools module not found")

    output = open_output(args.output)
    write_header(output)
    for path in args.input:
        image = load_image(args.format, path)
        drivers = load_drivers(image)
        dump_drivers(drivers, output)


if __name__ == "__main__":
    main()
