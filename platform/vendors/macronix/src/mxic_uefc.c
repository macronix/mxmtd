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

/**
 * TODO: Parallel mode
 */

#include <sys/time.h>
#include "platform_print.h"
#include "platform_conf.h"
#include "platform_itf.h"
#include "lib.h"
#include "mxic_uefc_err.h"
#include "sleep.h"
#include "xtime_l.h"
#include "xil_cache.h"

#define PARS_CH(x) ((1 == (x)) ? "xSPI" : ((2 == (x)) ? "RAW NAND" : ((3 == (x)) ? "eMMC" : "NA")))
#define BIT(x) (1U << (x))

#define ALIGN_BYTES_MAP 4
#define ALIGN_BYTES_DMA 4
#define BYTES_DMA_LIMIT 12

/* Host Controller Register */
#define HC_CTRL						0x00
#define HC_CTRL_RQE_EN				BIT(31)
#define HC_CTRL_SDMA_BD(x)			(((x) & 0x7) << 28)
#define HC_CTRL_PARALLEL_1			BIT(27)
#define HC_CTRL_PARALLEL_0			BIT(26)
#define HC_CTRL_DATA_ORDER			BIT(25) //OctaFlash, OctaRAM
#define HC_CTRL_SIO_SHIFTER(x)		(((x) & 0x3) << 23)
#define HC_CTRL_EX_SER_B			BIT(22)
#define HC_CTRL_EX_SER_A			BIT(21)
#define HC_CTRL_ASSIMI_BYTE_B(x)	(((x) & 0x3) << 19)
#define HC_CTRL_ASSIMI_BYTE_A(x)	(((x) & 0x3) << 17)
#define HC_CTRL_EX_PHY_ITE_B		BIT(16)
#define HC_CTRL_EX_PHY_ITE_A		BIT(15)
#define HC_CTRL_EX_PHY_DQS_B		BIT(14)
#define HC_CTRL_EX_PHY_DQS_A		BIT(13)
#define HC_CTRL_LED					BIT(12)
#define HC_CTRL_CH_SEL_B			BIT(11)
#define HC_CTRL_CH_SEL_A			0
#define HC_CTRL_CH_MASK				BIT(11)
#define HC_CTRL_LUN_SEL(x)			(((x) & 0x7) << 8) //NAND
#define HC_CTRL_LUN_MASK			HC_CTRL_LUN_SEL(0x7)
#define HC_CTRL_PORT_SEL(x)			(((x) & 0xff) << 0)
#define HC_CTRL_PORT_MASK			(HC_CTRL_PORT_SEL(0xff))

#define HC_CTRL_CH_LUN_PORT_MASK	(HC_CTRL_CH_MASK | HC_CTRL_LUN_MASK | HC_CTRL_PORT_MASK)
#define HC_CTRL_CH_LUN_PORT(ch, lun, port) (HC_CTRL_CH_SEL_##ch | HC_CTRL_LUN_SEL(lun) | HC_CTRL_PORT_SEL(port))

/* Normal Interrupt Status Register */
#define INT_STS				0x04
#define INT_STS_CA_REQ			BIT(30)
#define INT_STS_CACHE_RDY		BIT(29)
#define INT_STS_AC_RDY			BIT(28)
#define INT_STS_ERR_INT			BIT(15)
#define INT_STS_CQE_INT			BIT(14)
#define INT_STS_DMA_TFR_CMPLT		BIT(7)
#define INT_STS_DMA_INT			BIT(6)
#define INT_STS_BUF_RD_RDY		BIT(5)
#define INT_STS_BUF_WR_RDY		BIT(4)
#define INT_STS_ALL_CLR 		(INT_STS_AC_RDY | \
					INT_STS_ERR_INT | \
					INT_STS_DMA_TFR_CMPLT | \
					INT_STS_DMA_INT)

/* Error Interrupt Status Register */
#define ERR_INT_STS			0x08
#define ERR_INT_STS_ECC			BIT(19)
#define ERR_INT_STS_PREAM		BIT(18)
#define ERR_INT_STS_CRC			BIT(17)
#define ERR_INT_STS_AC			BIT(16)
#define ERR_INT_STS_ADMA		BIT(9)
#define ERR_INT_STS_AUTO_CMD		BIT(8)
#define ERR_INT_STS_DATA_END		BIT(6)
#define ERR_INT_STS_DATA_CRC		BIT(5)
#define ERR_INT_STS_DATA_TIMEOUT	BIT(4)
#define ERR_INT_STS_CMD_IDX		BIT(3)
#define ERR_INT_STS_CMD_END		BIT(2)
#define ERR_INT_STS_CMD_CRC		BIT(1)
#define ERR_INT_STS_CMD_TIMEOUT		BIT(0)
#define ERR_INT_STS_ALL_CLR		(ERR_INT_STS_ECC | \
					ERR_INT_STS_PREAM | \
					ERR_INT_STS_CRC | \
					ERR_INT_STS_AC | \
					ERR_INT_STS_ADMA)

/* Normal Interrupt Status Enable Register */
#define INT_STS_EN			0x0C
#define INT_STS_EN_CA_REQ		BIT(30)
#define INT_STS_EN_CACHE_RDY		BIT(29)
#define INT_STS_EN_AC_RDY		BIT(28)
#define INT_STS_EN_ERR_INT		BIT(15)
#define INT_STS_EN_DMA_TFR_CMPLT	BIT(7)
#define INT_STS_DMA					BIT(6)
#define INT_STS_EN_BUF_RD_RDY		BIT(5)
#define INT_STS_EN_BUF_WR_RDY		BIT(4)
#define INT_STS_EN_DMA_INT		BIT(3)
#define INT_STS_EN_BLK_GAP		BIT(2)
#define INT_STS_EN_DAT_CMPLT		BIT(1)
#define INT_STS_EN_CMD_CMPLT		BIT(0)
#define INT_STS_EN_ALL_EN		(INT_STS_EN_AC_RDY | \
					INT_STS_EN_ERR_INT | \
					INT_STS_EN_DMA_TFR_CMPLT | \
					INT_STS_EN_DMA_INT)

/* Error Interrupt Status Enable Register */
#define ERR_INT_STS_EN			0x10
#define ERR_INT_STS_EN_ECC		BIT(19)
#define ERR_INT_STS_EN_PREAM		BIT(18)
#define ERR_INT_STS_EN_CRC		BIT(17)
#define ERR_INT_STS_EN_AC		BIT(16)
#define ERR_INT_STS_EN_ADMA		BIT(9)
#define ERR_INT_STS_EN_AUTO_CMD		BIT(8)
#define ERR_INT_STS_EN_DATA_END		BIT(6)
#define ERR_INT_STS_EN_DATA_CRC		BIT(5)
#define ERR_INT_STS_EN_DATA_TIMEOUT	BIT(4)
#define ERR_INT_STS_EN_CMD_IDX		BIT(3)
#define ERR_INT_STS_EN_CMD_END		BIT(2)
#define ERR_INT_STS_EN_CMD_CRC		BIT(1)
#define ERR_INT_STS_EN_CMD_TIMEOUT	BIT(0)
#define ERR_INT_STS_EN_ALL_EN		(ERR_INT_STS_EN_ECC | \
					ERR_INT_STS_EN_PREAM | \
					ERR_INT_STS_EN_CRC | \
					ERR_INT_STS_EN_AC | \
					ERR_INT_STS_EN_ADMA)

/* Normal Interrupt Signal Enable Register */
#define INT_STS_SIG_EN			0x14
#define INT_STS_SIG_EN_CA_REQ		BIT(30)
#define INT_STS_SIG_EN_CACHE_RDY	BIT(29)
#define INT_STS_SIG_EN_AC_RDY		BIT(28)
#define INT_STS_SIG_EN_ERR_INT		BIT(15)
#define INT_STS_SIG_EN_DMA_TFR_CMPLT	BIT(7)
#define INT_STS_SIG_EN_BUF_RD_RDY	BIT(5)
#define INT_STS_SIG_EN_BUF_WR_RDY	BIT(4)
#define INT_STS_SIG_EN_DMA_INT		BIT(3)
#define INT_STS_SIG_EN_BLK_GAP		BIT(2)
#define INT_STS_SIG_EN_DAT_CMPLT	BIT(1)
#define INT_STS_SIG_EN_CMD_CMPLT	BIT(0)
#define INT_STS_SIG_EN_ALL_EN		(INT_STS_SIG_EN_AC_RDY | \
					INT_STS_SIG_EN_ERR_INT | \
					INT_STS_SIG_EN_DMA_TFR_CMPLT | \
					INT_STS_SIG_EN_DMA_INT)

