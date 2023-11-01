# SPDX-License-Identifier: BSD-3-Clause
# This file is not built as part of normal DPDK build.
# It is used to generate the eBPF code for TAP RSS.

CLANG=clang
CLANG_OPTS=-O2
TARGET=../tap_bpf_insns.h

all: $(TARGET)

clean:
	rm tap_bpf_program.o $(TARGET)

tap_bpf_program.o: tap_bpf_program.c
	$(CLANG) $(CLANG_OPTS) -emit-llvm -c $< -o - | \
	llc -march=bpf -filetype=obj -o $@

$(TARGET): tap_bpf_program.o
	python3 bpf_extract.py -stap_bpf_program.c -o $@ $<
