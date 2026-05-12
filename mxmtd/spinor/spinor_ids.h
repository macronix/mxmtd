/*
 * Copyright (c) 2025 Macronix International Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _SPINOR_IDS_H_
#define _SPINOR_IDS_H_

#include "spinor_cmd.h"

#define BIT(x) (1U << (x))

#define SIZE_PAGE	BIT(8)
#define SIZE_16MB	BIT(24)

#define SIZE_4KB    BIT(12)
#define SIZE_32KB   BIT(15)
#define SIZE_64KB   BIT(16)

#define MAX_ID_LEN	3

#define ADDR_3B		3
#define ADDR_4B		4

/* define spi nor special flags */
#define SPCL_RWW            BIT(0)
#define SPCL_CR_DC_BIT6_7   BIT(1)
#define SPCL_SR_QE_BIT6     BIT(2)
#define SPCL_SCUR_WPSEL		BIT(3)
#define SPCL_CR_ODS         BIT(4)
#define SPCL_ADDR_4B	    BIT(5)
#define SPCL_ADDR_3B   	    BIT(6)
#define SPCL_RDPASS_ADDR    BIT(7)
#define SPCL_SCUR_NO_LDSO	BIT(8)
#define SPCL_WORD_MODE		BIT(9)
#define SPCL_OPI_CMD_CMD	BIT(10)
#define SPCL_PREAMBLE		BIT(11)
#define SPCL_LR_SPBLD_BIT6	BIT(13)
#define SPCL_LR_SPBLD_BIT1	BIT(14)
#define SPCL_LR_PWDPMLD		BIT(15)
#define SPCL_ASP_4KB_IN_BLK	BIT(16)

/**
 * List for ID & REG related command, cmd_list.Xspi[0]
 **/

/* ID CMDs [11:00] */
#define MXIC_RES		BIT(0) /* Read Electronic Signature */
#define MXIC_REMS		BIT(1) /* Read Electronic Manufacturer ID & Device ID */
#define MXIC_REMS2		BIT(2) /* Read Electronic Manufacturer ID & Device ID for 2xIO */
#define MXIC_REMS4		BIT(3) /* Read Electronic Manufacturer ID & Device ID for 4xIO */
#define MXIC_REMS4D		BIT(4) /* Read Electronic Manufacturer ID & Device ID for 4xIO-DDR */
#define MXIC_QPIID		BIT(5) /* Read ID in QPI mode */
#define MXIC_RDID		BIT(6) /* Read ID in SPI/OPI mode */

/* REG CMDs [12:31] */
#define MXIC_WRSR		BIT(0) /* Write Status Register */
#define MXIC_WRSCUR		BIT(1) /* Write Security Register */
#define MXIC_RDSCUR		BIT(2) /* Read Security Register */
#define MXIC_RDCR		BIT(3) /* Read Configuration Register */
#define MXIC_CLSR		BIT(4) /* Clear Status Register Fail Flags */
#define MXIC_CR2		BIT(5) /* RDCR2, WRCR2: Read/Write Configuration Register 2 */
#define MXIC_EAR		BIT(6) /* RDEAR, WREAR: Read/Write Extended Address Register */
#define MXIC_FBR		BIT(7) /* RDFBR, WRFBR ESFBR: Read/Write/Erase Fast Boot Register */
#define MXIC_LR			BIT(8) /* RDLR, WRLR: Read/Write Lock Register */
#define MXIC_PASS		BIT(9) /* RDPASS, WRPASS PASSULK: ASP Password Protect */
#define MXIC_SPB		BIT(10) /* RDSPB, WRSPB ESSPB: Read/Write/Erase Solid Protection Mode */
#define MXIC_SPBLK		BIT(11) /* SPBLK, RDSPBLK: SPB Lock Bit */
#define MXIC_DPB		BIT(12) /* RDDPB, WRDPB: Read/Write Dynamic  Protection Mode */
#define MXIC_WRCR		BIT(13) /* Write Configuration Register */
#define MXIC_RDFSR		BIT(14) /* Read Factory Register */
#define MXIC_CFIRD		BIT(15) /* READ CFI */
#define MXIC_RDDMC		BIT(16) /* READ DMC */	
#define MXIC_RDSFDP		BIT(17) /* READ SFDP */

/**
 * List for MODE SETTING & RESET & SECURITY & SUSPEND related command, cmd_list.spi[1]
 **/

/* MODE SETTING CMDs [15:00]*/
#define MXIC_DP        BIT(0) /* Deep Power Down Mode */
#define MXIC_RDP       BIT(1) /* Release from Deep Power Down Mode */
#define MXIC_SO        BIT(2) /* Enter/Exit Secured OTP */
#define MXIC_SA        BIT(3) /* Enter/Exit Secured Area */
#define MXIC_SRY       BIT(4) /* Enable/Disable SO to Output Ready/Busy */
#define MXIC_EQIO      BIT(5) /* Enable QPI */
#define MXIC_WPSEL     BIT(6) /* Enable block protect mode */
#define MXIC_SBL       BIT(7) /* Set Burst Length */
#define MXIC_ENPLM     BIT(8) /* Enter Parallel Mode, 8I/O */
#define MXIC_EXPLM     BIT(9) /* Exit Parallel Mode, 8I/O */
#define MXIC_4B        BIT(10) /* Enter/Exit 4-byte addressing mdoe */
#define MXIC_HDE       BIT(11) /* Hold# Enable */
#define MXIC_FMEN      BIT(12) /* Factory Mode Enable */

/* Reset CMDs [19:16]*/
#define MXIC_RST       BIT(16) /* RSTEN, RST */
#define MXIC_RSTQIO    BIT(17) /* reset QIO */
#define MXIC_NOP       BIT(18)