/* Error Interrupt Signal Enable Register */
#define ERR_INT_STS_SIG_EN		0x18
#define ERR_INT_STS_SIG_EN_ECC		BIT(19)
#define ERR_INT_STS_SIG_EN_PREAM	BIT(18)
#define ERR_INT_STS_SIG_EN_CRC		BIT(17)
#define ERR_INT_STS_SIG_EN_AC		BIT(16)
#define ERR_INT_STS_SIG_EN_ADMA		BIT(9)
#define ERR_INT_STS_SIG_EN_AUTO_CMD	BIT(8)
#define ERR_INT_STS_SIG_EN_DATA_END	BIT(6)
#define ERR_INT_STS_SIG_EN_DATA_CRC	BIT(5)
#define ERR_INT_STS_SIG_EN_DATA_TIMEOUT BIT(4)
#define ERR_INT_STS_SIG_EN_CMD_IDX	BIT(3)
#define ERR_INT_STS_SIG_EN_CMD_END	BIT(2)
#define ERR_INT_STS_SIG_EN_CMD_CRC	BIT(1)
#define ERR_INT_STS_SIG_EN_CMD_TIMEOUT	BIT(0)
#define ERR_INT_STS_SIG_EN_ALL_EN	(ERR_INT_STS_SIG_EN_ECC | \
					ERR_INT_STS_SIG_EN_PREAM | \
					ERR_INT_STS_SIG_EN_CRC | \
					ERR_INT_STS_SIG_EN_AC | \
					ERR_INT_STS_SIG_EN_ADMA)

/* Transfer Mode register */
#define TFR_MODE			0x1C
	#define TFR_MODE_BUSW_1			0
	#define TFR_MODE_BUSW_2			1
	#define TFR_MODE_BUSW_4			2
	#define TFR_MODE_BUSW_8			3
#define TFR_MODE_DMA_TYPE			BIT(31)
#define TFR_MODE_DMA_KEEP_CSB		BIT(30)
#define TFR_MODE_TO_ENHC			BIT(29)
#define TFR_MODE_PREAM_WITH			BIT(28)
#define TFR_MODE_CSB_DONT_CARE		BIT(27)
#define TFR_MODE_SIO_1X_RD_BUS(x)	(((x) & 0x3) << 6)
#define TFR_MODE_MULT_BLK			BIT(5)
#define TFR_MODE_AUTO_CMD(x)		(((x) & 0x3) << 2)
#define TFR_MODE_CNT_EN				BIT(1)
#define TFR_MODE_DMA_EN				BIT(0)
/* share with MAPRD, MAPWR */
	#define OP_DMY_CNT(_len, _dtr, _bw) (((_len * (_dtr + 1)) / (8 / (_bw))) << 21)

	#define OP_ADDR_CNT(x)		(((x) & 0x7) << 18)
	#define OP_CMD_CNT(x)		(((x) - 1) << 17)
	#define OP_DATA_BUSW(x)		(((x) & 0x3) << 14)
	#define OP_DATA_DTR(x)		(((x) & 0x1) << 16)
	#define OP_ADDR_BUSW(x)		(((x) & 0x3) << 11)
	#define OP_ADDR_DTR(x)		(((x) & 0x1) << 13)
	#define OP_CMD_BUSW(x)		(((x) & 0x3) << 8)
	#define OP_CMD_DTR(x)		(((x) & 1) << 10)
	#define OP_DD_RD		BIT(4)

/* Transfer Control Register */
#define TFR_CTRL			0x20
#define TFR_CTRL_DEV_DIS		BIT(18)
#define TFR_CTRL_IO_END			BIT(16)
#define TFR_CTRL_DEV_ACT		BIT(2)
#define TFR_CTRL_HC_ACT			BIT(1)
#define TFR_CTRL_IO_START		BIT(0)

/* Present State Register */
#define PRES_STS			0x24
#define PRES_STS_ADMA(x)		(((x) & 0x7) << 29)
#define PRES_STS_XSPI_TX(x)		(((x) & 0xF) << 25)
#define PRES_STS_ONFI_TX(x)		(((x) & 0x1F) << 20)
#define PRES_STS_RX_NFULL		BIT(19)
#define PRES_STS_RX_NEMPT		BIT(18)
#define PRES_STS_TX_NFULL		BIT(17)
#define PRES_STS_TX_EMPT		BIT(16)
#define PRES_STS_EMMC_TX(x)		(((x) & 0xF) << 12)
#define PRES_STS_BUF_RD_EN		BIT(11)
#define PRES_STS_BUF_WR_EN		BIT(10)
#define PRES_STS_RD_TFR			BIT(9)
#define PRES_STS_WR_TFR			BIT(8)
#define PRES_STS_DAT_ACT		BIT(2)
#define PRES_STS_CMD_INH_DAT	BIT(1)
#define PRES_STS_CMD_INH_CMD	BIT(0)

/* SDMA Transfer Count Register */
#define SDMA_CNT			0x28
#define SDMA_CNT_TFR_BYTE(x)	(((x) & 0xFFFFFFFF) << 0)

/* SDMA System Address Register */
#define SDMA_ADDR			0x2C
#define SDMA_VAL(x)				(((x) & 0xFFFFFFFF) << 0)

/* ADMA2_System Address Register */
#define ADMA2_ADDR			0x30
#define ADMA2_ADDR_VALUE		(((x) & 0xFFFFFFFF) << 0)

/* ADMA3 System Address Register */
#define ADMA3_ADDR			0x34
#define ADMA3_ADDR_VALUE(x)		(((x) & 0xFFFFFFFF) << 0)

/* Mapping Base Address Register */
#define BASE_MAP_ADDR			0x38
#define BASE_MAP_ADDR_VALUE(x)		(((x) & 0xFFFFFFFF) << 0)

/* Software Reset Register */
#define SW_RST				0x44
#define SW_RST_DAT			BIT(2)
#define SW_RST_CMD			BIT(1)
#define SW_RST_ALL			BIT(0)

/* Timeout Control register */
#define TO_CTRL				0x48
#define TO_CTRL_CA(x)			(((x) & 0xF) << 16)
#define TO_CTRL_DAT(x)			(((x) & 0xF) << 16)

/* Clock Control Register */
#define CLK_CTRL			0x4C
#define CLK_CTRL_SLOW_CLOCK		BIT(31)
#define CLK_CTRL_RX_SS_B(x)		(((x) & 0x1F) << 21)
#define CLK_CTRL_RX_SS_A(x)		(((x) & 0x1F) << 16)
#define CLK_CTRL_PLL_SELECT(x)		(((x) & 0xFFFF) << 0)

/* Cache Control Register */
#define CACHE_CTRL			0x54
#define CACHE_CTRL_DIRTY_LEVEL(x)	(((x) & 0x3) << 30)
#define CACHE_CTRL_LEN_TH(x)		(((x) & 0xff) << 22)
#define CACHE_CTRL_CONT_ADDR		BIT(21)
#define CACHE_CTRL_FETCH_CNT(x)		(((x) & 0x7) << 18)
#define CACHE_CTRL_MST(x)		(((x) & 0xFFFF) << 2)
#define CACHE_CTRL_CLEAN		BIT(1)
#define CACHE_CTRL_INVALID		BIT(0)

/* Capabilities Register */
#define CAP_1				0x58
#define CAP_1_DUAL_CH			BIT(31)
#define CAP_1_XSPI_ITF			BIT(30)
#define CAP_1_ONFI_ITF			BIT(29)
#define CAP_1_EMMC_ITF			BIT(28)
#define CAP_1_MAPPING_MODE		BIT(27)
#define CAP_1_CACHE				BIT(26)
#define CAP_1_ATOMIC			BIT(25)
#define CAP_1_DMA_SLAVE_MODE	BIT(24)
#define CAP_1_DMA_MASTER_MODE	BIT(23)
#define CAP_1_CQE				BIT(22)
#define CAP_1_FIFO_DEPTH(x)		(((x) & 0x3) << 15)
#define CAP_1_SYS_DW(x)			(((x) & 0x3) << 13)
#define CAP_1_LUN_NUM(x)		(((x) & 0xF) << 9)
#define CAP_1_CSB_NUM(x)		(((x) & 0x1FF) << 0)
#define CAP_1_CSB_NUM_MASK		0x1FF
#define CAP_1_CSB_NUM_OFS		0

