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

/* these functions is for Arm Cortex-A9 */

#include "arm/arm_pmu.h"

uint32_t get_pmu_ccntr()
{
	uint32_t val;
	asm volatile("MRC p15, 0, %0, c9, c13, 0" : "=r" (val));
	return val;
}

/* write PMCR bit 0 to 1 */
void pmu_en()
{
	uint32_t val;
	asm volatile("MRC p15, 0, %0, c9, c12, 0" : "=r" (val));
	val |= 0x01;
	/* Write val to  PMCR */
	asm volatile("MCR p15, 0, %0, c9, c12, 0" :: "r" (val));
}

/* write PMCNTENCLR bit 31 to 1*/
void pmu_ccntr_pause()
{
	asm volatile("MRC p15, 0, r0, c9, c12, 2");
	asm volatile("ORR r0, r0, #(1 << 31)");
	asm volatile("MCR p15, 0, r0, c9, c12, 2");
}

void pmu_ccntr_resume()
{
	asm volatile("MOV r0, #(1 << 31)");
	asm volatile("MCR p15, 0, r0, c9, c12, 1");
}

void pmu_ccntr_clear()
{
//	uint32_t val;
//	asm volatile("MRC p15, 0, %0, c9, c12, 0" : "=r" (val));
//	val |= 0x02;
//	asm volatile("MCR p15, 0, %0, c9, c12, 0" :: "r" (val));

	asm volatile("MRC p15, 0, r0, c9, c12, 0");
	asm volatile("ORR r0, r0, #(1 << 2)");
	asm volatile("MCR p15, 0, r0, c9, c12, 0");
}
