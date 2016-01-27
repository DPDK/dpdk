/*-
 * Copyright (c) 2007-2013 QLogic Corporation. All rights reserved.
 *
 * Eric Davis        <edavis@broadcom.com>
 * David Christensen <davidch@broadcom.com>
 * Gary Zambrano     <zambrano@broadcom.com>
 *
 * Copyright (c) 2013-2015 Brocade Communications Systems, Inc.
 * Copyright (c) 2015 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.bnx2x_pmd for copyright and licensing details.
 */

#include "bnx2x.h"


/*
 * Debug versions of the 8/16/32 bit OS register read/write functions to
 * capture/display values read/written from/to the controller.
 */
void
bnx2x_reg_write8(struct bnx2x_softc *sc, size_t offset, uint8_t val)
{
	PMD_DEBUG_PERIODIC_LOG(DEBUG, "offset=0x%08lx val=0x%02x", (unsigned long)offset, val);
	*((volatile uint8_t*)((uintptr_t)sc->bar[BAR0].base_addr + offset)) = val;
}

void
bnx2x_reg_write16(struct bnx2x_softc *sc, size_t offset, uint16_t val)
{
	if ((offset % 2) != 0) {
		PMD_DRV_LOG(NOTICE, "Unaligned 16-bit write to 0x%08lx",
			    (unsigned long)offset);
	}

	PMD_DEBUG_PERIODIC_LOG(DEBUG, "offset=0x%08lx val=0x%04x", (unsigned long)offset, val);
	*((volatile uint16_t*)((uintptr_t)sc->bar[BAR0].base_addr + offset)) = val;
}

void
bnx2x_reg_write32(struct bnx2x_softc *sc, size_t offset, uint32_t val)
{
	if ((offset % 4) != 0) {
		PMD_DRV_LOG(NOTICE, "Unaligned 32-bit write to 0x%08lx",
			    (unsigned long)offset);
	}

	PMD_DEBUG_PERIODIC_LOG(DEBUG, "offset=0x%08lx val=0x%08x", (unsigned long)offset, val);
	*((volatile uint32_t*)((uintptr_t)sc->bar[BAR0].base_addr + offset)) = val;
}

uint8_t
bnx2x_reg_read8(struct bnx2x_softc *sc, size_t offset)
{
	uint8_t val;

	val = (uint8_t)(*((volatile uint8_t*)((uintptr_t)sc->bar[BAR0].base_addr + offset)));
	PMD_DEBUG_PERIODIC_LOG(DEBUG, "offset=0x%08lx val=0x%02x", (unsigned long)offset, val);

	return val;
}

uint16_t
bnx2x_reg_read16(struct bnx2x_softc *sc, size_t offset)
{
	uint16_t val;

	if ((offset % 2) != 0) {
		PMD_DRV_LOG(NOTICE, "Unaligned 16-bit read from 0x%08lx",
			    (unsigned long)offset);
	}

	val = (uint16_t)(*((volatile uint16_t*)((uintptr_t)sc->bar[BAR0].base_addr + offset)));
	PMD_DEBUG_PERIODIC_LOG(DEBUG, "offset=0x%08lx val=0x%08x", (unsigned long)offset, val);

	return val;
}

uint32_t
bnx2x_reg_read32(struct bnx2x_softc *sc, size_t offset)
{
	uint32_t val;

	if ((offset % 4) != 0) {
		PMD_DRV_LOG(NOTICE, "Unaligned 32-bit read from 0x%08lx",
			    (unsigned long)offset);
		return 0;
	}

	val = (uint32_t)(*((volatile uint32_t*)((uintptr_t)sc->bar[BAR0].base_addr + offset)));
	PMD_DEBUG_PERIODIC_LOG(DEBUG, "offset=0x%08lx val=0x%08x", (unsigned long)offset, val);

	return val;
}
