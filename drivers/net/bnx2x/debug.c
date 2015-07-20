/*-
 * Copyright (c) 2007-2013 QLogic Corporation. All rights reserved.
 *
 * Eric Davis        <edavis@broadcom.com>
 * David Christensen <davidch@broadcom.com>
 * Gary Zambrano     <zambrano@broadcom.com>
 *
 * Copyright (c) 2013-2015 Brocade Communications Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Broadcom Corporation nor the name of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written consent.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bnx2x.h"


/*
 * Debug versions of the 8/16/32 bit OS register read/write functions to
 * capture/display values read/written from/to the controller.
 */
void
bnx2x_reg_write8(struct bnx2x_softc *sc, size_t offset, uint8_t val)
{
	PMD_DRV_LOG(DEBUG, "offset=0x%08lx val=0x%02x", offset, val);
	*((volatile uint8_t*)((uint64_t)sc->bar[BAR0].base_addr + offset)) = val;
}

void
bnx2x_reg_write16(struct bnx2x_softc *sc, size_t offset, uint16_t val)
{
	if ((offset % 2) != 0) {
		PMD_DRV_LOG(DEBUG, "Unaligned 16-bit write to 0x%08lx", offset);
	}

	PMD_DRV_LOG(DEBUG, "offset=0x%08lx val=0x%04x", offset, val);
	*((volatile uint16_t*)((uint64_t)sc->bar[BAR0].base_addr + offset)) = val;
}

void
bnx2x_reg_write32(struct bnx2x_softc *sc, size_t offset, uint32_t val)
{
	if ((offset % 4) != 0) {
		PMD_DRV_LOG(DEBUG, "Unaligned 32-bit write to 0x%08lx", offset);
	}

	PMD_DRV_LOG(DEBUG, "offset=0x%08lx val=0x%08x", offset, val);
	*((volatile uint32_t*)((uint64_t)sc->bar[BAR0].base_addr + offset)) = val;
}

uint8_t
bnx2x_reg_read8(struct bnx2x_softc *sc, size_t offset)
{
	uint8_t val;

	val = (uint8_t)(*((volatile uint8_t*)((uint64_t)sc->bar[BAR0].base_addr + offset)));
	PMD_DRV_LOG(DEBUG, "offset=0x%08lx val=0x%02x", offset, val);

	return (val);
}

uint16_t
bnx2x_reg_read16(struct bnx2x_softc *sc, size_t offset)
{
	uint16_t val;

	if ((offset % 2) != 0) {
		PMD_DRV_LOG(DEBUG, "Unaligned 16-bit read from 0x%08lx", offset);
	}

	val = (uint16_t)(*((volatile uint16_t*)((uint64_t)sc->bar[BAR0].base_addr + offset)));
	PMD_DRV_LOG(DEBUG, "offset=0x%08lx val=0x%08x", offset, val);

	return (val);
}

uint32_t
bnx2x_reg_read32(struct bnx2x_softc *sc, size_t offset)
{
	uint32_t val;

	if ((offset % 4) != 0) {
		PMD_DRV_LOG(DEBUG, "Unaligned 32-bit read from 0x%08lx", offset);
		return 0;
	}

	val = (uint32_t)(*((volatile uint32_t*)((uint64_t)sc->bar[BAR0].base_addr + offset)));
	PMD_DRV_LOG(DEBUG, "offset=0x%08lx val=0x%08x", offset, val);

	return (val);
}
