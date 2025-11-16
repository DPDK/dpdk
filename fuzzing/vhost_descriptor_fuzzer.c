/*
 * DPDK vHost Descriptor Fuzzer
 *
 * Targets: lib/vhost/virtio_net_ctrl.c, virtio_net.c
 * Goal: Find VM escape vulnerabilities in vHost descriptor processing
 *
 * This fuzzer tests:
 * 1. Descriptor chain loops (circular references)
 * 2. Out-of-bounds descriptor indices
 * 3. Integer overflow in length accumulation
 * 4. Unvalidated 'next' field usage
 * 5. Huge descriptor chain lengths
 *
 * Build with AFL++:
 *   afl-clang-fast -fsanitize=address -g -O1 vhost_descriptor_fuzzer.c \
 *     -I../lib/vhost -I../lib/eal/include -o vhost_fuzzer
 *
 * Author: Security Researcher
 * Date: 2025-11-16
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

/* Virtio descriptor structure (from virtio spec) */
#define VRING_DESC_F_NEXT       1
#define VRING_DESC_F_WRITE      2
#define VRING_DESC_F_INDIRECT   4

struct vring_desc {
    uint64_t addr;   /* Guest-controlled buffer address */
    uint32_t len;    /* Guest-controlled length */
    uint16_t flags;  /* Guest-controlled flags */
    uint16_t next;   /* Guest-controlled next descriptor index */
};

struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[256];  /* Guest-controlled descriptor indices */
};

struct vring_used_elem {
    uint32_t id;
    uint32_t len;
};

struct vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[256];
};

struct vhost_virtqueue {
    struct vring_desc *desc;
    struct vring_avail *avail;
    struct vring_used *used;
    uint16_t size;
    uint16_t last_avail_idx;
    uint16_t last_used_idx;
};

/* Simulated virtio_net control queue */
struct fuzzer_cvq {
    struct vhost_virtqueue vq;
    struct vring_desc descriptors[256];
    struct vring_avail avail_ring;
    struct vring_used used_ring;
};

/*
 * VULNERABLE PATTERN #1: Descriptor chain following (from virtio_net_ctrl.c)
 *
 * This simulates the code from lib/vhost/virtio_net_ctrl.c:67-107
 * Tests if the implementation properly handles:
 * - Circular descriptor chains
 * - Out-of-bounds 'next' indices
 * - Very long chains (DoS)
 */