/* Host Controller Version Register */
#define HC_VER				0x5C
#define HC_VER_VALUE(x)			(((x) & 0xFFFFFFFF) << 0)

/*  RTL Version Register */
#define RTL_VER				0x60
#define RTL_VER_VALUE(x)		(((x) & 0xFFFFFFFF) << 0)

/* Transmit Data 0~3 Register */
#define TXD_REG				0x70
#define TXD(x)				(TXD_REG + ((x) * 4))

/* Receive Data Register */
#define RXD_REG				0x80
#define RXD_VALUE(x)			(((x) & 0xFFFFFFFF) << 0)

/* Send CRC Cycle Register */
#define SEND_CRC_CYC			0x84
#define SEND_CRC_CYC_EN			BIT(0)

/* Block Count Register */
#define BLK_CNT				0x90
#define BLK_CNT_VALUE(x)		(((x) & 0xFFFFFFFF) << 0)

/* Argument Register */
#define ARG_REG				0x94
#define ARG_REG_CMD(x)			(((x) & 0xFFFFFFFF) << 0)

/* Command Register */
#define CMD_REG				0x98
#define CMD_REG_BOOT_BUS(x)		(((x) & 0x7) << 19)
#define CMD_REG_BOOT_TYPE		BIT(18)
#define CMD_REG_BOOT_ACK_EN		BIT(17)
#define CMD_REG_BOOT_EN			BIT(16)
#define CMD_REG_CMD_IDX(x)		(((x) & 3F) << 8)
#define CMD_REG_WR_CRC_STS_EN		BIT(6)
#define CMD_REG_DAT_EN			BIT(5)
#define CMD_REG_CMD_IDX_CHK_EN		BIT(4)
#define CMD_REG_CMD_CRC_CHK_EN		BIT(3)
#define CMD_REG_RSP_SEL(x)		(((x) & 0x3) << 0)

/* Response 1 Register */
#define RSP_1				0x9C
#define RSP_1_VALUE(x)			(((x) & 0xFFFFFFFF) << 0)

/* Response 2 Register */
#define RSP_2				0xA0
#define RSP_2_VALUE(x)			(((x) & 0xFFFFFFFF) << 0)

/* Response 3 Register */
#define RSP_3				0xA4
#define RSP_3_VALUE(x)			(((x) & 0xFFFFFFFF) << 0)

/* Response 4 Register */
#define RSP_4				0xA8
#define RSP_4_1_VALUE(x)		(((x) & 0xff) << 0)
#define RSP_4_0_VALUE(x)		(((x) & 0xFFFFFFFF) << 0)

/* Buffer Data Port register */
#define DATA_REG			0xAC
#define DATA_REG_BUF(x)			(((x) & 0xFFFFFFFF) << 0)

/* Auto CMD Argument Register */
#define AUTO_CMD				0xB0
#define AUTO_CMD_ARGU(x)		(((x) & 0xFFFFFFFF) << 0)

/* Auto CMD Error Status Register */
#define AUTO_CMD_ERR_STS		0xB4
#define AUTO_CMD_ERR_STS_IDX		BIT(4)
#define AUTO_CMD_ERR_STS_END		BIT(3)
#define AUTO_CMD_ERR_STS_CRC		BIT(2)
#define AUTO_CMD_ERR_STS_TIMEOUT	BIT(1)

/* Boot System Address Register */
#define BOOT_SYS_ADDR			0xB8
#define BOOT_SYS_ADDR_VALUE(x)		(((x) & 0xFFFFFFFF) << 0)

/* Block Gap Control Register */
#define BLK_GAP_CTRL			0xBC
#define BLK_GAP_CTRL_CONT_REQ		BIT(1)
#define BLK_GAP_CTRL_STOP_GAP		BIT(0)

/* Device Present Status Register */
#define DEV_CTRL				0xC0
#define DEV_CTRL_TYPE(x)			(((x) & 0x7) << 29)
	#define DEV_CTRL_TYPE_MASK			DEV_CTRL_TYPE(0x7)
	#define DEV_CTRL_TYPE_SPI			DEV_CTRL_TYPE(0)
	#define DEV_CTRL_TYPE_LYBRA			DEV_CTRL_TYPE(1)
	#define DEV_CTRL_TYPE_OCTARAM		DEV_CTRL_TYPE(2)
	#define DEV_CTRL_TYPE_RAWNAND_ONFI	DEV_CTRL_TYPE(4)
	#define DEV_CTRL_TYPE_RAWNAND_JEDEC	DEV_CTRL_TYPE(5)
	#define DEV_CTRL_TYPE_EMMC			DEV_CTRL_TYPE(6)
#define DEV_CTRL_SCLK_SEL(x)		(((x) & 0xF) << 25)
#define DEV_CTRL_SCLK_SEL_MASK		DEV_CTRL_SCLK_SEL(0xF)
#define DEV_CTRL_SCLK_SEL_DIV(x)	(((x >> 1) - 1) << 25)
#define DEV_CTRL_CACHEABLE			BIT(24)
#define DEV_CTRL_WR_PLCY(x)			(((x) & 0x3) << 22)
#define DEV_CTRL_PAGE_SIZE(x)		(((x) & 0x7) << 19)
#define DEV_CTRL_BLK_SIZE(x)		(((x) & 0xFFF) << 7)
#define DEV_CTRL_PRE_DQS_EN			BIT(6)
#define DEV_CTRL_DQS_EN				BIT(5)
#define DEV_CTRL_CRC_EN				BIT(4)
#define DEV_CTRL_CRCB_IN_EN			BIT(3)
#define DEV_CTRL_CRC_CHUNK_SIZE(x)	(((x) & 0x3) << 1)
#define DEV_CTRL_CRCB_OUT_EN		BIT(0)

/* Mapping Read Control Register */
#define MAP_RD_CTRL			0xC4
#define MAP_RD_CTRL_PREAM_EN		BIT(28)
#define MAP_RD_CTRL_SIO_1X_RD(x)	(((x) & 0x3) << 6)

/* Linear/Mapping Write Control Register */
#define MAP_WR_CTRL					0xC8

/* Mapping Command Register */
#define MAP_CMD_REG					0xCC

/* Top Mapping Address Register */
#define TOP_MAP_ADDR_REG			0xD0
#define TOP_MAP_ADDR_VALUE(x)		(((x) & 0xFFFFFFFF) << 0)

/* General Purpose Inputs and Outputs Register */
#define GPIO_REG			0xD4
#define GPIO_REG_DATA_LEVEL(x)		(((x) & 0xff) << 24)
#define GPIO_REG_RYBYB_LEVE			BIT(23)
#define GPIO_REG_CMD_LEVEL			BIT(22)
#define GPIO_REG_SIO3_EN			BIT(13)
#define GPIO_REG_SIO2_EN			BIT(12)
#define GPIO_REG_SIO3_DRIV_HIGH		BIT(5)
#define GPIO_REG_SIO2_DRIV_HIGH		BIT(4)
#define GPIO_REG_HP_DRIV_HIGH		BIT(3)
#define GPIO_REG_RESTB_DRIV_HIGH	BIT(2)
#define GPIO_REG_HOLDB_DRIV_HIGH	BIT(1)
#define GPIO_REG_WPB_DRIV_HIGH		BIT(0)

/* Auto Calibration Control Register */
#define AC_CTRL				0xD8
#define AC_CTRL_CMD_2(x)			(((x) & 0xff) << 24)
#define AC_CTRL_CMD_1(x)			(((x) & 0xff) << 16)
#define AC_CTRL_WINDOW(x)			(((x) & 0x3) << 14)
#define AC_CTRL_LAZY_DQS_EN			BIT(9)
#define AC_CTRL_LEN_32B_SEL			BIT(8)
#define AC_CTRL_PHY_EN				BIT(6)
#define AC_CTRL_DQS_TEST_EN			BIT(5)
#define AC_CTRL_SIO_ALIG_EN			BIT(4)
#define AC_CTRL_NVDDR_EN			BIT(3)
#define AC_CTRL_SAMPLE_DQS_EN		BIT(2)
#define AC_CTRL_SAMPLE_EN			BIT(1)
#define AC_CTRL_START				BIT(0)

/* Preamble Bit 1 Register */
#define PREAM_1_REG			0xDC
#define PREAM_1_REG_SIO_1(x)		(((x) & 0xFFFF) << 16)
#define PREAM_1_REG_SIO_0(x)		(((x) & 0xFFFF) << 0)

