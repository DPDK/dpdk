/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2014-2015 Chelsio Communications.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Chelsio Communications nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __T4_PCI_ID_TBL_H__
#define __T4_PCI_ID_TBL_H__

/*
 * The Os-Dependent code can defined cpp macros for creating a PCI Device ID
 * Table.  This is useful because it allows the PCI ID Table to be maintained
 * in a single place and all supporting OSes to get new PCI Device IDs
 * automatically.
 *
 * The macros are:
 *
 * CH_PCI_DEVICE_ID_TABLE_DEFINE_BEGIN
 *   -- Used to start the definition of the PCI ID Table.
 *
 * CH_PCI_DEVICE_ID_FUNCTION
 *   -- The PCI Function Number to use in the PCI Device ID Table.  "0"
 *   -- for drivers attaching to PF0-3, "4" for drivers attaching to PF4,
 *   -- "8" for drivers attaching to SR-IOV Virtual Functions, etc.
 *
 * CH_PCI_DEVICE_ID_FUNCTION2 [optional]
 *   -- If defined, create a PCI Device ID Table with both
 *   -- CH_PCI_DEVICE_ID_FUNCTION and CH_PCI_DEVICE_ID_FUNCTION2 populated.
 *
 * CH_PCI_ID_TABLE_ENTRY(DeviceID)
 *   -- Used for the individual PCI Device ID entries.  Note that we will
 *   -- be adding a trailing comma (",") after all of the entries (and
 *   -- between the pairs of entries if CH_PCI_DEVICE_ID_FUNCTION2 is defined).
 *
 * CH_PCI_DEVICE_ID_TABLE_DEFINE_END
 *   -- Used to finish the definition of the PCI ID Table.  Note that we
 *   -- will be adding a trailing semi-colon (";") here.
 *
 * CH_PCI_DEVICE_ID_BYPASS_SUPPORTED [optional]
 *   -- If defined, indicates that the OS Driver has support for Bypass
 *   -- Adapters.
 */
#ifdef CH_PCI_DEVICE_ID_TABLE_DEFINE_BEGIN

/*
 * Some sanity checks ...
 */
#ifndef CH_PCI_DEVICE_ID_FUNCTION
#error CH_PCI_DEVICE_ID_FUNCTION not defined!
#endif
#ifndef CH_PCI_ID_TABLE_ENTRY
#error CH_PCI_ID_TABLE_ENTRY not defined!
#endif
#ifndef CH_PCI_DEVICE_ID_TABLE_DEFINE_END
#error CH_PCI_DEVICE_ID_TABLE_DEFINE_END not defined!
#endif

/*
 * T4 and later ASICs use a PCI Device ID scheme of 0xVFPP where:
 *
 *   V  = "4" for T4; "5" for T5, etc.
 *   F  = "0" for PF 0..3; "4".."7" for PF4..7; and "8" for VFs
 *   PP = adapter product designation
 *
 * We use this consistency in order to create the proper PCI Device IDs
 * for the specified CH_PCI_DEVICE_ID_FUNCTION.
 */
#ifndef CH_PCI_DEVICE_ID_FUNCTION2
#define CH_PCI_ID_TABLE_FENTRY(devid) \
	CH_PCI_ID_TABLE_ENTRY((devid) | \
			      ((CH_PCI_DEVICE_ID_FUNCTION) << 8))
#else
#define CH_PCI_ID_TABLE_FENTRY(devid) \
	CH_PCI_ID_TABLE_ENTRY((devid) | \
			      ((CH_PCI_DEVICE_ID_FUNCTION) << 8)), \
	CH_PCI_ID_TABLE_ENTRY((devid) | \
			      ((CH_PCI_DEVICE_ID_FUNCTION2) << 8))
#endif

CH_PCI_DEVICE_ID_TABLE_DEFINE_BEGIN
	/*
	 * T5 adapters:
	 */
	CH_PCI_ID_TABLE_FENTRY(0x5000),	/* T580-dbg */
	CH_PCI_ID_TABLE_FENTRY(0x5001),	/* T520-cr */
	CH_PCI_ID_TABLE_FENTRY(0x5002),	/* T522-cr */
	CH_PCI_ID_TABLE_FENTRY(0x5003),	/* T540-cr */
	CH_PCI_ID_TABLE_FENTRY(0x5004),	/* T520-bch */
	CH_PCI_ID_TABLE_FENTRY(0x5005),	/* T540-bch */
	CH_PCI_ID_TABLE_FENTRY(0x5006),	/* T540-ch */
	CH_PCI_ID_TABLE_FENTRY(0x5007),	/* T520-so */
	CH_PCI_ID_TABLE_FENTRY(0x5008),	/* T520-cx */
	CH_PCI_ID_TABLE_FENTRY(0x5009),	/* T520-bt */
	CH_PCI_ID_TABLE_FENTRY(0x500a),	/* T504-bt */
#ifdef CH_PCI_DEVICE_ID_BYPASS_SUPPORTED
	CH_PCI_ID_TABLE_FENTRY(0x500b),	/* B520-sr */
	CH_PCI_ID_TABLE_FENTRY(0x500c),	/* B504-bt */
#endif
	CH_PCI_ID_TABLE_FENTRY(0x500d),	/* T580-cr */
	CH_PCI_ID_TABLE_FENTRY(0x500e),	/* T540-LP-cr */
	CH_PCI_ID_TABLE_FENTRY(0x5010),	/* T580-LP-cr */
	CH_PCI_ID_TABLE_FENTRY(0x5011),	/* T520-LL-cr */
	CH_PCI_ID_TABLE_FENTRY(0x5012),	/* T560-cr */
	CH_PCI_ID_TABLE_FENTRY(0x5013),	/* T580-chr */
	CH_PCI_ID_TABLE_FENTRY(0x5014),	/* T580-so */
	CH_PCI_ID_TABLE_FENTRY(0x5015),	/* T502-bt */
	CH_PCI_ID_TABLE_FENTRY(0x5080),	/* Custom T540-cr */
	CH_PCI_ID_TABLE_FENTRY(0x5081),	/* Custom T540-LL-cr */
	CH_PCI_ID_TABLE_FENTRY(0x5082),	/* Custom T504-cr */
	CH_PCI_ID_TABLE_FENTRY(0x5083),	/* Custom T540-LP-CR */
	CH_PCI_ID_TABLE_FENTRY(0x5084),	/* Custom T580-cr */
	CH_PCI_ID_TABLE_FENTRY(0x5085),	/* Custom 3x T580-CR */
	CH_PCI_ID_TABLE_FENTRY(0x5086),	/* Custom 2x T580-CR */
	CH_PCI_ID_TABLE_FENTRY(0x5087),	/* Custom T580-CR */
	CH_PCI_ID_TABLE_FENTRY(0x5088),	/* Custom T570-CR */
	CH_PCI_ID_TABLE_FENTRY(0x5089),	/* Custom T520-CR */
	CH_PCI_ID_TABLE_FENTRY(0x5090), /* Custom T540-CR */
	CH_PCI_ID_TABLE_FENTRY(0x5091), /* Custom T522-CR */
	CH_PCI_ID_TABLE_FENTRY(0x5092), /* Custom T520-CR */
CH_PCI_DEVICE_ID_TABLE_DEFINE_END;

#endif /* CH_PCI_DEVICE_ID_TABLE_DEFINE_BEGIN */

#endif /* __T4_PCI_ID_TBL_H__ */
