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

#include "mxmtd_config.h"
#include "rawnand_test.h"
#include "spinand_test.h"
#include "spinor_test.h"
#include "pass_through.h"
#include "platform_print.h"

int main()
{
	int ret = 0;

#ifdef CONF_RAWNAND
	ret = rawnand_test();
	mxic_pr_inf("<rawnand_test> ret: %08X\r\n", ret);
#endif

#ifdef CONF_SPINAND
	ret = spinand_test();
	mxic_pr_inf("<spinand_test> ret: %08X\r\n", ret);
#endif

#if defined(CONF_SPINOR)
	ret = spinor_test();
	mxic_pr_inf("<spinor_test> ret: %08X\r\n", ret);
#endif

	return ret;
}