/* Preamble Bit 2 Register */
#define PREAM_2_REG			0xE0
#define PREAM_2_REG_SIO_3(x)		(((x) & 0xFFFF) << 16)
#define PREAM_2_REG_SIO_2(x)		(((x) & 0xFFFF) << 0)

/* Preamble Bit 3 Register */
#define PREAM_3_REG 0xE4
#define PREAM_3_REG_SIO_5(x)		(((x) & 0xFFFF) << 16)
#define PREAM_3_REG_SIO_4(x)		(((x) & 0xFFFF) << 0)

/* Preamble Bit 4 Register */
#define PREAM_4_REG 0xE8
#define PREAM_4_REG_SIO_7(x)		(((x) & 0xFFFF) << 16)
#define PREAM_4_REG_SIO_6(x)		(((x) & 0xFFFF) << 0)

/* Sample Point Adjust Register */
#define SAMPLE_ADJ 			0xEC
#define SAMPLE_ADJ_DQS_IDLY_DOPI(x)	((x) << 27)
#define SAMPLE_ADJ_DQS_IDLY_SOPI(x)	(((x) & 0x1f) << 19)
#define SAMPLE_ADJ_DQS_ODLY(x)		(((x) & 0xff) << 8)
#define SAMPLE_ADJ_POINT_SEL_DDR(x)	(((x) & 0x7) << 3)
#define SAMPLE_ADJ_POINT_SEL_SDR(x)	(((x) & 0x7) << 0)

/* SIO Input Delay 1 Register */
#define SIO_IDLY_1 0xF0
#define SIO_IDLY_1_SIO3(x)		(((x) & 0xff) << 24)
#define SIO_IDLY_1_SIO2(x)		(((x) & 0xff) << 16)
#define SIO_IDLY_1_SIO1(x)		(((x) & 0xff) << 8)
#define SIO_IDLY_1_SIO0(x)		(((x) & 0xff) << 0)
#define SIO_IDLY_1_0123(x)		SIO_IDLY_1_SIO0(x) | \
								SIO_IDLY_1_SIO1(x) | \
								SIO_IDLY_1_SIO2(x) | \
								SIO_IDLY_1_SIO3(x)

/* SIO Input Delay 2 Register */
#define SIO_IDLY_2 0xF4
#define SIO_IDLY_2_SIO4(x)		(((x) & 0xff) << 24)
#define SIO_IDLY_2_SIO5(x)		(((x) & 0xff) << 16)
#define SIO_IDLY_2_SIO6(x)		(((x) & 0xff) << 8)
#define SIO_IDLY_2_SIO7(x)		(((x) & 0xff) << 0)
#define SIO_IDLY_2_4567(x)		SIO_IDLY_2_SIO4(x) | \
								SIO_IDLY_2_SIO5(x) | \
								SIO_IDLY_2_SIO6(x) | \
								SIO_IDLY_2_SIO7(x)

#define IDLY_CODE_VAL(x, v)		((v) << (((x) % 4) * 8))

/* SIO Output Delay 1 Register */
#define SIO_ODLY_1			0xF8
#define SIO_ODLY_1_SIO3(x)		(((x) & 0xff) << 24)
#define SIO_ODLY_1_SIO2(x)		(((x) & 0xff) << 16)
#define SIO_ODLY_1_SIO1(x)		(((x) & 0xff) << 8)
#define SIO_ODLY_1_SIO0(x)		(((x) & 0xff) << 0)

/* SIO Output Delay 2 Register */
#define SIO_ODLY_2			0xFC
#define SIO_ODLY_2_SIO4(x)		(((x) & 0xff) << 24)
#define SIO_ODLY_2_SIO5(x)		(((x) & 0xff) << 16)
#define SIO_ODLY_2_SIO6(x)		(((x) & 0xff) << 8)
#define SIO_ODLY_2_SIO7(x)		(((x) & 0xff) << 0)

typedef struct {
	uint32_t freq:4,			// BIT[03:00]
			prot_ch_a:2,		// BIT[05:04]
			phy_supl_ch_a:1,	// BIT[06:06]
			volt_ch_a:2,		// BIT[08:07]
			prot_ch_b:2,		// BIT[10:9]
			phy_supl_ch_b:1,	// BIT[11:11]
			volt_ch_b:2;		// BIT[13:12]

} hc_ver_t;

typedef struct {
	uint32_t date:17,			// BIT[16:00]
			ver_min:4,			// BIT[20:17]
			ver_max:4;			// BIT[23:21]
} rtl_ver_t;

#define MXIC_RD(_type, _reg) \
	(*(volatile _type *)(xfer->base_addr + (_reg)))
#define MXIC_RD32(_reg) \
	(*(volatile uint32_t *)(xfer->base_addr + (_reg)))
#define MXIC_WR32(_val, _reg) \
	((*(uint32_t *)(xfer->base_addr + (_reg))) = (_val))
#define UPDATE_WRITE(_mask, _value, _reg) \
	MXIC_WR32(((_value) | (MXIC_RD32(_reg) & ~(_mask))), (_reg))

static inline uint32_t mxic_uefc_get_hc_ver(xfer_info_t *xfer)
{
	return MXIC_RD32(HC_VER);
}

static inline uint32_t mxic_uefc_get_rtl_ver(xfer_info_t *xfer)
{
	return MXIC_RD32(RTL_VER);
}

static int mxic_uefc_hc_setup(xfer_info_t *xfer, uint32_t ch_lun_port)
{
	int dev_ctrl_type;

	switch (xfer->hc_prot_type) {
	case HC_PROT_xSPI:
		dev_ctrl_type = DEV_CTRL_TYPE_SPI;
		break;
	case HC_PROT_RAWNAND:
		dev_ctrl_type = DEV_CTRL_TYPE_RAWNAND_ONFI;
		break;
	default:
		mxic_pr_err("HC_PROT_TYPE (%d) is not supported\r\n", xfer->hc_prot_type);
		return MXST_ERR_NOT_SUP;
	}

	UPDATE_WRITE(HC_CTRL_CH_LUN_PORT_MASK, ch_lun_port, HC_CTRL);
	UPDATE_WRITE(DEV_CTRL_TYPE_MASK | DEV_CTRL_SCLK_SEL_MASK, dev_ctrl_type | DEV_CTRL_SCLK_SEL_DIV(4), DEV_CTRL);

	return MXST_SUCCESS;
}

/* Polling hc reg with mask */
static int mxic_uefc_poll_hc_reg(xfer_info_t *xfer, uint32_t reg, uint32_t mask)
{
	uint32_t val, n = 10000;

	do {
		val = MXIC_RD32(reg) & mask;
		n--;
		xfer->ops.delay_us(1);
	} while (!val && n);
	if (!val) {
		mxic_pr_err("TIMEOUT! reg(%02Xh) & mask(%08Xh): val(%08Xh)\r\n", reg, mask, val);
		return MXST_ERR_TIMEOUT;
	}
	return MXST_SUCCESS;
}

static void mxic_uefc_cs_start(xfer_info_t *xfer)
{
	switch (xfer->pkts->cs_ctrl) {
	case CS_CTRL_REGULAR:
	case CS_CTRL_ASSERT:
		/* Assert CS */
		MXIC_WR32(TFR_CTRL_DEV_ACT, TFR_CTRL);
		while (TFR_CTRL_DEV_ACT & MXIC_RD32(TFR_CTRL));
		break;
	default:
		break;
	}
}

static void mxic_uefc_cs_end(xfer_info_t *xfer)
{
	switch (xfer->pkts->cs_ctrl) {
	case CS_CTRL_REGULAR:
	case CS_CTRL_DEASSERT:
		/* De-assert CS */
		MXIC_WR32(TFR_CTRL_DEV_DIS, TFR_CTRL);
		while (TFR_CTRL_DEV_DIS & MXIC_RD32(TFR_CTRL));
		break;
	default:
		break;
	}

	/* Disable IO Mode */
	MXIC_WR32(TFR_CTRL_IO_END, TFR_CTRL);
	while (TFR_CTRL_IO_END & MXIC_RD32(TFR_CTRL));
}

static inline void mxic_uefc_err_dessert_cs(xfer_info_t *xfer)
{
	xfer->pkts->cs_ctrl = CS_CTRL_DEASSERT;
	mxic_uefc_cs_end(xfer);
	xfer->pkts->cs_ctrl = CS_CTRL_REGULAR;
}