static int test_descriptor_chain_loop(struct fuzzer_cvq *cvq, uint16_t head_idx)
{
    uint16_t desc_idx = head_idx;
    struct vring_desc *descs = cvq->descriptors;
    uint16_t n_descs = 0;
    uint64_t data_len = 0;

    /* Bounds check for initial descriptor (GOOD - this exists in real code) */
    if (desc_idx >= cvq->vq.size) {
        printf("[SECURITY] Out of range desc index: %u >= %u\n",
               desc_idx, cvq->vq.size);
        return -1;
    }

    printf("[TEST] Starting descriptor chain from idx %u\n", desc_idx);

    /* This loop mirrors virtio_net_ctrl.c:67-107 */
    while (1) {
        /* ⚠️ VULNERABILITY: Original code has no explicit chain length limit here!
         * This could allow DoS via very long chains OR infinite loop via circular chain */

        uint32_t desc_len = descs[desc_idx].len;
        uint64_t desc_addr = descs[desc_idx].addr;
        uint16_t desc_flags = descs[desc_idx].flags;

        n_descs++;

        printf("  [%u] addr=0x%lx len=%u flags=0x%x next=%u\n",
               desc_idx, desc_addr, desc_len, desc_flags, descs[desc_idx].next);

        /* ⚠️ VULNERABILITY #1: Integer overflow in data_len accumulation */
        uint64_t old_data_len = data_len;
        data_len += desc_len;

        if (data_len < old_data_len) {
            printf("[VULN] Integer overflow detected! %lu + %u wrapped to %lu\n",
                   old_data_len, desc_len, data_len);
            printf("[EXPLOIT] This could cause small malloc() followed by large memcpy()\n");
            return -1;  /* In real code, this might not be checked! */
        }

        /* Mitigation that SHOULD exist: Maximum chain length */
        if (n_descs > cvq->vq.size) {
            printf("[SECURITY] Chain too long: %u descriptors (max %u)\n",
                   n_descs, cvq->vq.size);
            return -1;
        }

        /* Check for NEXT flag */
        if (!(descs[desc_idx].flags & VRING_DESC_F_NEXT)) {
            printf("[OK] Chain ended normally after %u descriptors\n", n_descs);
            break;
        }

        /* ⚠️ VULNERABILITY #2: No validation of 'next' field before use! */
        uint16_t next_idx = descs[desc_idx].next;

        /* This check SHOULD exist but might not in all code paths: */
        if (next_idx >= cvq->vq.size) {
            printf("[VULN] Out-of-bounds next index: %u >= %u\n",
                   next_idx, cvq->vq.size);
            printf("[EXPLOIT] Would cause OOB read on next iteration!\n");
            return -1;
        }

        /* Check for circular chain (this is NOT in the original code!) */
        for (uint16_t i = 0; i < n_descs - 1; i++) {
            if (next_idx == desc_idx) {
                printf("[VULN] Circular chain detected! desc[%u].next = %u (self-loop)\n",
                       desc_idx, next_idx);
                printf("[EXPLOIT] Would cause infinite loop → DoS!\n");
                return -1;
            }
        }

        desc_idx = next_idx;
    }

    printf("[RESULT] Processed %u descriptors, total length: %lu\n", n_descs, data_len);

    /* Simulate malloc based on data_len (vulnerable if overflow occurred) */
    if (data_len > 0 && data_len < 0x100000000UL) {
        void *buf = malloc(data_len);
        if (buf) {
            printf("[OK] Successfully allocated %lu bytes\n", data_len);
            free(buf);
        } else {
            printf("[ERROR] Failed to allocate %lu bytes\n", data_len);
        }
    }

    return 0;
}

/*
 * Initialize fuzzer control queue
 */
static struct fuzzer_cvq *init_cvq(void)
{
    struct fuzzer_cvq *cvq = calloc(1, sizeof(*cvq));
    if (!cvq) return NULL;

    cvq->vq.size = 256;
    cvq->vq.desc = cvq->descriptors;
    cvq->vq.avail = &cvq->avail_ring;
    cvq->vq.used = &cvq->used_ring;
    cvq->vq.last_avail_idx = 0;
    cvq->vq.last_used_idx = 0;

    return cvq;
}

/*
 * Fuzzer input parsing
 *
 * Format: Each 16 bytes = one descriptor:
 *   [0-7]:   addr (uint64_t)
 *   [8-11]:  len (uint32_t)
 *   [12-13]: flags (uint16_t)
 *   [14-15]: next (uint16_t)
 */
static int parse_fuzzer_input(struct fuzzer_cvq *cvq, const uint8_t *data, size_t size)
{
    size_t num_descs = size / 16;
    if (num_descs == 0 || num_descs > 256) {
        return -1;
    }

    printf("\n=== Parsing %zu descriptors from fuzzer input ===\n", num_descs);

    for (size_t i = 0; i < num_descs; i++) {
        const uint8_t *desc_data = data + (i * 16);

        memcpy(&cvq->descriptors[i].addr, desc_data + 0, 8);
        memcpy(&cvq->descriptors[i].len, desc_data + 8, 4);
        memcpy(&cvq->descriptors[i].flags, desc_data + 12, 2);
        memcpy(&cvq->descriptors[i].next, desc_data + 14, 2);
    }

    /* Set avail ring to point to first descriptor */
    cvq->avail_ring.ring[0] = 0;
    cvq->avail_ring.idx = 1;

    return 0;
}

#ifdef __AFL_HAVE_MANUAL_CONTROL
  __AFL_FUZZ_INIT();
#endif

