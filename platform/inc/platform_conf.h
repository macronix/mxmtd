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

#ifndef _PLATFORM_CONF_H
#define _PLATFORM_CONF_H

#define CONF_HC_TYPE HC_MXIC_UEFC

#if CONF_HC_TYPE == HC_MXIC_UEFC    
    #include "../vendors/macronix/inc/mxic_uefc.h"

	#define UEFC_BASE_ADDRESS 		0x43a00000U
	#define UEFC_BASE_MAP_ADDR 		0x60000000U
	#define UEFC_MAP_SIZE			0x20000000U
	#define UEFC_TOP_MAP_ADDR 		(UEFC_BASE_MAP_ADDR + UEFC_MAP_SIZE)
	#define UEFC_BASE_EXT_DDR_ADDR	0x00000000U
	#define UEFC_ALIGN_MAP			4 /* Mapping mode alignment is 4 bytes */
	#define UEFC_ALIGN_DMA			4 /* DMA mode alignment is 4 bytes */

#else
    #error HW_CTRL_TYPE is invalid!
#endif

#endif