static uint32_t mxic_uefc_conf(xfer_info_t *xfer, uint8_t en_dma)
{
	uint32_t conf =
			OP_CMD_CNT(!xfer->pkts->cmd.len ? 1 : xfer->pkts->cmd.len) |
			OP_CMD_BUSW(fmsb32(xfer->pkts->cmd.bw)) |
			OP_CMD_DTR(xfer->pkts->cmd.dtr);
	
	conf |= OP_ADDR_CNT(xfer->pkts->addr.len) |
			OP_ADDR_BUSW(fmsb32(xfer->pkts->addr.bw)) |
			OP_ADDR_DTR(xfer->pkts->addr.dtr);

	/* Dummy's bus width and DTR are determined by the data */
	conf |= OP_DMY_CNT(xfer->pkts->dummy.len, xfer->pkts->data.dtr, xfer->pkts->data.bw);

	conf |= OP_DATA_BUSW(fmsb32(xfer->pkts->data.bw)) |
			OP_DATA_DTR(xfer->pkts->data.dtr) |
			((DIR_RD == xfer->pkts->dir) ? OP_DD_RD : 0);

	UPDATE_WRITE(DEV_CTRL_DQS_EN, xfer->pkts->dqs ? DEV_CTRL_DQS_EN : 0, DEV_CTRL);
	UPDATE_WRITE(HC_CTRL_DATA_ORDER, xfer->pkts->word_mode ? 0 : HC_CTRL_DATA_ORDER, HC_CTRL);
	
	if (en_dma) {
		conf |= TFR_MODE_DMA_EN | TFR_MODE_CSB_DONT_CARE | TFR_MODE_DMA_KEEP_CSB;
	}

	return conf;
}

static int mxic_uefc_io_mode_xfer(xfer_info_t *xfer, void *tx, void *rx, uint32_t len,
		uint8_t is_data)
{
	uint32_t data, nbytes, tmp = 0, ofs = 0;
	uint8_t data_octal_dtr = is_data && xfer->pkts->data.dtr && (8 == xfer->pkts->data.bw);

	while (ofs < len) {
		int ret;

		nbytes = len - ofs;
		data = 0xffffffff;

		if (nbytes > 4) {
			nbytes = 4;
		}

		if (tx) {
			memcpy(&data, tx + ofs, nbytes);
			mxic_pr_dbg_b("tx data: %08X\r\n", data);
		}

		if (data_octal_dtr && (nbytes % 2)) {
			tmp = nbytes;
			nbytes++;
		}

		ret = mxic_uefc_poll_hc_reg(xfer, PRES_STS, PRES_STS_TX_NFULL);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		MXIC_WR32(data, TXD(nbytes % 4));
		ret = mxic_uefc_poll_hc_reg(xfer, PRES_STS, PRES_STS_RX_NEMPT);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		data = MXIC_RD32(RXD_REG);
		if (rx) {
			memcpy(rx + ofs, &data, tmp ? tmp : nbytes);
		}
		mxic_pr_dbg_b("rx data: %08X\r\n", data);
		ofs += nbytes;
	}

	return MXST_SUCCESS;
}

static int mxic_uefc_auto_xfer(xfer_info_t *xfer)
{
	int ret;
	uint8_t len_small_data = (xfer->pkts->data.len < BYTES_DMA_LIMIT) ?
			xfer->pkts->data.len : (xfer->pkts->data.len % ALIGN_BYTES_DMA);
	uint32_t reg_int_sts;
	uint64_t addr = xfer->pkts->addr.val;
	uint64_t len_data = xfer->pkts->data.len;
	uint8_t *buf_data = xfer->pkts->data.buf;

	MXIC_WR32(mxic_uefc_conf(xfer, 1), TFR_MODE);

//	mxic_pr_dbg("REG SAMPLE ADJ: %08X, DEV CTRL: %08X, HC_CTRL: %08X, TFR_MODE: %08X, CLK_CTRL: %08X\r\n",
//				MXIC_RD32(SAMPLE_ADJ), MXIC_RD32(DEV_CTRL), MXIC_RD32(HC_CTRL), MXIC_RD32(TFR_MODE), MXIC_RD32(CLK_CTRL));


	/* Enable IO Mode */
	MXIC_WR32(TFR_CTRL_IO_START, TFR_CTRL);
	while (TFR_CTRL_IO_START & MXIC_RD32(TFR_CTRL));

	mxic_uefc_cs_start(xfer);

	/* Set up command  */
	if (xfer->pkts->cmd.len) {
		/* Enable host controller, reset counter */
		MXIC_WR32(TFR_CTRL_HC_ACT, TFR_CTRL);
		while (TFR_CTRL_HC_ACT & MXIC_RD32(TFR_CTRL));

		ret = mxic_uefc_io_mode_xfer(xfer, (uint8_t *)&xfer->pkts->cmd.val, 0, xfer->pkts->cmd.len,
				0);
		if (MXST_SUCCESS != ret) {
			mxic_uefc_err_dessert_cs(xfer);
			return ret;
		}
	}

	/* Set up address. change to big endian */
	if (xfer->pkts->addr.len) {
		if (xfer->hc_prot_type != HC_PROT_RAWNAND) {
			if (-1 == swap64_nbytes(&addr, xfer->pkts->addr.len)) {
				return MXST_ERR_PARAM;
			}
		}
		ret = mxic_uefc_io_mode_xfer(xfer, (uint8_t *)&addr, 0, xfer->pkts->addr.len, 0);
		if (MXST_SUCCESS != ret) {
			mxic_uefc_err_dessert_cs(xfer);
			return ret;
		}
	}

	/* Setup dummy: dummy's bus width and DTR are determined by the data */
	if (xfer->pkts->dummy.len) {
		uint8_t len_dummy =
				(xfer->pkts->dummy.len * (xfer->pkts->data.dtr + 1)) / (8 / (xfer->pkts->data.bw));
		ret = mxic_uefc_io_mode_xfer(xfer, 0, 0, len_dummy, 0);
		if (MXST_SUCCESS != ret) {
			mxic_uefc_err_dessert_cs(xfer);
			return ret;
		}
	}

	if (len_small_data) {
		/* Set up read/write Data */
		ret = mxic_uefc_io_mode_xfer(xfer,
				DIR_WR == xfer->pkts->dir ? buf_data : 0,
				DIR_RD == xfer->pkts->dir ? buf_data : 0,
				len_small_data, 1);
		if (MXST_SUCCESS != ret) {
			mxic_uefc_err_dessert_cs(xfer);
			return ret;
		}
		len_data -= len_small_data;
		buf_data += len_small_data;
	}

	if (len_data) {
		/* Flush D-Cache to DRAM in range used by DMA */
		Xil_DCacheFlushRange((uint32_t)buf_data, len_data);

		/* Clear DMA_TFR_CMPLT & DMA_INIT in REG_INT_STS */
		MXIC_WR32(INT_STS_DMA_TFR_CMPLT | INT_STS_DMA_INT, INT_STS);

		MXIC_WR32(len_data, SDMA_CNT);
		MXIC_WR32((uint32_t)buf_data, SDMA_ADDR);

		do {
			reg_int_sts = MXIC_RD32(INT_STS);

			if (INT_STS_DMA_INT & reg_int_sts) {
				MXIC_WR32(INT_STS_DMA_INT, INT_STS);
				MXIC_WR32(MXIC_RD32(SDMA_ADDR), SDMA_ADDR);
			}
		} while (!(INT_STS_DMA_TFR_CMPLT & reg_int_sts));

		/* Disable D-Cache */
		Xil_DCacheInvalidateRange((uint32_t)buf_data, len_data);
	}
	mxic_uefc_cs_end(xfer);

	return MXST_SUCCESS;
}

