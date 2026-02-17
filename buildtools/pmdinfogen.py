#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2016 Neil Horman <nhorman@tuxdriver.com>
# Copyright (c) 2020 Dmitry Kozlyuk <dmitry.kozliuk@gmail.com>

import argparse
import json
import re
import struct
import sys
import tempfile

try:
    import elftools
    from elftools.elf.elffile import ELFFile
    from elftools.elf.sections import SymbolTableSection
except ImportError:
    pass


# x86_64 little-endian
COFF_MAGIC = 0x8664

# Names up to this length are stored immediately in symbol table entries.
COFF_NAMELEN = 8

# Special "section numbers" changing the meaning of symbol table entry.
COFF_SN_UNDEFINED = 0
COFF_SN_ABSOLUTE = -1
COFF_SN_DEBUG = -2


class CoffFileHeader(ctypes.LittleEndianStructure):
    _pack_ = True
    _fields_ = [
        ("magic", ctypes.c_uint16),
        ("section_count", ctypes.c_uint16),
        ("timestamp", ctypes.c_uint32),
        ("symbol_table_offset", ctypes.c_uint32),
        ("symbol_count", ctypes.c_uint32),
        ("optional_header_size", ctypes.c_uint16),
        ("flags", ctypes.c_uint16),
    ]


class CoffName(ctypes.Union):
    class Reference(ctypes.LittleEndianStructure):
        _pack_ = True
        _fields_ = [
            ("zeroes", ctypes.c_uint32),
            ("offset", ctypes.c_uint32),
        ]

    Immediate = ctypes.c_char * 8

    _pack_ = True
    _fields_ = [
        ("immediate", Immediate),
        ("reference", Reference),
    ]


class CoffSection(ctypes.LittleEndianStructure):
    _pack_ = True
    _fields_ = [
        ("name", CoffName),
        ("physical_address", ctypes.c_uint32),
        ("physical_address", ctypes.c_uint32),
        ("size", ctypes.c_uint32),
        ("data_offset", ctypes.c_uint32),
        ("relocations_offset", ctypes.c_uint32),
        ("line_numbers_offset", ctypes.c_uint32),
        ("relocation_count", ctypes.c_uint16),
        ("line_number_count", ctypes.c_uint16),
        ("flags", ctypes.c_uint32),
    ]


class CoffSymbol(ctypes.LittleEndianStructure):
    _pack_ = True
    _fields_ = [
        ("name", CoffName),
        ("value", ctypes.c_uint32),
        ("section_number", ctypes.c_int16),
        ("type", ctypes.c_uint16),
        ("storage_class", ctypes.c_uint8),
        ("auxiliary_count", ctypes.c_uint8),
    ]


class Symbol:
    def __init__(self, image, symbol: CoffSymbol):
        self._image = image
        self._coff = symbol

    @property
    def name(self):
        if self._coff.name.reference.zeroes:
            return decode_asciiz(bytes(self._coff.name.immediate))

        offset = self._coff.name.reference.offset
        offset -= ctypes.sizeof(ctypes.c_uint32)
        return self._image.get_string(offset)

    def get_value(self, offset):
        section_number = self._coff.section_number

        if section_number == COFF_SN_UNDEFINED:
            return None

        if section_number == COFF_SN_DEBUG:
            return None

        if section_number == COFF_SN_ABSOLUTE:
            return bytes(ctypes.c_uint32(self._coff.value))

        section_data = self._image.get_section_data(section_number)
        section_offset = self._coff.value + offset
        return section_data[section_offset:]


class Image:
    def __init__(self, data):
        header = CoffFileHeader.from_buffer_copy(data)
        header_size = ctypes.sizeof(header) + header.optional_header_size

        sections_desc = CoffSection * header.section_count
        sections = sections_desc.from_buffer_copy(data, header_size)

        symbols_desc = CoffSymbol * header.symbol_count
        symbols = symbols_desc.from_buffer_copy(data, header.symbol_table_offset)

        strings_offset = header.symbol_table_offset + ctypes.sizeof(symbols)
        strings = Image._parse_strings(data[strings_offset:])

        self._data = data
        self._header = header
        self._sections = sections
        self._symbols = symbols
        self._strings = strings

    @staticmethod
    def _parse_strings(data):
        full_size = ctypes.c_uint32.from_buffer_copy(data)
        header_size = ctypes.sizeof(full_size)
        return data[header_size : full_size.value]

    @property
    def symbols(self):
        i = 0
        while i < self._header.symbol_count:
            symbol = self._symbols[i]
            yield Symbol(self, symbol)
            i += symbol.auxiliary_count + 1

    def get_section_data(self, number):
        # section numbers are 1-based
        section = self._sections[number - 1]
        base = section.data_offset
        return self._data[base : base + section.size]

    def get_string(self, offset):
        return decode_asciiz(self._strings[offset:])


def decode_asciiz(data):
    index = data.find(b'\x00')
    end = index if index >= 0 else len(data)
    return data[:end].decode()

class ELFSymbol:
    def __init__(self, image, symbol):
        self._image = image
        self._symbol = symbol

    @property
    def string_value(self):
        size = self._symbol["st_size"]
        value = self.get_value(0, size)
        return decode_asciiz(value)  # not COFF-specific

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

    def find_by_pattern(self, pattern):
        pattern = pattern.encode("utf-8") if self._legacy_elftools else pattern
        for i in range(self._symtab.num_symbols()):
            symbol = self._symtab.get_symbol(i)
            if re.match(pattern, symbol.name):
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
        return decode_asciiz(value) if value else ""


class COFFImage:
    def __init__(self, data):
        self._image = Image(data)

    @property
    def is_big_endian(self):
        return False

    def find_by_pattern(self, pattern):
        for symbol in self._image.symbols:
            if re.match(pattern, symbol.name):
                yield COFFSymbol(self._image, symbol)

    def find_by_name(self, name):
        for symbol in self._image.symbols:
            if symbol.name == name:
                return COFFSymbol(self._image, symbol)
        return None


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

        if image.is_big_endian:
            fmt = ">"
        else:
            fmt = "<"
        fmt += "LHHHH"

        result = []
        while True:
            size = struct.calcsize(fmt)
            offset = size * len(result)
            data = table_symbol.get_value(offset, size)
            if not data:
                break
            _, vendor, device, ss_vendor, ss_device = struct.unpack_from(fmt, data)
            if not device:
                break
            result.append((vendor, device, ss_vendor, ss_device))

        return result

    def dump(self, file):
        dumped = json.dumps(self.__dict__)
        escaped = dumped.replace('"', '\\"')
        print(
            'RTE_PMD_EXPORT_SYMBOL(const char, %s_pmd_info)[] = "PMD_INFO_STRING= %s";'
            % (self.name, escaped),
            file=file,
        )


def load_drivers(image):
    drivers = []
    for symbol in image.find_by_pattern("^this_pmd_name[0-9a-zA-Z_]+$"):
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
        "input", nargs="+", help="input object file path or '-' for stdin"
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
        "#include <dev_driver.h>\n"
        'static __rte_unused const char *generator = "%s";\n' % sys.argv[0]
    )


def main():
    args = parse_args()
    if args.input.count("-") > 1:
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