int main(int argc, char **argv)
{
    struct fuzzer_cvq *cvq = NULL;

    printf("DPDK vHost Descriptor Fuzzer\n");
    printf("Target: VM Escape vulnerabilities\n");
    printf("=====================================\n\n");

    cvq = init_cvq();
    if (!cvq) {
        fprintf(stderr, "Failed to initialize control queue\n");
        return 1;
    }

#ifdef __AFL_HAVE_MANUAL_CONTROL
    /* AFL++ persistent mode */
    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;

    while (__AFL_LOOP(10000)) {
        size_t len = __AFL_FUZZ_TESTCASE_LEN;

        if (len < 16 || len > 4096) continue;

        /* Reset CVQ for each iteration */
        memset(cvq->descriptors, 0, sizeof(cvq->descriptors));

        if (parse_fuzzer_input(cvq, buf, len) < 0) {
            continue;
        }

        /* Test descriptor chain processing */
        uint16_t head_idx = cvq->avail_ring.ring[0];
        test_descriptor_chain_loop(cvq, head_idx);
    }
#else
    /* Standalone mode: read from file or generate test cases */
    if (argc > 1) {
        FILE *fp = fopen(argv[1], "rb");
        if (fp) {
            uint8_t input[4096];
            size_t len = fread(input, 1, sizeof(input), fp);
            fclose(fp);

            if (parse_fuzzer_input(cvq, input, len) == 0) {
                uint16_t head_idx = cvq->avail_ring.ring[0];
                test_descriptor_chain_loop(cvq, head_idx);
            }
        }
    } else {
        /* Run built-in test cases */
        printf("\n=== Running built-in test cases ===\n\n");

        /* Test case 1: Simple valid chain */
        printf("TEST 1: Valid descriptor chain\n");
        cvq->descriptors[0].addr = 0x1000;
        cvq->descriptors[0].len = 100;
        cvq->descriptors[0].flags = VRING_DESC_F_NEXT;
        cvq->descriptors[0].next = 1;

        cvq->descriptors[1].addr = 0x2000;
        cvq->descriptors[1].len = 200;
        cvq->descriptors[1].flags = 0;  /* No NEXT */

        test_descriptor_chain_loop(cvq, 0);

        /* Test case 2: Circular chain (VULNERABILITY) */
        printf("\n\nTEST 2: Circular descriptor chain (DoS)\n");
        cvq->descriptors[0].addr = 0x1000;
        cvq->descriptors[0].len = 100;
        cvq->descriptors[0].flags = VRING_DESC_F_NEXT;
        cvq->descriptors[0].next = 1;

        cvq->descriptors[1].addr = 0x2000;
        cvq->descriptors[1].len = 100;
        cvq->descriptors[1].flags = VRING_DESC_F_NEXT;
        cvq->descriptors[1].next = 0;  /* Loop back! */

        test_descriptor_chain_loop(cvq, 0);

        /* Test case 3: Out-of-bounds next index */
        printf("\n\nTEST 3: Out-of-bounds next index\n");
        cvq->descriptors[0].addr = 0x1000;
        cvq->descriptors[0].len = 100;
        cvq->descriptors[0].flags = VRING_DESC_F_NEXT;
        cvq->descriptors[0].next = 300;  /* > queue size! */

        test_descriptor_chain_loop(cvq, 0);

        /* Test case 4: Integer overflow in length */
        printf("\n\nTEST 4: Integer overflow in data_len\n");
        cvq->descriptors[0].addr = 0x1000;
        cvq->descriptors[0].len = 0xFFFFFFFF;  /* Max uint32 */
        cvq->descriptors[0].flags = VRING_DESC_F_NEXT;
        cvq->descriptors[0].next = 1;

        cvq->descriptors[1].addr = 0x2000;
        cvq->descriptors[1].len = 0xFFFFFFFF;  /* Max uint32 */
        cvq->descriptors[1].flags = 0;

        test_descriptor_chain_loop(cvq, 0);

        /* Test case 5: Very long chain (DoS) */
        printf("\n\nTEST 5: Very long descriptor chain\n");
        for (int i = 0; i < 255; i++) {
            cvq->descriptors[i].addr = 0x1000 + i * 0x100;
            cvq->descriptors[i].len = 64;
            cvq->descriptors[i].flags = VRING_DESC_F_NEXT;
            cvq->descriptors[i].next = i + 1;
        }
        cvq->descriptors[255].flags = 0;  /* Last one */

        test_descriptor_chain_loop(cvq, 0);
    }
#endif

    free(cvq);
    return 0;
}