static int mxic_uefc_io_mode(xfer_info_t *xfer)
{
	int ret = MXST_SUCCESS;
	uint8_t dummy_len;
	uint64_t addr = xfer->pkts->addr.val;

	if (xfer->pkts->cmd.len || xfer->pkts->addr.len || xfer->pkts->dummy.len || xfer->pkts->data.len) {
		MXIC_WR32(mxic_uefc_conf(xfer, 0), TFR_MODE);
	}
//	mxic_pr_dbg("REG SAMPLE ADJ: %08X, DEV CTRL: %08X, HC_CTRL: %08X, TFR_MODE: %08X, CLK_CTRL: %08X\r\n",
//		MXIC_RD32(SAMPLE_ADJ), MXIC_RD32(DEV_CTRL), MXIC_RD32(HC_CTRL), MXIC_RD32(TFR_MODE), MXIC_RD32(CLK_CTRL));

	/* Enable IO Mode */
	MXIC_WR32(TFR_CTRL_IO_START, TFR_CTRL);
	while (TFR_CTRL_IO_START & MXIC_RD32(TFR_CTRL));

	mxic_uefc_cs_start(xfer);

	/* Set up command  */
	if (xfer->pkts->cmd.len) {
		/* Enable host controller, reset counter */
		MXIC_WR32(TFR_CTRL_HC_ACT, TFR_CTRL);
		while (TFR_CTRL_HC_ACT & MXIC_RD32(TFR_CTRL));

		ret = mxic_uefc_io_mode_xfer(xfer, (uint8_t *)&xfer->pkts->cmd.val, 0, xfer->pkts->cmd.len,
				0);
		if (MXST_SUCCESS != ret) {
			mxic_uefc_err_dessert_cs(xfer);
			return ret;
		}
	}

	/* Set up address */
	if (xfer->pkts->addr.len) {
		if (xfer->hc_prot_type != HC_PROT_RAWNAND) {
			if (-1 == swap64_nbytes(&addr, xfer->pkts->addr.len)) {
				return MXST_ERR_PARAM;
			}
		}
		ret = mxic_uefc_io_mode_xfer(xfer, (uint8_t *)&addr, 0, xfer->pkts->addr.len, 0);
		if (MXST_SUCCESS != ret) {
			mxic_uefc_err_dessert_cs(xfer);
			return ret;
		}
	}

	/* Setup dummy: dummy's bus width and DTR are determined by the data */
	if (xfer->pkts->dummy.len) {
		dummy_len = (xfer->pkts->dummy.len * (xfer->pkts->data.dtr + 1)) / (8 / (xfer->pkts->data.bw));
		ret = mxic_uefc_io_mode_xfer(xfer, 0, 0, dummy_len, 0);
		if (MXST_SUCCESS != ret) {
			mxic_uefc_err_dessert_cs(xfer);
			return ret;
		}
	}

	/* Set up read/write Data */
	if (xfer->pkts->data.len) {
		ret = mxic_uefc_io_mode_xfer(xfer,
			DIR_WR == xfer->pkts->dir  ? xfer->pkts->data.buf : 0,
			DIR_RD == xfer->pkts->dir ? xfer->pkts->data.buf : 0,
			xfer->pkts->data.len, 1);
		if (MXST_SUCCESS != ret) {
			mxic_uefc_err_dessert_cs(xfer);
			return ret;
		}
	}

	mxic_uefc_cs_end(xfer);

	return MXST_SUCCESS;
}

static int mxic_uefc_map_mode(xfer_info_t *xfer)
{
	uint32_t addr = (uint32_t)xfer->pkts->addr.val;

	if (addr % ALIGN_BYTES_MAP || (addr + xfer->pkts->data.len) % ALIGN_BYTES_MAP) {
		mxic_pr_err("The starting or ending address is not align %d bytes in mapping mode\r\n");
		return MXST_ERR_NOT_SUP;
	}

	/* Disable IO Mode */
	MXIC_WR32(TFR_CTRL_IO_END, TFR_CTRL);
	while (TFR_CTRL_IO_END & MXIC_RD32(TFR_CTRL));

	if (DIR_RD == xfer->pkts->dir) {
		MXIC_WR32(mxic_uefc_conf(xfer, 0), MAP_RD_CTRL);
		MXIC_WR32(xfer->pkts->cmd.val, MAP_CMD_REG);

		memcpy(xfer->pkts->data.buf, (void *)(xfer->base_addr_map + addr), xfer->pkts->data.len);

	} else {
		MXIC_WR32(mxic_uefc_conf(xfer, 0), MAP_WR_CTRL);
		MXIC_WR32(xfer->pkts->cmd.val << 16, MAP_CMD_REG);

		memcpy((void *)(xfer->base_addr_map + addr), xfer->pkts->data.buf, xfer->pkts->data.len);
	}

	return MXST_SUCCESS;
}

static int mxic_uefc_dma_master_mode(xfer_info_t *xfer)
{
	int ret;
	uint8_t dummy_len;
	uint32_t reg_int_sts;
	uint64_t addr = xfer->pkts->addr.val;

	if (addr % ALIGN_BYTES_DMA || (addr + xfer->pkts->data.len) % ALIGN_BYTES_DMA) {
		mxic_pr_err("The starting or ending address is not align %d bytes in dma mode\r\n");
		return MXST_ERR_NOT_SUP;
	}

	MXIC_WR32(mxic_uefc_conf(xfer, 1), TFR_MODE);

	/* Flush D-Cache to DRAM in range used by DMA */
	Xil_DCacheFlushRange((uint32_t)xfer->pkts->data.buf, xfer->pkts->data.len);

	/* Clear DMA_TFR_CMPLT & DMA_INIT in REG_INT_STS */
	MXIC_WR32(INT_STS_DMA_TFR_CMPLT | INT_STS_DMA_INT, INT_STS);

	/* Enable IO Mode */
	MXIC_WR32(TFR_CTRL_IO_START, TFR_CTRL);
	while (TFR_CTRL_IO_START & MXIC_RD32(TFR_CTRL));

	mxic_uefc_cs_start(xfer);

	/* Set up command  */
	if (xfer->pkts->cmd.len) {
		/* Enable host controller, reset counter */
		MXIC_WR32(TFR_CTRL_HC_ACT, TFR_CTRL);
		while (TFR_CTRL_HC_ACT & MXIC_RD32(TFR_CTRL));

		ret = mxic_uefc_io_mode_xfer(xfer, (uint8_t *)&xfer->pkts->cmd.val, 0, xfer->pkts->cmd.len,
				0);
		if (MXST_SUCCESS != ret) {
			mxic_uefc_err_dessert_cs(xfer);
			return ret;
		}
	}

	/* Set up address. change to big endian */
	if (xfer->pkts->addr.len) {
		if (xfer->hc_prot_type != HC_PROT_RAWNAND) {
			if (-1 == swap64_nbytes(&addr, xfer->pkts->addr.len)) {
				return MXST_ERR_PARAM;
			}
		}
		ret = mxic_uefc_io_mode_xfer(xfer, (uint8_t *)&addr, 0, xfer->pkts->addr.len, 0);
		if (MXST_SUCCESS != ret) {
			mxic_uefc_err_dessert_cs(xfer);
			return ret;
		}
	}

	/* Setup dummy: dummy's bus width and DTR are determined by the data */
	if (xfer->pkts->dummy.len) {
		dummy_len = (xfer->pkts->dummy.len * (xfer->pkts->data.dtr + 1)) / (8 / (xfer->pkts->data.bw));
		ret = mxic_uefc_io_mode_xfer(xfer, 0, 0, dummy_len, 0);
		if (MXST_SUCCESS != ret) {
			mxic_uefc_err_dessert_cs(xfer);
			return ret;
		}
	}

	if (xfer->pkts->data.len) {
		MXIC_WR32(xfer->pkts->data.len, SDMA_CNT);
		MXIC_WR32((uint32_t)xfer->pkts->data.buf, SDMA_ADDR);

		do {
			reg_int_sts = MXIC_RD32(INT_STS);

			if (INT_STS_DMA_INT & reg_int_sts) {
				MXIC_WR32(INT_STS_DMA_INT, INT_STS);
				MXIC_WR32(MXIC_RD32(SDMA_ADDR), SDMA_ADDR);
			}

		} while (!(INT_STS_DMA_TFR_CMPLT & reg_int_sts));
	}
	/* Disable D-Cache */
	Xil_DCacheInvalidateRange((uint32_t)xfer->pkts->data.buf , xfer->pkts->data.len);

	mxic_uefc_cs_end(xfer);

	return MXST_SUCCESS;
}

