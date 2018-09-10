/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2016-2018 Solarflare Communications Inc.
 * All rights reserved.
 */

/*
 * This is NOT the original source file. Do NOT edit it.
 * To update the image layout headers, please edit the copy in
 * the sfregistry repo and then, in that repo,
 * "make layout_headers" or "make export" to
 * regenerate and export all types of headers.
 */

/* These structures define the layouts for the signed firmware image binary
 * saved in NVRAM. The original image is in the Cryptographic message
 * syntax (CMS) format which contains the bootable firmware binary plus the
 * signatures. The entire image is written into NVRAM to enable the firmware
 * to validate the signatures. However, the bootrom still requires the
 * bootable-image to start at offset 0 of the NVRAM partition. Hence the image
 * is parsed upfront by host utilities (sfupdate) and written into nvram as
 * 'signed_image_chunks' described by a header.
 *
 * This file is used by the MC as well as host-utilities (sfupdate).
 */


#ifndef CI_MGMT_SIGNED_IMAGE_LAYOUT_H
#define CI_MGMT_SIGNED_IMAGE_LAYOUT_H

/* Signed image chunk type identifiers */
enum {
  SIGNED_IMAGE_CHUNK_CMS_HEADER,        /* CMS header describing the signed data */
  SIGNED_IMAGE_CHUNK_REFLASH_HEADER,    /* Reflash header */
  SIGNED_IMAGE_CHUNK_IMAGE,             /* Bootable binary image */
  SIGNED_IMAGE_CHUNK_REFLASH_TRAILER,   /* Reflash trailer */
  SIGNED_IMAGE_CHUNK_SIGNATURE,         /* Remaining contents of the signed image,
                                         * including the certifiates and signature */
  NUM_SIGNED_IMAGE_CHUNKS,
};

/* Magic */
#define SIGNED_IMAGE_CHUNK_HDR_MAGIC 0xEF105161 /* EF10 SIGned Image */

/* Initial version definition - version 1 */
#define SIGNED_IMAGE_CHUNK_HDR_VERSION 0x1

/* Header length is 32 bytes */
#define SIGNED_IMAGE_CHUNK_HDR_LEN   32
/* Structure describing the header of each chunk of signed image
 * as stored in nvram
 */
typedef struct signed_image_chunk_hdr_e {
  /* Magic field to recognise a valid entry
   * should match SIGNED_IMAGE_CHUNK_HDR_MAGIC
   */
  uint32_t magic;
  /* Version number of this header */
  uint32_t version;
  /* Chunk type identifier */
  uint32_t id;
  /* Chunk offset */
  uint32_t offset;
  /* Chunk length */
  uint32_t len;
  /* Reserved for future expansion of this structure - always set to zeros */
  uint32_t reserved[3];
} signed_image_chunk_hdr_t;

#endif /* CI_MGMT_SIGNED_IMAGE_LAYOUT_H */
