#!/usr/bin/python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Cavium, Inc

ident = []
fname = '/sys/devices/system/cpu/cpu0/regs/identification/midr_el1'
with open(fname) as f:
    content = f.read()

midr_el1 = (int(content.rstrip('\n'), 16))

ident.append(hex((midr_el1 >> 24) & 0xFF))  # Implementer
ident.append(hex((midr_el1 >> 20) & 0xF))   # Variant
ident.append(hex((midr_el1 >> 16) & 0XF))   # Architecture
ident.append(hex((midr_el1 >> 4) & 0xFFF))  # Primary Part number
ident.append(hex(midr_el1 & 0xF))           # Revision

print(' '.join(ident))