static int mxic_uefc_cali_dqs_dis(xfer_info_t *xfer)
{
	int ret;
	uint8_t lb = 0xff, hb = 0xff, n;
	uint32_t reg_sample_adj = MXIC_RD32(SAMPLE_ADJ);
	char* msg;

	if (DIR_RD != xfer->pkts->dir) {
		mxic_pr_err("Wrong parameters for calibration, should be read operation\r\n");
		return MXST_ERR_PARAM;
	}

	if (xfer->pkts->data.dtr) {
		reg_sample_adj &= ~SAMPLE_ADJ_POINT_SEL_DDR(0x7U);
		msg = "DDR";
	} else {
		reg_sample_adj &= ~SAMPLE_ADJ_POINT_SEL_SDR(0x7U);
		msg = "SDR";
	}

	mxic_pr_dbg("REG SAMPLE ADJ: %08X, DEV CTRL: %08X, HC_CTRL: %08X\r\n",
			MXIC_RD32(SAMPLE_ADJ), MXIC_RD32(DEV_CTRL), MXIC_RD32(HC_CTRL));

	for (n = 0; n < 8; n++) {
		MXIC_WR32(reg_sample_adj | (xfer->pkts->data.dtr ?
				SAMPLE_ADJ_POINT_SEL_DDR(n) : SAMPLE_ADJ_POINT_SEL_SDR(n)), SAMPLE_ADJ);
		ret = mxic_uefc_io_mode(xfer);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		if (!memcmp(xfer->pkts->data.buf, xfer->buf_cali, xfer->pkts->data.len)) {
			if (0xff == lb) {
				lb = n;
			}
			hb = n;
		}
		mxic_pr_dbg("--SAMPLE POINT %s: %01d, lb: %01d, hb: %01d\r\n", msg, n, lb, hb);
	}
	if (0xff == lb || 0xff == hb) {
		return MXST_ERR_CALI;
	}
	n = lb + (hb - lb) / 2;
	MXIC_WR32(reg_sample_adj | (xfer->pkts->data.dtr ? SAMPLE_ADJ_POINT_SEL_DDR(n) :
			SAMPLE_ADJ_POINT_SEL_SDR(n)), SAMPLE_ADJ);

	mxic_pr_dbg("Calibration Pass, SAMPLE POINT %s set %01d\r\n", msg, n);

	return MXST_SUCCESS;
}

static int mxic_uefc_cali_dqs_en(xfer_info_t *xfer)
{
	int ret;
	uint8_t lb[32 / 4], hb[32 / 4], cmp[32 / 4], n, m, i;
	uint32_t reg_sample_adj = MXIC_RD32(SAMPLE_ADJ) & ~SAMPLE_ADJ_DQS_IDLY_DOPI(0x1fU);

	if (DIR_RD != xfer->pkts->dir) {
		mxic_pr_err("Wrong parameters for calibration, should be read operation\r\n");
		return MXST_ERR_PARAM;
	}

	memset(lb, 0xff, sizeof(lb));
	memset(hb, 0xff, sizeof(hb));
	memset(cmp, 0xff, sizeof(cmp));

	mxic_pr_dbg("REG SAMPLE ADJ: %08X, DEV CTRL: %08X, HC_CTRL: %08X\r\n",
			MXIC_RD32(SAMPLE_ADJ), MXIC_RD32(DEV_CTRL), MXIC_RD32(HC_CTRL));

	for (m = 0; m < 32; m += 4) {
		mxic_pr_dbg("--------------------------SIO: %02d\r\n", m);
		MXIC_WR32(SIO_IDLY_1_0123(m << 3), SIO_IDLY_1);
		MXIC_WR32(SIO_IDLY_2_4567(m << 3), SIO_IDLY_2);

		for (n = 0; n < 32; n += 2) {
			MXIC_WR32(reg_sample_adj | SAMPLE_ADJ_DQS_IDLY_DOPI(n), SAMPLE_ADJ);
			ret = mxic_uefc_io_mode(xfer);
			if (MXST_SUCCESS != ret) {
				return ret;
			}

			if (!memcmp(xfer->pkts->data.buf, xfer->buf_cali, xfer->pkts->data.len)) {
				if (0xff == lb[m / 4]) {
					lb[m / 4] = n;
				}
				hb[m/4] = n;
			} else {
#if CONF_PR_LEVEL > 1
//				for (i = 0; i < xfer->pkts->data.len; i++) {
//					mxic_pr_dbg("SIO_IDLY: %03d, DQS_IDLY: %02d, ret:exp(%02X:%02X @ %02X) <B%02d>, [%08X, %08X, %08X] %s\r\n",
//							m, n,
//							xfer->pkts->data.buf[i], xfer->buf_cali[i], xfer->pkts->data.buf[i] ^ xfer->buf_cali[i], i,
//							MXIC_RD32(SAMPLE_ADJ), MXIC_RD32(SIO_IDLY_1), MXIC_RD32(SIO_IDLY_2),
//							xfer->pkts->data.buf[i] != xfer->buf_cali[i] ? "**" : "");
//				}
#endif
			}
			mxic_pr_dbg("--DQS: %02d, lb: %03d, hb: %03d\r\n",n, lb[m/4], hb[m/4]);
		}
	}

	m = 0;
	n = 0;
	for (i = 0; i < 32 / 4; i++) {
		mxic_pr_dbg("SIO: %02d, lb: %02d, hb: %02d\r\n", i * 4, lb[i], hb[i]);
		if (0xff != lb[i] && 0xff != hb[i]) {
			if ((hb[i] - lb[i]) >= n) {
				n = hb[i] - lb[i];
				m = i;
			}
		}
	}

	if (!memcmp(lb, cmp, sizeof(lb)) || !memcmp(hb, cmp, sizeof(lb))) {
		return MXST_ERR_CALI;
	}

	n = lb[m] + n / 2;
	m *= 4;

	MXIC_WR32(reg_sample_adj | SAMPLE_ADJ_DQS_IDLY_DOPI(n), SAMPLE_ADJ);
	MXIC_WR32(SIO_IDLY_1_0123(m), SIO_IDLY_1);
	MXIC_WR32(SIO_IDLY_2_4567(m), SIO_IDLY_2);
	mxic_pr_dbg("Calibration pass, DQS IDLY set %02d, SIO IDLY set %02d\r\n", n, m);
	mxic_pr_dbg("  - SAMPLE_ADJ: %08X, SIO_IDLY_1: %08X: SIO_IDLY_2: %08X\r\n",
			MXIC_RD32(SAMPLE_ADJ), MXIC_RD32(SIO_IDLY_1), MXIC_RD32(SIO_IDLY_2));

	return MXST_SUCCESS;
}

static int mxic_uefc_xfer(xfer_info_t *xfer)
{
	int ret;

	if (xfer->hc_busy) {
		return MXST_ERR_HC_BUSY;
	}
	xfer->hc_busy = 1;

	/* Update channel, lun and port for each transmission */
	UPDATE_WRITE(HC_CTRL_CH_LUN_PORT_MASK, xfer->uefc.channel_conf, HC_CTRL);

	if (xfer->buf_cali) {
		if (xfer->pkts->dqs) {
			ret = mxic_uefc_cali_dqs_en(xfer);
		} else {
			ret = mxic_uefc_cali_dqs_dis(xfer);
		}
	} else {
		ret = mxic_uefc_io_mode(xfer);
//		ret = mxic_uefc_auto_xfer(xfer);
	}
	xfer->hc_busy = 0;
	return ret;
}

static inline void mxic_uefc_delay_us(uint32_t us)
{
	usleep(us);
}

static int mxic_uefc_parse_ver_regs(hc_ver_t *hc, rtl_ver_t *rtl)
{
	mxic_pr_dbg("Bitstream date: %02d-%02d-%02d (MM-DD-YY)\r\n",
			rtl->date / 10000, (rtl->date % 10000) / 100, rtl->date % 100);
	mxic_pr_dbg("Max. version: %d\r\n", rtl->ver_max);
	mxic_pr_dbg("Min. version: %d\r\n", rtl->ver_min);

	switch (hc->freq) {
	case 0:	mxic_pr_dbg("Max. IO Freq: 50MHz\r\n");		break;
	case 1:	mxic_pr_dbg("Max. IO Freq: 200MHz\r\n");	break;
	case 2: mxic_pr_dbg("Max. UI Freq: 800MHz\r\n");	break;
	case 3: mxic_pr_dbg("Max. UI Freq: 1200MHz\r\n");	break;
	case 4: mxic_pr_dbg("Max. UI Freq: 160MHz\r\n");	break;
	default:
		return MXST_ERR_NOT_SUP;
	}

	mxic_pr_dbg("Channel A :\r\n");
	mxic_pr_dbg("-- PHY support: %s\r\n", hc->phy_supl_ch_a ? "yes" : "no");
	mxic_pr_dbg("-- Protocol: %s\r\n",
			(0 == hc->prot_ch_a) ? "NA" :
			(1 == hc->prot_ch_a) ? "SPI" :
			(2 == hc->prot_ch_a) ? "ONFI" : "eMMC");
	mxic_pr_dbg("-- Voltage: %s\r\n",
			(0 == hc->volt_ch_a) ? "NA" :
			(1 == hc->volt_ch_a) ? "1.2v" :
			(2 == hc->volt_ch_a) ? "1.8v" : "3.3v");

	mxic_pr_dbg("Channel B :\r\n");
	mxic_pr_dbg("-- PHY support: %s\r\n", hc->phy_supl_ch_b ? "yes" : "no");
	mxic_pr_dbg("-- Protocol: %s\r\n",
			(0 == hc->prot_ch_b) ? "NA" :
			(1 == hc->prot_ch_b) ? "SPI" :
			(2 == hc->prot_ch_b) ? "ONFI" : "eMMC");
	mxic_pr_dbg("-- Voltage: %s\r\n",
			(0 == hc->volt_ch_b) ? "NA" :
			(1 == hc->volt_ch_b) ? "1.2v" :
			(2 == hc->volt_ch_b) ? "1.8v" : "3.3v");

	return MXST_SUCCESS;
}


