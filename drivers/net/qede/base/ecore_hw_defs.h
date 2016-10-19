/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef _ECORE_IGU_DEF_H_
#define _ECORE_IGU_DEF_H_

/* Fields of IGU PF CONFIGRATION REGISTER */
/* function enable        */
#define IGU_PF_CONF_FUNC_EN       (0x1 << 0)
/* MSI/MSIX enable        */
#define IGU_PF_CONF_MSI_MSIX_EN   (0x1 << 1)
/* INT enable             */
#define IGU_PF_CONF_INT_LINE_EN   (0x1 << 2)
/* attention enable       */
#define IGU_PF_CONF_ATTN_BIT_EN   (0x1 << 3)
/* single ISR mode enable */
#define IGU_PF_CONF_SINGLE_ISR_EN (0x1 << 4)
/* simd all ones mode     */
#define IGU_PF_CONF_SIMD_MODE     (0x1 << 5)

/* Fields of IGU VF CONFIGRATION REGISTER */
/* function enable        */
#define IGU_VF_CONF_FUNC_EN        (0x1 << 0)
/* MSI/MSIX enable        */
#define IGU_VF_CONF_MSI_MSIX_EN    (0x1 << 1)
/* single ISR mode enable */
#define IGU_VF_CONF_SINGLE_ISR_EN  (0x1 << 4)
/* Parent PF              */
#define IGU_VF_CONF_PARENT_MASK    (0xF)
/* Parent PF              */
#define IGU_VF_CONF_PARENT_SHIFT   5

/* Igu control commands
 */
enum igu_ctrl_cmd {
	IGU_CTRL_CMD_TYPE_RD,
	IGU_CTRL_CMD_TYPE_WR,
	MAX_IGU_CTRL_CMD
};

/* Control register for the IGU command register
 */
struct igu_ctrl_reg {
	u32 ctrl_data;
#define IGU_CTRL_REG_FID_MASK		0xFFFF /* Opaque_FID	 */
#define IGU_CTRL_REG_FID_SHIFT		0
#define IGU_CTRL_REG_PXP_ADDR_MASK	0xFFF /* Command address */
#define IGU_CTRL_REG_PXP_ADDR_SHIFT	16
#define IGU_CTRL_REG_RESERVED_MASK	0x1
#define IGU_CTRL_REG_RESERVED_SHIFT	28
#define IGU_CTRL_REG_TYPE_MASK		0x1 /* use enum igu_ctrl_cmd */
#define IGU_CTRL_REG_TYPE_SHIFT		31
};

#endif
