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

#ifndef _SPINAND_IDS_H_
#define _SPINAND_IDS_H_

typedef struct {
	char *name;
	uint8_t *id;
	uint8_t len_id;
	struct {
		uint8_t strength;
		uint32_t size_step;
	} ecc;
} id_info_t;

#define BIT(x) (1U << (x))

#define SPCL_QE 			BIT(0)
#define SPCL_PLANE_SEL_PROG	BIT(1)
#define SPCL_PLANE_SEL_READ BIT(2)

#define IDS(_id...)								\
	.id = (uint8_t[]){_id}, 					\
	.len_id = (uint8_t)sizeof((uint8_t[]){_id})
#define ECC_INFO(_strength, _size_step)			\
	.ecc.strength = (_strength), 				\
	.ecc.size_step = (_size_step)
#define SPCL(_flag)								\
	.flag_spcl = (_flag)

#define ID_INFOS(_name, _id_info...) static snand_id_info_t _name = {_id_info, {}}

ID_INFOS(ids[],
	/* 3V 4Gb ECC Free */
	{"MX35LF4GE4AD",
		IDS(0xc2, 0x37, 0x03),
		ECC_INFO(8, 512),
		SPCL(SPCL_QE),
	},
	/* 3V 4Gb ECC Require */
	{"MX35LF4G24AD",
		IDS(0xc2, 0x35, 0x03),
		ECC_INFO(8, 512),
		SPCL(SPCL_QE | SPCL_PLANE_SEL_PROG),
	},
	{"MX35LF4G24AD-Z4I8",
		IDS(0xc2, 0x75, 0x03),
		ECC_INFO(8, 512),
		SPCL(SPCL_QE),
	},
	/* 3V 2Gb ECC Free */
	{"MX35LF2GE4AD",
		IDS(0xc2, 0x22),
		ECC_INFO(8, 512),
		SPCL(SPCL_QE),
	},
	/* 3V 2Gb ECC Require */
	{"MX35LF2G24AD",
		IDS(0xc2, 0x24, 0x03),
		ECC_INFO(8, 512),
		SPCL(SPCL_QE | SPCL_PLANE_SEL_PROG),
	},
	{"MX35LF2G24AD-Z4I8",
		IDS(0xc2, 0x64, 0x03),
		ECC_INFO(8, 512),
		SPCL(SPCL_QE),
	},
	{"MX35LF2G14AC",
		IDS(0xc2, 0x20),
		ECC_INFO(4, 512),
		SPCL(SPCL_QE | SPCL_PLANE_SEL_PROG | SPCL_PLANE_SEL_READ),
	},
	/* 3V 1G ECC Free */
	{"MX35LF1GE4AB",
		IDS(0xc2, 0x12),
		ECC_INFO(4, 512),
		SPCL(SPCL_QE),
	},
	/* 3V 1G ECC Require */
	{"MX35LF1G24AD",
		IDS(0xc2, 0x14, 0x03),
		ECC_INFO(8, 512),
		SPCL(SPCL_QE),
	},
	/* 3V 2G ECC Free, EOL */
	{"MX35LF2GE4AB",
		IDS(0xc2, 0x22),
		ECC_INFO(4, 512),
		SPCL(SPCL_QE | SPCL_PLANE_SEL_PROG | SPCL_PLANE_SEL_READ),
	},
	/* 1.8V 4Gb ECC Free */
	{"MX35UF4GE4AD",
		IDS(0xc2, 0xb7, 0x03),
		ECC_INFO(8, 512),
		SPCL(SPCL_QE),
	},
	/* 1.8V 4Gb ECC Require */
	{"MX35UF4G24AD",
		IDS(0xc2, 0xb5, 0x03),
		ECC_INFO(8, 512),
		SPCL(SPCL_QE | SPCL_PLANE_SEL_PROG),
	},
	{"MX35UF4G24AD-Z4I8",
		IDS(0xc2, 0xf5, 0x03),
		ECC_INFO(8, 512),
		SPCL(SPCL_QE),
	},
	/* 1.8V 2Gb ECC Free */
	{"MX35UF2GE4AD",
		IDS(0xc2, 0xa6, 0x03),
		ECC_INFO(8, 512),
		SPCL(SPCL_QE),
	},
	{"MX35UF2GE4AC",
		IDS(0xc2, 0xa2, 0x01),
		ECC_INFO(4, 512),
		SPCL(SPCL_QE),
	},
	/* 1.8V 2Gb ECC Require */
	{"MX35UF2G24AD",
		IDS(0xc2, 0xa4, 0x03),
		ECC_INFO(8, 512),
		SPCL(SPCL_QE | SPCL_PLANE_SEL_PROG),
	},
	{"MX35UF2G24AD-Z4I8",
		IDS(0xc2, 0xe4, 0x03),
		ECC_INFO(8, 512),
		SPCL(SPCL_QE),
	},
	{"MX35UF2G14AC",
		IDS(0xc2, 0xa0),
		ECC_INFO(4, 512),
		SPCL(SPCL_QE),
	},
	/* 1.8V 2Gb ECC Free */
	{"MX35UF1GE4AD",
		IDS(0xc2, 0x96, 0x03),
		ECC_INFO(8, 512),
		SPCL(SPCL_QE),
	},
	{"MX35UF1GE4AC",
		IDS(0xc2, 0x92, 0x01),
		ECC_INFO(4, 512),
		SPCL(SPCL_QE),
	},
	/* 1.8V 2Gb ECC Require */
	{"MX35UF1G24AD",
		IDS(0xc2, 0x94, 0x03),
		ECC_INFO(8, 512),
		SPCL(SPCL_QE),
	},
	{"MX35UF1G14AC",
		IDS(0xc2, 0x90),
		ECC_INFO(4, 512),
		SPCL(SPCL_QE),
	}
);

#endif
