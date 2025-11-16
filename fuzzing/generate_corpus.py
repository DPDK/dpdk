#!/usr/bin/env python3
"""
Corpus Generator for DPDK vHost Descriptor Fuzzer

Generates test cases for fuzzing DPDK vHost descriptor processing.
Each test case is designed to trigger specific vulnerability patterns:

1. Valid baselines
2. Circular descriptor chains (DoS)
3. Out-of-bounds indices (OOB read)
4. Integer overflow in lengths (buffer overflow)
5. Very long chains (DoS)
6. Edge cases (zero length, huge addresses, etc.)

Descriptor Format (16 bytes per descriptor):
  Bytes 0-7:   addr (uint64_t, little-endian)
  Bytes 8-11:  len (uint32_t, little-endian)
  Bytes 12-13: flags (uint16_t, little-endian)
  Bytes 14-15: next (uint16_t, little-endian)

Author: Security Researcher
Date: 2025-11-16
"""

import struct
import os
import sys

# Virtio descriptor flags
VRING_DESC_F_NEXT = 1
VRING_DESC_F_WRITE = 2
VRING_DESC_F_INDIRECT = 4

def create_descriptor(addr, length, flags, next_idx=0):
    """
    Pack a virtio descriptor into 16-byte binary format

    Returns: bytes object (16 bytes)
    """
    return struct.pack("<QIHH", addr, length, flags, next_idx)

def write_test_case(corpus_dir, name, descriptors):
    """Write a test case to the corpus directory"""
    filepath = os.path.join(corpus_dir, name)
    with open(filepath, "wb") as f:
        for desc in descriptors:
            f.write(desc)
    print(f"  [+] {name}: {len(descriptors)} descriptor(s)")

def generate_valid_corpus(corpus_dir):
    """Generate valid baseline test cases"""
    print("\n[*] Generating valid baseline corpus...")

    # Test 1: Single valid descriptor
    desc = create_descriptor(
        addr=0x1000,
        length=1500,
        flags=0,
        next_idx=0
    )
    write_test_case(corpus_dir, "valid_single", [desc])

    # Test 2: Simple chain (2 descriptors)
    chain = [
        create_descriptor(0x1000, 100, VRING_DESC_F_NEXT, 1),
        create_descriptor(0x2000, 200, 0, 0)
    ]
    write_test_case(corpus_dir, "valid_chain_2", chain)

    # Test 3: Longer chain (5 descriptors)
    chain = [
        create_descriptor(0x1000, 512, VRING_DESC_F_NEXT, 1),
        create_descriptor(0x2000, 512, VRING_DESC_F_NEXT, 2),
        create_descriptor(0x3000, 512, VRING_DESC_F_NEXT, 3),
        create_descriptor(0x4000, 512, VRING_DESC_F_NEXT, 4),
        create_descriptor(0x5000, 512, 0, 0)
    ]
    write_test_case(corpus_dir, "valid_chain_5", chain)

    # Test 4: Write descriptor
    desc = create_descriptor(0x10000, 4096, VRING_DESC_F_WRITE, 0)
    write_test_case(corpus_dir, "valid_write", [desc])

    # Test 5: Indirect descriptor
    desc = create_descriptor(0x20000, 1024, VRING_DESC_F_INDIRECT, 0)
    write_test_case(corpus_dir, "valid_indirect", [desc])

    # Test 6: Mixed flags
    desc = create_descriptor(0x30000, 2048,
                           VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 1)
    chain = [desc, create_descriptor(0x40000, 1024, 0, 0)]
    write_test_case(corpus_dir, "valid_mixed_flags", chain)

