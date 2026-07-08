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


#ifndef BCH_H
#define BCH_H

#include <stdint.h>

typedef struct {
    uint8_t m;
    uint8_t t;
    uint16_t n;
    uint16_t ecc_bits;
    uint8_t ecc_bytes;
    uint8_t ecc_words;
    uint16_t size_step;
    uint32_t len;
    uint8_t  le;
    uint16_t *a_pow;
    uint16_t *a_log;
    uint32_t *mod_tab;
    uint32_t *ecc;
    uint32_t *syn;
    uint32_t *elp;
    uint32_t *buf;
    uint32_t *buf2;
    uint32_t *g;
} bch_t;

/**
 * bch_init - initialize BCH codec
 * @m:         GF Galois Field parameter
 * @t:         Error Correction Capability
 * @size_step: NUmber of data bytes per encoding step
 * @bch_ret:   output，points to the allocated bch_t on success
 *
 * return 0 on success，or error code
 */
int bch_init(int m, int t, uint32_t size_step, bch_t **bch_ret);

/**
 * bch_encode - Perform BCH encoding
 * @bch:  BCH control structure
 * @data: input data（size_step bytes）
 * @ecc:  output ECC（ecc_bytes bytes），if NULL only updates internal ecc
 */
void bch_encode(bch_t *bch, uint8_t *data, uint8_t *ecc);

/**
 * bch_decode - Perform BCH decoding and error correction
 * @bch:  BCH control structure
 * @data: input/output data（size_step bytes，error bits are corrected in place）
 * @ecc:  Stored ECC（ecc_bytes bytes）
 *
 * return the number of corrected bits
 *   0 if no errors were found，
 *   -1 if errors were not correctable
 */
int bch_decode(bch_t *bch, uint8_t *data, uint8_t *ecc);

/**
 * bch_free - Release BCH control structure
 * @bch:  BCH control structure
 */
void bch_free(bch_t *bch);

#endif /* BCH_H */