/* Security CMDs [30:20]*/
#define MXIC_SBLK      BIT(20) /* SBLK, SBULK, RDBLOCK */
#define MXIC_GBLK      BIT(21) /* GBLK, GBULK */
#define MXIC_KEY       BIT(22) /* KEY1, KEY2 */
#define MXIC_BLOCKP    BIT(23) /* BLOCKP, RDBLOCK2, UNLOCK */
#define MXIC_WRLB      BIT(24) /* Write Read-Lock Reg. bit2 */
#define MXIC_PLOCK     BIT(25) /* PLOCK(Permanent Lock), RDPLOCK(Read Permanent Lock Status) */
#define MXIC_RLCR      BIT(26) /* WRLCR(Write Read-Lock Reg.), RRLCR(Read Read-Lock Reg.) */
#define MXIC_PLLK      BIT(27) /* PLLK(Permanent Lock Bit Lock Down), RDPLLK(Read Permanent Lock Bit Lock Down) */

#define MXIC_QPI_CMDS	(MXIC_EQIO | MXIC_RSTQIO)

/* Suspend / Resume CMDs [31:31]*/
#define MXIC_SUS_RES   BIT(31)  /* Suspend/Resume Program or Erase */

/**
 * List for PROGRAM related command, cmd_list.Xspi[2]
 **/

/* PROGRAM CMDs [15:00] */
#define CMD_IDX_PGM_START	fmsb32(MXIC_4PP4B)
#define CMD_IDX_PGM_END		fmsb32(MXIC_PP)

enum IDX_CMD_PGM {
	MXIC_PP =		BIT(0),
	MXIC_PP4B = 	BIT(1),
	MXIC_QPP =		BIT(2),
	MXIC_QPP4B =	BIT(3),
	MXIC_4PP =		BIT(4),
	MXIC_4PP4B =	BIT(5),
};

static cmd_info_t cmd_info_pgm[] = {
    {SPINOR_PP,			ADDR_3B, {PROT_1S_1S_1S}},
	{SPINOR_PP4B,		ADDR_4B, {PROT_1S_1S_1S, 0, PROT_8S_8S_8S, PROT_8D_8D_8D},},
	{SPINOR_QPP,		ADDR_3B, {PROT_1S_1S_4S}},
    {SPINOR_QPP4B,		ADDR_4B, {PROT_1S_1S_4S}},
    {SPINOR_4PP,		ADDR_3B, {PROT_1S_4S_4S, PROT_4S_4S_4S}},
    {SPINOR_4PP4B,		ADDR_4B, {PROT_1S_4S_4S, PROT_4S_4S_4S}},
};

#define MXIC_OPI_PGM_CMDS   SPINOR_PP4B
#define MXIC_QE_PGM_CMDS    (MXIC_QPP | MXIC_QPP4B | MXIC_4PP | MXIC_4PP4B)

/**
 * List for ERASE related command, cmd_list.Xspi[3]
 **/
#define CMD_IDX_ERS_START	fmsb32(MXIC_BE4B)
#define CMD_IDX_ERS_END		fmsb32(MXIC_SE)

enum IDX_CMD_ERS {
	MXIC_SE =		BIT(0),
	MXIC_SE4B = 	BIT(1),
	MXIC_BE32K =	BIT(2),
	MXIC_BE32K4B =	BIT(3),
	MXIC_BE =		BIT(4),
	MXIC_BE4B =		BIT(5),
	MXIC_CE =		BIT(6), /* Chip ERS */
};

static cmd_info_t cmd_info_ers[] = {
	{SPINOR_SE, 		ADDR_3B, {PROT_1S_1S_1S, PROT_4S_4S_4S}},
	{SPINOR_SE4B, 		ADDR_4B, {PROT_1S_1S_1S, PROT_4S_4S_4S,	PROT_8S_8S_8S, PROT_8D_8D_8D}},
	{SPINOR_BE32K,		ADDR_3B, {PROT_1S_1S_1S, PROT_4S_4S_4S}},
	{SPINOR_BE32K4B,	ADDR_4B, {PROT_1S_1S_1S, PROT_4S_4S_4S,	PROT_8S_8S_8S, PROT_8D_8D_8D}},
	{SPINOR_BE,			ADDR_3B, {PROT_1S_1S_1S, PROT_4S_4S_4S}},
	{SPINOR_BE4B,		ADDR_4B, {PROT_1S_1S_1S, PROT_4S_4S_4S,	PROT_8S_8S_8S, PROT_8D_8D_8D}},
};

#define MXIC_4B_ERS_CMDS        (MXIC_SE4B | MXIC_BE32K4B | MXIC_BE4B)
#define MXIC_OPI_ERS_CMDS	    (MXIC_SE4B | MXIC_BE32K4B | MXIC_BE4B)
#define MXIC_OPI_PGM_ERS_CMDS   (MXIC_OPI_PGM_CMDS | MXIC_OPI_ERS_CMDS)
#define MXIC_QPI_ERS_CMDS		(MXIC_SE | MXIC_SE4B | MXIC_BE32K | MXIC_BE32K4B | MXIC_BE | MXIC_BE4B)
#define MXIC_QPI_PGM_CMDS		(MXIC_4PP | MXIC_4PP4B)
#define MXIC_QPI_PGM_ERS_CMDS	(MXIC_QPI_ERS_CMDS | MXIC_QPI_PGM_CMDS)

/**
 * List for READ related command, cmd_list.Xspi[4]
 **/

/* READ CMDs [31:00] */
#define CMD_IDX_RD_START	fmsb32(MXIC_8DTRD) /* exclude 8DTRD & 8READ */
#define CMD_IDX_RD_END		fmsb32(MXIC_READ)

enum IDX_CMD_RD {
	MXIC_READ =		BIT(0),
	MXIC_READ4B =	BIT(1),
	MXIC_FREAD =	BIT(2),
	MXIC_FREAD4B =	BIT(3),
	MXIC_FDTRD =	BIT(4),
	MXIC_FDTRD4B =	BIT(5),
	MXIC_DREAD =	BIT(6),
	MXIC_DREAD4B =	BIT(7),
	MXIC_2READ =	BIT(8),
	MXIC_2READ4B =	BIT(9),
	MXIC_2DTR =		BIT(10),
	MXIC_2DTR4B =	BIT(11),
	MXIC_QREAD =	BIT(12),
	MXIC_QREAD4B =	BIT(13),
	MXIC_W4READ =	BIT(14),
	MXIC_4READ =	BIT(15),
	MXIC_4READ4B =	BIT(16),
	MXIC_4DTRD =	BIT(17),
	MXIC_4DTR4B =	BIT(18),
	MXIC_OREAD =	BIT(19),
	MXIC_OREAD4B =	BIT(20),
	MXIC_8READ =	BIT(21),
	MXIC_8DTRD =	BIT(22),
};