def generate_malicious_corpus(corpus_dir):
    """Generate malicious test cases targeting known vulnerability patterns"""
    print("\n[*] Generating malicious test cases...")

    # VULN 1: Circular chain (2-descriptor loop)
    chain = [
        create_descriptor(0x1000, 100, VRING_DESC_F_NEXT, 1),
        create_descriptor(0x2000, 100, VRING_DESC_F_NEXT, 0)  # Points back to 0!
    ]
    write_test_case(corpus_dir, "vuln_circular_2", chain)

    # VULN 2: Self-loop (1-descriptor loop)
    desc = create_descriptor(0x1000, 100, VRING_DESC_F_NEXT, 0)  # Points to self!
    write_test_case(corpus_dir, "vuln_self_loop", [desc])

    # VULN 3: Circular chain (3-descriptor loop)
    chain = [
        create_descriptor(0x1000, 100, VRING_DESC_F_NEXT, 1),
        create_descriptor(0x2000, 100, VRING_DESC_F_NEXT, 2),
        create_descriptor(0x3000, 100, VRING_DESC_F_NEXT, 0)  # Loop back!
    ]
    write_test_case(corpus_dir, "vuln_circular_3", chain)

    # VULN 4: Out-of-bounds next index (just out of range)
    desc = create_descriptor(0x1000, 100, VRING_DESC_F_NEXT, 256)  # Max valid is 255
    write_test_case(corpus_dir, "vuln_oob_next_256", [desc])

    # VULN 5: Out-of-bounds next index (very large)
    desc = create_descriptor(0x1000, 100, VRING_DESC_F_NEXT, 65535)
    write_test_case(corpus_dir, "vuln_oob_next_max", [desc])

    # VULN 6: Integer overflow in length (2 huge descriptors)
    chain = [
        create_descriptor(0x1000, 0xFFFFFFFF, VRING_DESC_F_NEXT, 1),  # Max uint32
        create_descriptor(0x2000, 0xFFFFFFFF, 0, 0)  # Sum overflows!
    ]
    write_test_case(corpus_dir, "vuln_length_overflow_2", chain)

    # VULN 7: Integer overflow (4 large descriptors)
    chain = [
        create_descriptor(0x1000, 0x80000000, VRING_DESC_F_NEXT, 1),
        create_descriptor(0x2000, 0x80000000, VRING_DESC_F_NEXT, 2),
        create_descriptor(0x3000, 0x80000000, VRING_DESC_F_NEXT, 3),
        create_descriptor(0x4000, 0x80000000, 0, 0)
    ]
    write_test_case(corpus_dir, "vuln_length_overflow_4", chain)

    # VULN 8: Very long chain (DoS)
    chain = []
    for i in range(256):
        flags = VRING_DESC_F_NEXT if i < 255 else 0
        next_idx = (i + 1) if i < 255 else 0
        chain.append(create_descriptor(0x1000 + i * 0x100, 64, flags, next_idx))
    write_test_case(corpus_dir, "vuln_long_chain_256", chain)

def generate_edge_cases(corpus_dir):
    """Generate edge case test inputs"""
    print("\n[*] Generating edge cases...")

    # Edge 1: Zero length
    desc = create_descriptor(0x1000, 0, 0, 0)
    write_test_case(corpus_dir, "edge_zero_length", [desc])

    # Edge 2: Length = 1
    desc = create_descriptor(0x1000, 1, 0, 0)
    write_test_case(corpus_dir, "edge_length_1", [desc])

    # Edge 3: Maximum valid length
    desc = create_descriptor(0x1000, 65536, 0, 0)
    write_test_case(corpus_dir, "edge_length_64k", [desc])

    # Edge 4: NULL address
    desc = create_descriptor(0x0, 100, 0, 0)
    write_test_case(corpus_dir, "edge_null_addr", [desc])

    # Edge 5: Very high address (potential host memory)
    desc = create_descriptor(0x7FFFFFFFFFFF, 4096, VRING_DESC_F_WRITE, 0)
    write_test_case(corpus_dir, "edge_high_addr", [desc])

    # Edge 6: Unaligned address
    desc = create_descriptor(0x1001, 100, 0, 0)  # Not aligned to cache line
    write_test_case(corpus_dir, "edge_unaligned_addr", [desc])

    # Edge 7: All flags set
    desc = create_descriptor(0x1000, 100, 0xFFFF, 0)
    write_test_case(corpus_dir, "edge_all_flags", [desc])

    # Edge 8: Empty chain (no descriptors)
    write_test_case(corpus_dir, "edge_empty", [])

    # Edge 9: Single byte descriptor
    write_test_case(corpus_dir, "edge_single_byte", [b'\x00'])

    # Edge 10: Partial descriptor (15 bytes instead of 16)
    partial = create_descriptor(0x1000, 100, 0, 0)[:-1]
    write_test_case(corpus_dir, "edge_partial_desc", [partial])

