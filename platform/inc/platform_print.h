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

#ifndef _PLATFORM_PRINT_H_
#define _PLATFORM_PRINT_H_

#include "xil_printf.h"

/*
 * PR_LEVEL 0: disable all print
 * PR_LEVEL 1: enable mxic_pr_inf, mxic_pr_err
 * PR_LEVEL 2: LEVEL 1 + mxic_pr_war
 * PR_LEVEL 3, LEVEL 2 + mxic_pr_dbg 
 * PR_LEVEL 4, LEVEL 3 + mxic_pr_dbg_a
 * PR_LEVEL 5, LEVEL 4 + mxic_pr_dbg_b
 **/

#define CONF_PR_LEVEL 2

#if CONF_PR_LEVEL > 0
    /* Based on each platform */
    #define mxic_pr_inf xil_printf

    #define mxic_pr_err(format, ...) mxic_pr_inf("* \033[1;31mERROR   [%04d, %s]:\033[0m " format, __LINE__, __func__, ##__VA_ARGS__)
#else
    #define mxic_pr_inf(...)
    #define mxic_pr_err(...)
#endif

#if CONF_PR_LEVEL > 1
    #define mxic_pr_war(format, ...) mxic_pr_inf("* \033[1;32mWARNING [%04d, %s]:\033[0m " format, __LINE__, __func__, ##__VA_ARGS__)
#else
        #define mxic_pr_war(...)
#endif

#if CONF_PR_LEVEL > 2
    #define mxic_pr_dbg(format, ...) mxic_pr_inf("* \033[1;36mDEBUG   [%04d, %s]:\033[0m " format, __LINE__, __func__, ##__VA_ARGS__)
#else
        #define mxic_pr_dbg(...)
#endif

#if CONF_PR_LEVEL > 3
    #define mxic_pr_dbg_a(format, ...) mxic_pr_inf("* \033[1;37mDEBUG-A [%04d, %s]:\033[0m " format, __LINE__, __func__, ##__VA_ARGS__)
#else
        #define mxic_pr_dbg_a(...)
#endif

#if CONF_PR_LEVEL > 4
    #define mxic_pr_dbg_b(format, ...) mxic_pr_inf("* \033[1;37mDEBUG-B [%04d, %s]:\033[0m] " format, __LINE__, __func__, ##__VA_ARGS__)
#else
        #define mxic_pr_dbg_b(...)
#endif

#endif