static cmd_info_t cmd_info_rd[] = {
	{SPINOR_READ,		ADDR_3B, {PROT_1S_1S_1S}},
	{SPINOR_READ4B,		ADDR_4B, {PROT_1S_1S_1S}},
	{SPINOR_FREAD,		ADDR_3B, {PROT_1S_1S_1S, PROT_4S_4S_4S}},
	{SPINOR_FREAD4B,	ADDR_4B, {PROT_1S_1S_1S, PROT_4S_4S_4S}},
	{SPINOR_FDTRD,		ADDR_3B, {PROT_1S_1D_1D, PROT_4S_4D_4D}},
	{SPINOR_FDTRD4B,	ADDR_4B, {PROT_1S_1D_1D, PROT_4S_4D_4D}},
	{SPINOR_DREAD,		ADDR_3B, {PROT_1S_1S_2S}},
	{SPINOR_DREAD4B,	ADDR_4B, {PROT_1S_1S_2S}},
	{SPINOR_2READ,		ADDR_3B, {PROT_1S_2S_2S}},
	{SPINOR_2READ4B,	ADDR_4B, {PROT_1S_2S_2S}},
	{SPINOR_2DTRD,		ADDR_3B, {PROT_1S_2D_2D}},
	{SPINOR_2DTRD4B,	ADDR_4B, {PROT_1S_2D_2D}},
	{SPINOR_QREAD,		ADDR_3B, {PROT_1S_1S_4S}},
	{SPINOR_QREAD4B,	ADDR_4B, {PROT_1S_1S_4S}},
	{SPINOR_W4READ,		ADDR_3B, {PROT_1S_4S_4S, PROT_4S_4S_4S}},
	{SPINOR_4READ,		ADDR_3B, {PROT_1S_4S_4S, PROT_4S_4S_4S}},
	{SPINOR_4READ4B,	ADDR_4B, {PROT_1S_4S_4S, PROT_4S_4S_4S}},
	{SPINOR_4DTRD,		ADDR_3B, {PROT_1S_4D_4D, PROT_4S_4D_4D}},
	{SPINOR_4DTRD4B,	ADDR_4B, {PROT_1S_4D_4D, PROT_4S_4D_4D}},
	{SPINOR_ORAED,		ADDR_3B, {PROT_1S_1S_8S}},
	{SPINOR_ORAED4B,	ADDR_4B, {PROT_1S_1S_8S}},
	{SPINOR_8READ,		ADDR_4B, {PROT_8S_8S_8S, 0,	PROT_8S_8S_8S, 0}, .dqs = 0},
	{SPINOR_8DTRD,		ADDR_4B, {PROT_8D_8D_8D, 0,	0, PROT_8D_8D_8D}, .dqs = 1},
};
#define MXIC_OPI_RD_CMDS    (MXIC_8DTRD | MXIC_8READ)
#define MXIC_QPI_RD_CMDS	(MXIC_FREAD | MXIC_FREAD4B | MXIC_FDTRD | \
							MXIC_FDTRD4B | MXIC_W4READ | MXIC_4READ | MXIC_4READ4B | \
							MXIC_4DTRD | MXIC_4DTR4B)
#define MXIC_QE_RD_CMDS     (MXIC_QREAD | MXIC_QREAD4B | MXIC_W4READ | MXIC_4READ | \
							MXIC_4READ4B | MXIC_4DTRD | MXIC_4DTRD4B)

#define IDS(_id...)								\
	.id = (uint8_t[]){_id},						\
	.len_id = (uint8_t)sizeof((uint8_t[]){_id})

typedef struct _rdcmds{
	uint32_t idx_cmd;
	uint16_t dc;
} rdcmds_t;

#define RDCMDS(_mode, _rdcmds...)	\
	.cmd_list[_mode].rdcmds = (rdcmds_t[]){_rdcmds},	\
	.cmd_list[_mode].nrdcmds = (uint8_t)(sizeof((rdcmds_t[]){_rdcmds}) / sizeof(rdcmds_t))

/* flash info */
#define INFO(																		\
			_id,																	\
			_size_block, _nblocks,													\
			_flag_spcl,																\
        	_cmd_list_spi0, _cmd_list_spi1, _cmd_list_spi2,	_cmd_list_spi3,			\
			_cmd_list_rdcmds_spi,													\
			_cmd_list_qpi0, _cmd_list_qpi1, _cmd_list_qpi2,	_cmd_list_qpi3,			\
			_cmd_list_rdcmds_qpi,													\
			_cmd_list_opi0, _cmd_list_opi1, _cmd_list_opi2, _cmd_list_opi3,			\
			_cmd_list_rdcmds_dopi,													\
			_cmd_list_rdcmds_sopi,													\
			_t_w, _t_dp, _t_bp, _t_pp, _t_se, _t_be32, _t_be, _t_ce, _t_wreaw		\
		)																			\
			_id,																	\
			.size_chip = (_size_block) * (_nblocks),								\
			.size_block = (_size_block),											\
			.size_page = SIZE_PAGE, 												\
			.nblocks = (_nblocks),													\
			.flag_spcl = (_flag_spcl),												\
			.cmd_list[SPINOR_MODE_SPI].val = 										\
			{																		\
				_cmd_list_spi0, _cmd_list_spi1, _cmd_list_spi2, _cmd_list_spi3,	0	\
			},																		\
			_cmd_list_rdcmds_spi,													\
			.cmd_list[SPINOR_MODE_QPI].val =										\
			{																		\
				_cmd_list_qpi0, _cmd_list_qpi1, _cmd_list_qpi2, _cmd_list_qpi3,	0	\
			},																		\
			_cmd_list_rdcmds_qpi,													\
			.cmd_list[SPINOR_MODE_SOPI].val = 										\
			{																		\
				_cmd_list_opi0, _cmd_list_opi1, _cmd_list_opi2, _cmd_list_opi3,	0	\
			},																		\
			_cmd_list_rdcmds_sopi,													\
			.cmd_list[SPINOR_MODE_DOPI].val = 										\
			{																		\
				_cmd_list_opi0, _cmd_list_opi1, _cmd_list_opi2, _cmd_list_opi3, 0	\
			},																		\
			_cmd_list_rdcmds_dopi,													\
			.timing =																\
			{																		\
				.w =_t_w,															\
				.dp =_t_dp,															\
				.bp =_t_bp,   					   									\
				.pp    =_t_pp,  				    								\
				.se    =_t_se, 														\
				.be32  =_t_be32,													\
				.be    =_t_be,    													\
				.ce    =_t_ce,														\
				.wreaw =_t_wreaw													\
			}