def generate_combined_attacks(corpus_dir):
    """Generate test cases combining multiple attack vectors"""
    print("\n[*] Generating combined attack scenarios...")

    # Combined 1: Circular chain + huge lengths
    chain = [
        create_descriptor(0x1000, 0xFFFFFFFF, VRING_DESC_F_NEXT, 1),
        create_descriptor(0x2000, 0xFFFFFFFF, VRING_DESC_F_NEXT, 0)  # Loop!
    ]
    write_test_case(corpus_dir, "combined_circular_overflow", chain)

    # Combined 2: OOB index + write flag
    desc = create_descriptor(0x7FFF00000000, 4096,
                           VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 300)
    write_test_case(corpus_dir, "combined_oob_write", [desc])

    # Combined 3: Indirect + circular
    chain = [
        create_descriptor(0x1000, 1024,
                        VRING_DESC_F_INDIRECT | VRING_DESC_F_NEXT, 1),
        create_descriptor(0x2000, 512, VRING_DESC_F_NEXT, 0)  # Loop back
    ]
    write_test_case(corpus_dir, "combined_indirect_circular", chain)

    # Combined 4: Very long chain with overflow
    chain = []
    for i in range(100):
        chain.append(create_descriptor(
            0x1000 + i * 0x100,
            0x10000000,  # 256MB each
            VRING_DESC_F_NEXT if i < 99 else 0,
            i + 1 if i < 99 else 0
        ))
    write_test_case(corpus_dir, "combined_long_overflow", chain)

def generate_protocol_fuzzing(corpus_dir):
    """Generate protocol-level fuzzing cases"""
    print("\n[*] Generating protocol fuzzing cases...")

    # Protocol 1: Inconsistent NEXT flag (flag set but next=0)
    desc = create_descriptor(0x1000, 100, VRING_DESC_F_NEXT, 0)
    write_test_case(corpus_dir, "protocol_next_to_self", [desc])

    # Protocol 2: NEXT flag not set but next != 0
    desc = create_descriptor(0x1000, 100, 0, 5)
    write_test_case(corpus_dir, "protocol_no_flag_but_next", [desc])

    # Protocol 3: Write and indirect together (unusual)
    desc = create_descriptor(0x1000, 1024,
                           VRING_DESC_F_WRITE | VRING_DESC_F_INDIRECT, 0)
    write_test_case(corpus_dir, "protocol_write_indirect", [desc])

def main():
    corpus_dir = "corpus_vhost"

    print("=" * 60)
    print("DPDK vHost Descriptor Fuzzer - Corpus Generator")
    print("=" * 60)

    # Create corpus directory
    os.makedirs(corpus_dir, exist_ok=True)
    print(f"\n[+] Corpus directory: {corpus_dir}/")

    # Generate all test cases
    generate_valid_corpus(corpus_dir)
    generate_malicious_corpus(corpus_dir)
    generate_edge_cases(corpus_dir)
    generate_combined_attacks(corpus_dir)
    generate_protocol_fuzzing(corpus_dir)

    # Statistics
    files = os.listdir(corpus_dir)
    total_size = sum(os.path.getsize(os.path.join(corpus_dir, f)) for f in files)

    print("\n" + "=" * 60)
    print("Corpus Generation Complete!")
    print("=" * 60)
    print(f"Total test cases: {len(files)}")
    print(f"Total corpus size: {total_size} bytes")
    print(f"\nCorpus breakdown:")
    print(f"  - Valid baselines: 6")
    print(f"  - Malicious cases: 8")
    print(f"  - Edge cases: 10")
    print(f"  - Combined attacks: 4")
    print(f"  - Protocol fuzzing: 3")
    print(f"\nNext steps:")
    print(f"  1. Build the fuzzer: ./build_fuzzer.sh")
    print(f"  2. Test corpus: ./vhost_fuzzer_standalone corpus_vhost/vuln_*")
    print(f"  3. Start AFL++: afl-fuzz -i {corpus_dir} -o findings -- ./vhost_fuzzer_afl")
    print()

if __name__ == "__main__":
    main()