static int mxic_uefc_channel_config(xfer_info_t *xfer, int ch_type, int port)
{
	int ret;
	hc_ver_t hc = MXIC_RD(hc_ver_t, HC_VER);
	rtl_ver_t rtl = MXIC_RD(rtl_ver_t, RTL_VER);
	uint32_t reg_cap1;
	uint16_t ncsbs;
	uint8_t dual_ch;

	switch (ch_type) {
	case HC_CH_PAR:
		mxic_pr_err("Parallel mode is not supported yet\r\n");
		return MXST_ERR_PLATFORM;
	case HC_CH_A:
		if (xfer->hc_prot_type != hc.prot_ch_a) {
			mxic_pr_err("Channel A specified flash type error, should be %s\r\n", PARS_CH(hc.prot_ch_a));
			return MXST_ERR_PLATFORM;
		}
		xfer->uefc.channel_conf = HC_CTRL_CH_LUN_PORT(A, 0, port);
		break;
	case HC_CH_B:
		if (xfer->hc_prot_type != hc.prot_ch_b) {
			mxic_pr_err("Channel B specified flash type error, should be %s\r\n", PARS_CH(hc.prot_ch_b));
			return MXST_ERR_PLATFORM;
		}
		xfer->uefc.channel_conf = HC_CTRL_CH_LUN_PORT(B, 0, port);
		break;
	default:
		mxic_pr_err("Channel setting error %d\r\n", ch_type);
		return MXST_ERR_PLATFORM;

		break;
	}

	ret = mxic_uefc_parse_ver_regs(&hc, &rtl);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	reg_cap1 = MXIC_RD32(CAP_1);
	ncsbs = (reg_cap1 & CAP_1_CSB_NUM_MASK) >> CAP_1_CSB_NUM_OFS;
	if (port >= ncsbs) {
		mxic_pr_err("Specified port(%d) has exceeded max. number of ports(%d)\r\n", port, ncsbs);
		return MXST_ERR_PLATFORM;
	}

	dual_ch = !!(reg_cap1 & CAP_1_DUAL_CH);
	if (dual_ch && (0 == hc.prot_ch_a || 0 == hc.prot_ch_b)) {
		mxic_pr_err("DUAL Channel is enable, but channel A: %d, channel B: %d\r\n",
				hc.prot_ch_a, hc.prot_ch_b);
		return MXST_ERR_PLATFORM;
	}

	mxic_pr_dbg("dual_ch: %d, csb num: %d, ch_a: %s, ch_b: %s\r\n",
			dual_ch, ncsbs, PARS_CH(hc.prot_ch_a), PARS_CH(hc.prot_ch_b));

	while (ncsbs--) {
		UPDATE_WRITE(HC_CTRL_CH_LUN_PORT_MASK, HC_CTRL_CH_LUN_PORT(A, 0, ncsbs), HC_CTRL);
		MXIC_WR32(UEFC_TOP_MAP_ADDR, TOP_MAP_ADDR_REG);
		if (dual_ch) {
			UPDATE_WRITE(HC_CTRL_CH_LUN_PORT_MASK, HC_CTRL_CH_LUN_PORT(B, 0, ncsbs), HC_CTRL);
			MXIC_WR32(UEFC_TOP_MAP_ADDR, TOP_MAP_ADDR_REG);
		}
	}

	mxic_uefc_hc_setup(xfer, xfer->uefc.channel_conf);

	return MXST_SUCCESS;
}

static void mxic_uefc_cap_init(xfer_info_t *xfer)
{
	/* HC supported in DTR mode */
	xfer->caps_hc.dtr = 1;
	/* HC supported in word mode */
	xfer->caps_hc.word_mode = 1;
	/* HC supported in 8/4/2/1 IOs */
	xfer->caps_hc.txrx_io = SET_HC_CAPS_IO(8);

	mxic_pr_dbg("UEFC host controller capabilities: DTR(%d), Word Mode(%d), IOs(%X)\r\n",
		xfer->caps_hc.dtr,
		xfer->caps_hc.word_mode,
		1 << fmsb32(xfer->caps_hc.txrx_io));
}

static int mxic_uefc_init(xfer_info_t *xfer, int ch_type, int port)
{
	int ret;

	xfer->base_addr = UEFC_BASE_ADDRESS;
	MXIC_WR32(UEFC_BASE_MAP_ADDR, BASE_MAP_ADDR);

	mxic_uefc_cap_init(xfer);

	ret = mxic_uefc_channel_config(xfer, ch_type, port);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	xfer->base_addr_map = UEFC_BASE_MAP_ADDR;
	xfer->max_map_size = UEFC_MAP_SIZE;

	mxic_pr_dbg("UEFC base address: %08X\r\n", xfer->base_addr);
	mxic_pr_dbg("UEFC map address [%08Xh : %08Xh] = %08Xh\r\n",
			UEFC_TOP_MAP_ADDR, UEFC_BASE_MAP_ADDR, UEFC_TOP_MAP_ADDR - UEFC_BASE_MAP_ADDR);

	UPDATE_WRITE(HC_CTRL_SIO_SHIFTER(3), HC_CTRL_SIO_SHIFTER(3), HC_CTRL);

	MXIC_WR32(CLK_CTRL_RX_SS_A(1) | CLK_CTRL_RX_SS_B(1), CLK_CTRL);

	MXIC_WR32(INT_STS_ALL_CLR, INT_STS);
	MXIC_WR32(INT_STS_EN_ALL_EN, INT_STS_EN);
	MXIC_WR32(INT_STS_SIG_EN_ALL_EN, INT_STS_SIG_EN);

	MXIC_WR32(ERR_INT_STS_ALL_CLR, ERR_INT_STS);
	MXIC_WR32(ERR_INT_STS_EN_ALL_EN, ERR_INT_STS_EN);
	MXIC_WR32(ERR_INT_STS_SIG_EN_ALL_EN, ERR_INT_STS_SIG_EN);

	MXIC_WR32(INT_STS_DMA | INT_STS_EN_DMA_TFR_CMPLT, INT_STS_EN);

	MXIC_WR32(SAMPLE_ADJ_DQS_IDLY_DOPI(23) | SAMPLE_ADJ_POINT_SEL_DDR(0) |
			SAMPLE_ADJ_POINT_SEL_SDR(2), SAMPLE_ADJ);
	MXIC_WR32(SIO_IDLY_1_0123(0), SIO_IDLY_1);
	MXIC_WR32(SIO_IDLY_2_4567(0), SIO_IDLY_2);

	/* Enable host controller, reset counter */
	MXIC_WR32(TFR_CTRL_HC_ACT, TFR_CTRL);
	while (TFR_CTRL_HC_ACT & MXIC_RD32(TFR_CTRL));


	return MXST_SUCCESS;
}

int setup_mxic_uefc(xfer_info_t *xfer, int ch_type, int port)
{
	Xil_ICacheEnable();
	Xil_DCacheEnable();

    xfer->ops.xfer = mxic_uefc_xfer;
	xfer->ops.delay_us = mxic_uefc_delay_us;

	return mxic_uefc_init(xfer, ch_type, port);
}

void release_mxic_uefc(xfer_info_t *xfer)
{
	Xil_ICacheDisable();
	Xil_DCacheDisable();
}

void _gettimeofday(struct timeval *tv, void *tzvp)
{
	XTime xtime;
	uint64_t us;

	XTime_GetTime(&xtime);

	us = (xtime * 1000000) / (COUNTS_PER_SECOND);
	tv->tv_sec = us / 1000000;
	tv->tv_usec = us % 1000000;
}