#define ID_INFOS(_name, _id_info...) static snor_id_info_t _name = {_id_info, {}}

#define SNOR_NOT_SUPPORT(_mode) 0,0,0,0,RDCMDS(SPINOR_MODE_##_mode)

ID_INFOS(ids[],
	{"mx25um51245g",
		INFO(
			/* ID & ID length */
			IDS(0xc2, 0x80, 0x3a),
			/* block size, block count */
			64 * 1024, 1024,
			/* support for special commands */
			SPCL_RDPASS_ADDR | SPCL_CR_ODS | SPCL_WORD_MODE | SPCL_PREAMBLE,
            /**
             * The following is the description after the 2nd parameter of the RDCMDS:
			 * bit15-08: Dummy cycle value in configuration register, where 0xff indicates no setting is required.
			 * bit07-00: Number of dummy cycles, where 0xff indicates that this command is not supported.
			 */

			/* cmd_list[SPINOR_MODE_SPI].val[3:0] */
			(MXIC_RDID | MXIC_WRSR | MXIC_WRSCUR | MXIC_RDSCUR | MXIC_RDCR | MXIC_CR2 | MXIC_FBR | MXIC_LR | MXIC_PASS | MXIC_SPB | MXIC_DPB | MXIC_RDSFDP),
			(MXIC_DP | MXIC_RDP | MXIC_SO | MXIC_WPSEL | MXIC_RST | MXIC_NOP | MXIC_GBLK | MXIC_SUS_RES),
			(MXIC_SE4B | MXIC_SE | MXIC_BE4B | MXIC_BE | MXIC_CE),
			(MXIC_PP4B | MXIC_PP),
			RDCMDS(SPINOR_MODE_SPI, {MXIC_FREAD4B, 0xff08}, {MXIC_FREAD, 0xff08}, {MXIC_READ4B, 0xff00}, {MXIC_READ, 0xff00}),
			/* cmd_list[SPINOR_MODE_QPI].val[3:0] */
//			SNOR_NOT_SUPPORT(QPI),
			0, 0, 0, 0, RDCMDS(SPINOR_MODE_QPI),
			/* cmd_list[SPINOR_MODE_SOPI/DOPI].val[3:0] */
			(MXIC_RDID | MXIC_WRSR | MXIC_WRSCUR | MXIC_RDSCUR | MXIC_RDCR | MXIC_CR2 | MXIC_FBR | MXIC_LR | MXIC_PASS | MXIC_SPB | MXIC_DPB | MXIC_WRCR | MXIC_RDSFDP),
			(MXIC_DP | MXIC_RDP | MXIC_SO | MXIC_WPSEL | MXIC_SBL | MXIC_RST | MXIC_RSTQIO | MXIC_NOP | MXIC_GBLK | MXIC_SUS_RES),
			(MXIC_SE4B | MXIC_BE4B | MXIC_CE),
			(MXIC_PP4B),
			RDCMDS(SPINOR_MODE_SOPI, {MXIC_8READ, 0x0014}),
			RDCMDS(SPINOR_MODE_DOPI, {MXIC_8DTRD, 0x0014}),
			/*timing value*/
			40000, 10, 30, 1500, 120000, 0, 650000, 300000000, 1
		)
	},
	{"mx25uw51245g",
		INFO(
			/* ID & ID length */
			IDS(0xc2, 0x81, 0x3a),
			/* block size, block count */
			64 * 1024, 1024,
			/* support for special commands */
			SPCL_RDPASS_ADDR | SPCL_CR_ODS | SPCL_WORD_MODE | SPCL_PREAMBLE,

			/**
			 * The following is the description after the 2nd parameter of the RDCMDS:
			 * bit15-08: Dummy cycle value in configuration register, where 0xff indicates no setting is required.
			 * bit07-00: Number of dummy cycles, where 0xff indicates that this command is not supported.
			 */

			/* cmd_list[SPINOR_MODE_SPI].val[3:0] */
			(MXIC_RDID | MXIC_WRSR | MXIC_WRSCUR | MXIC_RDSCUR | MXIC_RDCR | MXIC_CR2 | MXIC_FBR | MXIC_LR | MXIC_PASS | MXIC_SPB | MXIC_DPB | MXIC_RDSFDP),
			(MXIC_DP | MXIC_RDP | MXIC_SO | MXIC_WPSEL | MXIC_RST | MXIC_NOP | MXIC_GBLK | MXIC_SUS_RES),
			(MXIC_SE4B | MXIC_SE | MXIC_BE4B | MXIC_BE | MXIC_CE),
			(MXIC_PP4B | MXIC_PP),
			RDCMDS(SPINOR_MODE_SPI, {MXIC_FREAD4B, 0xff08}, {MXIC_FREAD, 0xff08}, {MXIC_READ4B, 0xff00}, {MXIC_READ, 0xff00}),
			/* cmd_list[SPINOR_MODE_QPI].val[3:0] */
			0, 0, 0, 0,
			RDCMDS(SPINOR_MODE_QPI),
			/* cmd_list[SPINOR_MODE_SOPI/DOPI].val[3:0] */
			(MXIC_RDID | MXIC_WRSR | MXIC_WRSCUR | MXIC_RDSCUR | MXIC_RDCR | MXIC_CR2 | MXIC_FBR | MXIC_LR | MXIC_PASS | MXIC_SPB | MXIC_DPB | MXIC_WRCR | MXIC_RDSFDP),
			(MXIC_DP | MXIC_RDP | MXIC_SO | MXIC_WPSEL | MXIC_SBL | MXIC_RST | MXIC_RSTQIO | MXIC_NOP | MXIC_GBLK | MXIC_SUS_RES),
			(MXIC_SE4B | MXIC_BE4B | MXIC_CE),
			(MXIC_PP4B),
			RDCMDS(SPINOR_MODE_SOPI, {MXIC_8READ, 0x0014}),
			RDCMDS(SPINOR_MODE_DOPI, {MXIC_8DTRD, 0x0014}),
			/*timing value*/
			40000, 10, 30, 1500, 120000, 0, 650000, 300000000, 1
		)
	},
	{"mx25uw12845g",
		INFO(
			/* ID & ID length */
			IDS(0xc2, 0x81, 0x38),
			/* block size, block count */
			64 * 1024, 256,
			/* support for special commands */
			SPCL_RDPASS_ADDR | SPCL_CR_ODS | SPCL_WORD_MODE | SPCL_PREAMBLE,

			/**
			 * The following is the description after the 2nd parameter of the RDCMDS:
			 * bit15-08: Dummy cycle value in configuration register, where 0xff indicates no setting is required.
			 * bit07-00: Number of dummy cycles, where 0xff indicates that this command is not supported.
			 */

			/* cmd_list[SPINOR_MODE_SPI].val[3:0] */
			(MXIC_RDID | MXIC_WRSR | MXIC_WRSCUR | MXIC_RDSCUR | MXIC_RDCR | MXIC_CR2 | MXIC_FBR | MXIC_LR | MXIC_PASS | MXIC_SPB | MXIC_DPB | MXIC_RDSFDP),
			(MXIC_DP | MXIC_RDP | MXIC_SO | MXIC_WPSEL | MXIC_RST | MXIC_NOP | MXIC_GBLK | MXIC_SUS_RES),
			(MXIC_SE4B | MXIC_SE | MXIC_BE4B | MXIC_BE | MXIC_CE),
			(MXIC_PP4B | MXIC_PP),
			RDCMDS(SPINOR_MODE_SPI, {MXIC_FREAD4B, 0xff08}, {MXIC_FREAD, 0xff08}, {MXIC_READ4B, 0xff00}, {MXIC_READ, 0xff00}),
			/* cmd_list[SPINOR_MODE_QPI].val[3:0] */
			0, 0, 0, 0,
			RDCMDS(SPINOR_MODE_QPI),
			/* cmd_list[SPINOR_MODE_SOPI/DOPI].val[3:0] */
			(MXIC_RDID | MXIC_WRSR | MXIC_WRSCUR | MXIC_RDSCUR | MXIC_RDCR | MXIC_CR2 | MXIC_FBR | MXIC_LR | MXIC_PASS | MXIC_SPB | MXIC_DPB | MXIC_WRCR | MXIC_RDSFDP),
			(MXIC_DP | MXIC_RDP | MXIC_SO | MXIC_WPSEL | MXIC_SBL | MXIC_RST | MXIC_RSTQIO | MXIC_NOP | MXIC_GBLK | MXIC_SUS_RES),
			(MXIC_SE4B | MXIC_BE4B | MXIC_CE),
			(MXIC_PP4B),
			RDCMDS(SPINOR_MODE_SOPI, {MXIC_8READ, 0x0014}),
			RDCMDS(SPINOR_MODE_DOPI, {MXIC_8DTRD, 0x0014}),
			/*timing value*/
			40000, 10, 30, 1500, 120000, 0, 650000, 300000000, 1
		)
	},
	{"mx25uw12345g",
		INFO(
			/* ID & ID length */
			IDS(0xc2, 0x84, 0x38),
			/* block size, block count */
			64 * 1024, 256,
			/* support for special commands */
			SPCL_RDPASS_ADDR | SPCL_CR_ODS | SPCL_PREAMBLE,

			/**
			 * The following is the description after the 2nd parameter of the RDCMDS:
			 * bit15-08: Dummy cycle value in configuration register, where 0xff indicates no setting is required.
			 * bit07-00: Number of dummy cycles, where 0xff indicates that this command is not supported.
			 */

			/* cmd_list[SPINOR_MODE_SPI].val[3:0] */
			(MXIC_RDID | MXIC_WRSR | MXIC_WRSCUR | MXIC_RDSCUR | MXIC_RDCR | MXIC_CR2 | MXIC_FBR | MXIC_LR | MXIC_PASS | MXIC_SPB | MXIC_DPB | MXIC_RDSFDP),
			(MXIC_DP | MXIC_RDP | MXIC_SO | MXIC_WPSEL | MXIC_RST | MXIC_NOP | MXIC_GBLK | MXIC_SUS_RES),
			(MXIC_SE4B | MXIC_SE | MXIC_BE4B | MXIC_BE | MXIC_CE),
			(MXIC_PP4B | MXIC_PP),
			RDCMDS(SPINOR_MODE_SPI, {MXIC_FREAD4B, 0xff08}, {MXIC_FREAD, 0xff08}, {MXIC_READ4B, 0xff00}, {MXIC_READ, 0xff00}),
			/* cmd_list[SPINOR_MODE_QPI].val[3:0] */
			0, 0, 0, 0,
			RDCMDS(SPINOR_MODE_QPI),
			/* cmd_list[SPINOR_MODE_SOPI/DOPI].val[3:0] */
			(MXIC_RDID | MXIC_WRSR | MXIC_WRSCUR | MXIC_RDSCUR | MXIC_RDCR | MXIC_CR2 | MXIC_FBR | MXIC_LR | MXIC_PASS | MXIC_SPB | MXIC_DPB | MXIC_WRCR | MXIC_RDSFDP),
			(MXIC_DP | MXIC_RDP | MXIC_SO | MXIC_WPSEL | MXIC_SBL | MXIC_RST | MXIC_RSTQIO | MXIC_NOP | MXIC_GBLK | MXIC_SUS_RES),
			(MXIC_SE4B | MXIC_BE4B | MXIC_CE),
			(MXIC_PP4B),
			RDCMDS(SPINOR_MODE_SOPI, {MXIC_8READ, 0x0014}),
			RDCMDS(SPINOR_MODE_DOPI, {MXIC_8DTRD, 0x0014}),
			/*timing value*/
			40000, 10, 30, 1500, 120000, 0, 650000, 300000000, 1
		)
	},
	{"mx25uw1g345g",
		INFO(
			/* ID & ID length */
			IDS(0xc2, 0x84, 0x3b),
			/* block size, block count */
			64 * 1024, 256,
			/* support for special commands */
			SPCL_RDPASS_ADDR | SPCL_CR_ODS | SPCL_PREAMBLE,

			/**
			 * The following is the description after the 2nd parameter of the RDCMDS:
			 * bit15-08: Dummy cycle value in configuration register, where 0xff indicates no setting is required.
			 * bit07-00: Number of dummy cycles, where 0xff indicates that this command is not supported.
			 */

			/* cmd_list[SPINOR_MODE_SPI].val[3:0] */
			(MXIC_RDID | MXIC_WRSR | MXIC_WRSCUR | MXIC_RDSCUR | MXIC_RDCR | MXIC_CR2 | MXIC_FBR | MXIC_LR | MXIC_PASS | MXIC_SPB | MXIC_DPB | MXIC_RDSFDP),
			(MXIC_DP | MXIC_RDP | MXIC_SO | MXIC_WPSEL | MXIC_RST | MXIC_NOP | MXIC_GBLK | MXIC_SUS_RES),
			(MXIC_SE4B | MXIC_SE | MXIC_BE4B | MXIC_BE | MXIC_CE),
			(MXIC_PP4B | MXIC_PP),
			RDCMDS(SPINOR_MODE_SPI, {MXIC_FREAD4B, 0xff08}, {MXIC_FREAD, 0xff08}, {MXIC_READ4B, 0xff00}, {MXIC_READ, 0xff00}),
			/* cmd_list[SPINOR_MODE_QPI].val[3:0] */
			0, 0, 0, 0,
			RDCMDS(SPINOR_MODE_QPI),
			/* cmd_list[SPINOR_MODE_SOPI/DOPI].val[3:0] */
			(MXIC_RDID | MXIC_WRSR | MXIC_WRSCUR | MXIC_RDSCUR | MXIC_RDCR | MXIC_CR2 | MXIC_FBR | MXIC_LR | MXIC_PASS | MXIC_SPB | MXIC_DPB | MXIC_WRCR | MXIC_RDSFDP),
			(MXIC_DP | MXIC_RDP | MXIC_SO | MXIC_WPSEL | MXIC_SBL | MXIC_RST | MXIC_RSTQIO | MXIC_NOP | MXIC_GBLK | MXIC_SUS_RES),
			(MXIC_SE4B | MXIC_BE4B | MXIC_CE),
			(MXIC_PP4B),
			RDCMDS(SPINOR_MODE_SOPI, {MXIC_8READ, 0x0014}),
			RDCMDS(SPINOR_MODE_DOPI, {MXIC_8DTRD, 0x0014}),
			/*timing value*/
			40000, 10, 30, 1500, 120000, 0, 650000, 300000000, 1
		)
	},
	{"mx66uw1g45g",
		INFO(
			/* ID & ID length */
			IDS(0xc2, 0x81, 0x3b),
			/* block size, block count */
			64 * 1024, 2048,
			/* support for special commands */
			SPCL_RDPASS_ADDR | SPCL_CR_ODS | SPCL_WORD_MODE | SPCL_PREAMBLE,

			/**
			 * The following is the description after the 2nd parameter of the RDCMDS:
			 * bit15-08: Dummy cycle value in configuration register, where 0xff indicates no setting is required.
			 * bit07-00: Number of dummy cycles, where 0xff indicates that this command is not supported.
			 */

			/* cmd_list[SPINOR_MODE_SPI].val[3:0] */
			(MXIC_RDID | MXIC_WRSR | MXIC_WRSCUR | MXIC_RDSCUR | MXIC_RDCR | MXIC_CR2 | MXIC_FBR | MXIC_LR | MXIC_PASS | MXIC_SPB | MXIC_DPB | MXIC_RDSFDP),
			(MXIC_DP | MXIC_RDP | MXIC_SO | MXIC_WPSEL | MXIC_RST | MXIC_NOP | MXIC_GBLK | MXIC_SUS_RES),
			(MXIC_SE4B | MXIC_SE | MXIC_BE4B | MXIC_BE | MXIC_CE),
			(MXIC_PP4B | MXIC_PP),
			RDCMDS(SPINOR_MODE_SPI, {MXIC_FREAD4B, 0xff08}, {MXIC_FREAD, 0xff08}, {MXIC_READ4B, 0xff00}, {MXIC_READ, 0xff00}),
			/* cmd_list[SPINOR_MODE_QPI].val[3:0] */
			0, 0, 0, 0,
			RDCMDS(SPINOR_MODE_QPI),
			/* cmd_list[SPINOR_MODE_SOPI/DOPI].val[3:0] */
			(MXIC_RDID | MXIC_WRSR | MXIC_WRSCUR | MXIC_RDSCUR | MXIC_RDCR | MXIC_CR2 | MXIC_FBR | MXIC_LR | MXIC_PASS | MXIC_SPB | MXIC_DPB | MXIC_WRCR | MXIC_RDSFDP),
			(MXIC_DP | MXIC_RDP | MXIC_SO | MXIC_WPSEL | MXIC_SBL | MXIC_RST | MXIC_RSTQIO | MXIC_NOP | MXIC_GBLK | MXIC_SUS_RES),
			(MXIC_SE4B | MXIC_BE4B | MXIC_CE),
			(MXIC_PP4B),
			RDCMDS(SPINOR_MODE_SOPI, {MXIC_8READ, 0x0014}),
			RDCMDS(SPINOR_MODE_DOPI, {MXIC_8DTRD, 0x0014}),
			/*timing value*/
			40000, 10, 30, 1500, 120000, 0, 650000, 300000000, 1
		)
	},
	{"mx66um1g45g",
		INFO(
			/* ID & ID length */
			IDS(0xc2, 0x80, 0x3b),
			/* block size, block count */
			64 * 1024, 2048,
			/* support for special commands */
			SPCL_RDPASS_ADDR | SPCL_CR_ODS | SPCL_WORD_MODE | SPCL_PREAMBLE,

			/**
			 * The following is the description after the 2nd parameter of the RDCMDS:
			 * bit15-08: Dummy cycle value in configuration register, where 0xff indicates no setting is required.
			 * bit07-00: Number of dummy cycles, where 0xff indicates that this command is not supported.
			 */

			/* cmd_list[SPINOR_MODE_SPI].val[3:0] */
			(MXIC_RDID | MXIC_WRSR | MXIC_WRSCUR | MXIC_RDSCUR | MXIC_RDCR | MXIC_CR2 | MXIC_FBR | MXIC_LR | MXIC_PASS | MXIC_SPB | MXIC_DPB | MXIC_RDSFDP),
			(MXIC_DP | MXIC_RDP | MXIC_SO | MXIC_WPSEL | MXIC_RST | MXIC_NOP | MXIC_GBLK | MXIC_SUS_RES),
			(MXIC_SE4B | MXIC_SE | MXIC_BE4B | MXIC_BE | MXIC_CE),
			(MXIC_PP4B | MXIC_PP),
			RDCMDS(SPINOR_MODE_SPI, {MXIC_FREAD4B, 0xff08}, {MXIC_FREAD, 0xff08}, {MXIC_READ4B, 0xff00}, {MXIC_READ, 0xff00}),
			/* cmd_list[SPINOR_MODE_QPI].val[3:0] */
			0, 0, 0, 0,
			RDCMDS(SPINOR_MODE_QPI),
			/* cmd_list[SPINOR_MODE_SOPI/DOPI].val[3:0] */
			(MXIC_RDID | MXIC_WRSR | MXIC_WRSCUR | MXIC_RDSCUR | MXIC_RDCR | MXIC_CR2 | MXIC_FBR | MXIC_LR | MXIC_PASS | MXIC_SPB | MXIC_DPB | MXIC_WRCR | MXIC_RDSFDP),
			(MXIC_DP | MXIC_RDP | MXIC_SO | MXIC_WPSEL | MXIC_SBL | MXIC_RST | MXIC_RSTQIO | MXIC_NOP | MXIC_GBLK | MXIC_SUS_RES),
			(MXIC_SE4B | MXIC_BE4B | MXIC_CE),
			(MXIC_PP4B),
			RDCMDS(SPINOR_MODE_SOPI, {MXIC_8READ, 0x0014}),
			RDCMDS(SPINOR_MODE_DOPI, {MXIC_8DTRD, 0x0014}),
			/*timing value*/
			40000, 10, 30, 1500, 120000, 0, 650000, 300000000, 1
		)
	},
	{"mx66um2g45g",
		INFO(
			/* ID & ID length */
			IDS(0xc2, 0x80, 0x3c),
			/* block size, block count */
			64 * 1024, 4096,
			/* support for special commands */
			SPCL_RDPASS_ADDR | SPCL_CR_ODS | SPCL_WORD_MODE | SPCL_PREAMBLE,

			/**
			 * The following is the description after the 2nd parameter of the RDCMDS:
			 * bit15-08: Dummy cycle value in configuration register, where 0xff indicates no setting is required.
			 * bit07-00: Number of dummy cycles, where 0xff indicates that this command is not supported.
			 */

			/* cmd_list[SPINOR_MODE_SPI].val[3:0] */
			(MXIC_RDID | MXIC_WRSR | MXIC_WRSCUR | MXIC_RDSCUR | MXIC_RDCR | MXIC_CR2 | MXIC_FBR | MXIC_LR | MXIC_PASS | MXIC_SPB | MXIC_DPB | MXIC_RDSFDP),
			(MXIC_DP | MXIC_RDP | MXIC_SO | MXIC_WPSEL | MXIC_RST | MXIC_NOP | MXIC_GBLK | MXIC_SUS_RES),
			(MXIC_SE4B | MXIC_SE | MXIC_BE4B | MXIC_BE | MXIC_CE),
			(MXIC_PP4B | MXIC_PP),
			RDCMDS(SPINOR_MODE_SPI, {MXIC_FREAD4B, 0xff08}, {MXIC_FREAD, 0xff08}, {MXIC_READ4B, 0xff00}, {MXIC_READ, 0xff00}),
			/* cmd_list[SPINOR_MODE_QPI].val[3:0] */
			0, 0, 0, 0,
			RDCMDS(SPINOR_MODE_QPI),
			/* cmd_list[SPINOR_MODE_SOPI/DOPI].val[3:0] */
			(MXIC_RDID | MXIC_WRSR | MXIC_WRSCUR | MXIC_RDSCUR | MXIC_RDCR | MXIC_CR2 | MXIC_FBR | MXIC_LR | MXIC_PASS | MXIC_SPB | MXIC_DPB | MXIC_WRCR | MXIC_RDSFDP),
			(MXIC_DP | MXIC_RDP | MXIC_SO | MXIC_WPSEL | MXIC_SBL | MXIC_RST | MXIC_RSTQIO | MXIC_NOP | MXIC_GBLK | MXIC_SUS_RES),
			(MXIC_SE4B | MXIC_BE4B | MXIC_CE),
			(MXIC_PP4B),
			RDCMDS(SPINOR_MODE_SOPI, {MXIC_8READ, 0x0014}),
			RDCMDS(SPINOR_MODE_DOPI, {MXIC_8DTRD, 0x0014}),
			/*timing value*/
			40000, 10, 30, 1500, 120000, 0, 650000, 300000000, 1
		)
	},
	{"mx78u128b",
		INFO(
			/* ID & ID length */
			IDS(0xc2, 0x29, 0x18),
			/* block size, block count */
			64 * 1024, 256,
			/* support for special commands */
			SPCL_SR_QE_BIT6 | SPCL_CR_DC_BIT6_7,

			/**
			 * The following is the description after the 2nd parameter of the RDCMDS:
			 * bit15-08: Dummy cycle value in configuration register, where 0xff indicates no setting is required.
			 * bit07-00: Number of dummy cycles, where 0xff indicates that this command is not supported.
			 */

			/* cmd_list[SPINOR_MODE_SPI].val[3:0] */
			(MXIC_RDID | MXIC_WRSR | MXIC_WRSCUR | MXIC_RDSCUR | MXIC_RDCR | MXIC_CR2),
			(MXIC_RST | MXIC_NOP),
			(MXIC_BE | MXIC_CE | MXIC_SE),
			(MXIC_PP | MXIC_4PP | MXIC_QPP),
			RDCMDS(SPINOR_MODE_SPI, {MXIC_4READ, 0x030a}, {MXIC_QREAD, 0x030a}, {MXIC_FREAD, 0x0106}),
			/* cmd_list[SPINOR_MODE_QPI].val[3:0] */
			0, 0, 0, 0,
			RDCMDS(SPINOR_MODE_QPI),
			/* cmd_list[SPINOR_MODE_SOPI/DOPI].val[3:0] */
			0, 0, 0, 0,
			RDCMDS(SPINOR_MODE_SOPI),
			RDCMDS(SPINOR_MODE_DOPI),
			/*timing value*/
			40000, 10, 30, 1500, 120000, 0, 650000, 300000000, 1
		)
	},
	{"mx76u25645g",
		INFO(
			/* ID & ID length */
			IDS(0xc2, 0x75, 0x39),
			/* block size, block count */
			64 * 1024, 512,
			/* support for special commands */
			SPCL_SR_QE_BIT6 | SPCL_CR_DC_BIT6_7 | SPCL_SCUR_WPSEL | SPCL_ASP_4KB_IN_BLK,

			/**
			 * The following is the description after the 2nd parameter of the RDCMDS:
			 * bit15-08: Dummy cycle value in configuration register, where 0xff indicates no setting is required.
			 * bit07-00: Number of dummy cycles, where 0xff indicates that this command is not supported.
			 */

			/* cmd_list[SPINOR_MODE_SPI].val[3:0] */
			(MXIC_RDID | MXIC_WRSR | MXIC_WRSCUR | MXIC_RDSCUR | MXIC_RDCR),
			(MXIC_RST | MXIC_NOP),
			(MXIC_BE | MXIC_CE | MXIC_SE),
//			(MXIC_PP | MXIC_QPP),
			(MXIC_PP),
//			RDCMDS(SPINOR_MODE_SPI, {MXIC_QREAD, 0x030a}, {MXIC_FREAD, 0x0008}),
			RDCMDS(SPINOR_MODE_SPI, {MXIC_FREAD, 0x0008}),
			/* cmd_list[SPINOR_MODE_QPI].val[3:0] */
			0, 0, 0, 0,
			RDCMDS(SPINOR_MODE_QPI),
			/* cmd_list[SPINOR_MODE_SOPI/DOPI].val[3:0] */
			0, 0, 0, 0,
			RDCMDS(SPINOR_MODE_SOPI),
			RDCMDS(SPINOR_MODE_DOPI),
			/*timing value*/
			40000, 10, 30, 1500, 120000, 0, 650000, 300000000, 1
		)
	},
	{"mx25u51279g",
        INFO(
            /* ID & ID length */
            IDS(0xc2, 0x95, 0x3a),
            /* block size, block count */
            64 * 1024, 1024,
            /* support for special commands */
            SPCL_SR_QE_BIT6 | SPCL_CR_DC_BIT6_7 | SPCL_SCUR_WPSEL | SPCL_ASP_4KB_IN_BLK,

            /**
             * The following is the description after the 2nd parameter of the RDCMDS:
             * bit15-08: Dummy cycle value in configuration register, where 0xff indicates no setting is required.
             * bit07-00: Number of dummy cycles, where 0xff indicates that this command is not supported.
             */

            /* cmd_list[SPINOR_MODE_SPI].val[3:0] */
            (MXIC_RDID | MXIC_WRSR | MXIC_WRSCUR | MXIC_RDSCUR | MXIC_RDCR),
            (MXIC_RST | MXIC_NOP),
            (MXIC_CE | MXIC_SE | MXIC_BE | MXIC_BE32K | MXIC_SE4B | MXIC_BE32K4B | MXIC_BE4B),
            (MXIC_PP | MXIC_4PP | MXIC_PP4B | MXIC_4PP4B),
            RDCMDS(SPINOR_MODE_SPI, {MXIC_FREAD, 0x000a}, {MXIC_QREAD, 0x000a}, {MXIC_4READ, 0x000a},
                    {MXIC_QREAD4B, 0x000a}, {MXIC_4READ4B, 0x000a}),
            /* cmd_list[SPINOR_MODE_QPI].val[3:0] */
            0, 0, 0, 0,
            RDCMDS(SPINOR_MODE_QPI),
            /* cmd_list[SPINOR_MODE_SOPI/DOPI].val[3:0] */
            0, 0, 0, 0,
            RDCMDS(SPINOR_MODE_SOPI),
            RDCMDS(SPINOR_MODE_DOPI),
            /*timing value*/
            40000, 10, 30, 4000, 480000, 1100000, 2200000, 400000000, 1
        )
    }
);

#endif
