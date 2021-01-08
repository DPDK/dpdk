# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2020 Dmitry Kozlyuk <dmitry.kozliuk@gmail.com>

import ctypes

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
