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
#if (CONF_RAWNAND_ECC_TYPE == ECC_SW_BCH) || (CONF_SPINAND_ECC_TYPE == ECC_SW_BCH)
#include <string.h>
#include <malloc.h>
#include "platform_print.h"
#include "ecc/bch.h"
#include "mxmtd_err.h"

#define BCH_MAX_M          13
#define BCH_MAX_T          12
#define MAX_GEN_POLY_SIZE  (BCH_MAX_M * BCH_MAX_T + 2)   /* 158 */
#define BCH_BUF3_MAX_WORDS  (((1 << BCH_MAX_M) / 8 / 4) + BCH_MAX_T)  /* 268 */

#ifdef CONF_BCH_SMALL_HEAP
static const uint8_t pow_m[] = {
	#if CONF_BCH_ECC_SIZE == 512
		#include "pow_m13.inc"
	#else
		#include "pow_m12.inc"
	#endif
};

static const uint8_t log_m[] = {
	#if CONF_BCH_ECC_SIZE == 512
		#include "log_m13.inc"
	#else
		#include "log_m12.inc"
	#endif
};
#endif

#define MALLOC_BCH(buf, size)  \
 do {       \
  (buf) = malloc(size);  \
  if (!(buf)) {    \
   bch_free(bch);   \
   return MXST_ERR_ALLOC; \
  }       \
 } while(0)

#define CALLOC_BCH(buf, num, size)  \
 do {        \
  (buf) = calloc((num), (size)); \
  if (!(buf)) {     \
   bch_free(bch);    \
   return MXST_ERR_ALLOC;  \
  }        \
 } while(0)       \

#define FREE_BCH(buf) \
 do {    \
  if (buf) {  \
   free(buf); \
   buf = 0; \
  }    \
 } while(0)

static inline uint32_t swap32_byte(uint32_t val)
{
    return (val & 0xff000000) >> 24 |
           (val & 0x00ff0000) >> 8  |
           (val & 0x0000ff00) << 8  |
           (val & 0x000000ff) << 24;
}

static inline int mod_n(bch_t *bch, uint32_t v)
{
    if (v < (2 * bch->n)) {
        return (v >= bch->n) ? (v - bch->n) : v;
    }

    while (v >= bch->n) {
        v -= bch->n;
        v = (v & bch->n) + (v >> bch->m);
    }
    return v;
}

static void build_syndrome(bch_t *bch)
{
    int i, j;
    int ecc_bits = bch->ecc_bits;
    uint32_t *ecc = bch->ecc;

    memset(bch->syn, 0, 2 * bch->t * sizeof(*bch->syn));

    while (ecc_bits > 0) {
        i = ecc_bits - 32;
        ecc_bits = i;
        while (*ecc > 0) {
            if ((*ecc & 1) != 0U) {
                for (j = 0; j < 2 * bch->t; j++) {
                    bch->syn[j] ^= bch->a_pow[mod_n(bch, (j + 1) * i)];
                }
            }
            *ecc >>= 1;
            i++;
        }
        ecc++;
    }
}

static int build_error_location_poly(bch_t *bch)
{
    int i, j, k;
    int deg, buf_deg, tmp_deg;
    int pp = -1;
    uint32_t tmp, dp = 1, d = bch->syn[0];

    memset(bch->elp, 0, (bch->t + 1) * sizeof(*bch->elp));
    buf_deg = 0;
    bch->buf[0] = 1;
    deg = 0;
    bch->elp[0] = 1;

    for (i = 0; (i < bch->t) && (deg <= bch->t); i++) {
        if (d != 0) {
            k = 2 * i - pp;
            if (buf_deg + k > deg) {
                tmp_deg = deg;
                for (j = 0; j <= deg; j++) {
                    bch->buf2[j] = bch->elp[j];
                }
            }
            tmp = bch->n + bch->a_log[d] - bch->a_log[dp];
            for (j = 0; j <= buf_deg; j++) {
                if (bch->buf[j] != 0) {
                    bch->elp[j + k] ^=
                        bch->a_pow[mod_n(bch, tmp + bch->a_log[bch->buf[j]])];
                }
            }
            if (buf_deg + k > deg) {
                deg = buf_deg + k;
                buf_deg = tmp_deg;
                for (j = 0; j <= tmp_deg; j++) {
                    bch->buf[j] = bch->buf2[j];
                }
                dp = d;
                pp = 2 * i;
            }
        }
        if (i < bch->t - 1) {
            k = 2 * i + 1;
            d = bch->syn[k + 1];
            for (j = 1; j <= deg; j++) {
                if (bch->elp[j] && bch->syn[k]) {
                    d ^= bch->a_pow[mod_n(bch,
                        bch->a_log[bch->elp[j]] + bch->a_log[bch->syn[k]])];
                }
                k--;
            }
        }
    }
    return (deg > bch->t) ? -1 : deg;
}

static int chien_search(bch_t *bch, int deg)
{
    int i, j, nroot = 0;
    int *root = (int *)bch->buf2;
    uint32_t syn, syn0;
    int reg[BCH_MAX_T + 1];
    int step[BCH_MAX_T + 1];
    int k = bch->n - bch->a_log[bch->elp[deg]];

    for (j = 0; j < deg; j++) {
        if (bch->elp[j]) {
            reg[j]  = mod_n(bch, bch->a_log[bch->elp[j]] + k);
            step[j] = j;
        } else {
            reg[j]  = -1;
            step[j] = 0;
        }
    }
    reg[deg]  = 0;
    step[deg] = deg;

    syn0 = bch->elp[0] ? bch->a_pow[reg[0]] : 0;

    for (i = 0; i <= bch->n; i++) {
        for (j = 1, syn = syn0; j <= deg; j++) {
            if (reg[j] >= 0) {
                syn ^= bch->a_pow[reg[j]];
            }
        }
        if (syn == 0) {
            root[nroot++] = bch->n - i;
            if (nroot == deg) {
                return nroot;
            }
        }

        for (j = 1; j <= deg; j++) {
            if (reg[j] >= 0) {
                reg[j] += step[j];
                if (reg[j] >= bch->n) {
                    reg[j] -= bch->n;
                }
            }
        }
    }
    return 0;
}

static void build_gf_table(bch_t *bch)
{
    uint32_t x, msb, poly;
    uint32_t prim_poly[] = {0x11d, 0x211, 0x409, 0x805, 0x1053, 0x201b};

    poly = prim_poly[bch->m - 8];
    msb  = 1 << bch->m;

    bch->a_pow[0] = 1;
    bch->a_log[1] = 0;
    x = 2;

    for (int i = 1; i < bch->n; i++) {
        bch->a_pow[i] = (uint16_t)x;
        bch->a_log[x] = (uint16_t)i;
        x <<= 1;
        if (x >= msb) {
            x ^= poly;
        }
    }

    bch->a_pow[bch->n] = 1;
    bch->a_log[0]      = 0;
}

static void build_mod_tables(bch_t *bch, const uint32_t *g)
{
    int i, j, b, d;
    int plen   = (bch->ecc_bits + 32) / 32;
    int ecclen = (bch->ecc_bits + 31) / 32;
    uint32_t data, hi, lo, *tab, poly;

    for (i = 0; i < 4; i++) {
        for (b = 0; b < 16; b++) {
            tab  = bch->mod_tab + (b * 4 + i) * bch->ecc_words;
            data = i << (2 * b);
            while (data) {
                d    = 0;
                poly = (data >> 1);
                while (poly) {
                    poly >>= 1;
                    d++;
                }
                data ^= g[0] >> (31 - d);
                for (j = 0; j < ecclen; j++) {
                    hi = (d < 31) ? g[j] << (d + 1) : 0;
                    lo = (j + 1 < plen) ? g[j + 1] >> (31 - d) : 0;
                    tab[j] ^= hi | lo;
                }
            }
        }
    }
}

static void build_generator_poly(bch_t *bch)
{
    int i, j, k, m, t;
    uint32_t n;
    uint32_t x[MAX_GEN_POLY_SIZE] = {0};

    for (t = 0, x[0] = 1, bch->ecc_bits = 0; t < bch->t; t++) {
        for (m = 0, i = 2 * t + 1; m < bch->m; m++) {
            x[bch->ecc_bits + 1] = 1;
            for (j = bch->ecc_bits; j > 0; j--) {
                if (x[j] != 0) {
                    x[j] = bch->a_pow[mod_n(bch, bch->a_log[x[j]] + i)] ^
                           x[j - 1];
                } else {
                    x[j] = x[j - 1];
                }
            }
            if (x[j]) {
                x[j] = bch->a_pow[mod_n(bch, bch->a_log[x[j]] + i)];
            }
            bch->ecc_bits++;
            i = mod_n(bch, 2 * i);
        }
    }

    for (k = bch->ecc_bits + 1, i = 0; k > 0; k = k - n) {
        n = (k > 32) ? 32 : k;
        for (j = 0; j < (int)n; j++) {
            if (x[k - 1 - j] != 0) {
                bch->g[i] |= 1U << (31 - j);
            }
        }
        i++;
    }
}

void bch_encode(bch_t *bch, uint8_t *data, uint8_t *ecc)
{
 int i, j, mlen;
    uint32_t w, ecc_words = bch->ecc_words, *p, *c[16], *t[16], *mod_tab_base = bch->mod_tab;
    uint32_t buf3[BCH_BUF3_MAX_WORDS] = {};

    memset(bch->ecc, 0, ecc_words * sizeof(*bch->ecc));
    memcpy((uint8_t *)buf3 + bch->ecc_words * sizeof(uint32_t), data, bch->size_step);

    for (i = 0; i < 16; i++) {
     t[i] = mod_tab_base + (i * 4 * ecc_words);
    }
    p    = buf3;
    mlen = bch->len / 4;

    while (mlen-- > 0) {
        w = bch->le ? swap32_byte(*p) : *p;
        p++;
        w ^= bch->ecc[0];

        for (i = 0; i < 16; i++) {
         c[i] = t[i] + ecc_words * ((w >> (i * 2)) & 0x03);
        }

        for (i = 0; i < ecc_words - 1;  i++) {
         uint32_t acc = c[0][i];
         for (j = 1; j < 16; j++) {
          acc ^= c[j][i];
         }
         bch->ecc[i] = bch->ecc[i + 1] ^ acc;
        }

        uint32_t acc = c[0][i];
        for (j = 1; j < 16; j++) {
         acc ^= c[j][i];
        }
        bch->ecc[i] = acc;
    }

    if (ecc != NULL) {
        uint8_t tmp[BCH_MAX_M * BCH_MAX_T / 8 + 4] = {};

        for (i = 0; i < ecc_words; i++) {
            uint32_t sw = swap32_byte(bch->ecc[i]);
            memcpy(tmp + i * sizeof(uint32_t), &sw, sizeof(uint32_t));
        }
        memcpy(ecc, tmp, bch->ecc_bytes);
    }
}

int bch_decode(bch_t *bch, uint8_t *data, uint8_t *ecc)
{
    int i, err = 0, nroot;
    int *root = (int *)bch->buf2;
    uint32_t nbits;

    bch_encode(bch, data, NULL);

    uint8_t ecc_buf[BCH_MAX_M * BCH_MAX_T / 8 + 4];
    memset(ecc_buf, 0, bch->ecc_words * sizeof(uint32_t));
    memcpy(ecc_buf, ecc, bch->ecc_bytes);

    for (i = 0; i < bch->ecc_words; i++) {
        uint32_t ecc_in;
        memcpy(&ecc_in, ecc_buf + i * sizeof(uint32_t), sizeof(uint32_t));
        ecc_in = swap32_byte(ecc_in);
        bch->ecc[i] ^= ecc_in;
        err |= bch->ecc[i];
    }

    if (err == 0) {
        return 0;
    }

    build_syndrome(bch);
    err = build_error_location_poly(bch);
    if (err <= 0) {
        return -1;
    }
    nroot = chien_search(bch, err);
    if (err != nroot) {
        return -1;
    }

    nbits = (bch->len * 8) + bch->ecc_bits;
    for (i = 0; i < err; i++) {
        root[i] = nbits - 1 - root[i];
        root[i] = (root[i] & ~7) | (7 - (root[i] & 7));

        if ((root[i] / 8) < (bch->ecc_words * 4)) {
            mxic_pr_dbg("Error bit is in ecc range form byte 0 to %d, SKIP!!\r\n",
                root[i] / 8);
            continue;
        }
        mxic_pr_dbg_a("Before correct: <%d> %02X\r\n",
            (root[i] / 8) - (bch->ecc_words * 4),
            data[(root[i] / 8) - (bch->ecc_words * 4)]);

        data[(root[i] / 8) - (bch->ecc_words * 4)] ^= 1 << (root[i] % 8);

        mxic_pr_dbg_a("After  correct: <%d> %02X\r\n",
            (root[i] / 8) - (bch->ecc_words * 4),
            data[(root[i] / 8) - (bch->ecc_words * 4)]);
    }

    return err;
}

int bch_init(int m, int t, uint32_t size_step, bch_t **bch_ret)
{
    bch_t *bch;
    uint32_t a = 1;
    uint8_t *p = (uint8_t *)&a;
    *bch_ret = 0;

    if ((m < 8) || (m > BCH_MAX_M) || (t < 1) || (t > BCH_MAX_T)) {
        mxic_pr_err("bch init failed, params should be m: 8~%d, t: 1~%d\r\n",
            BCH_MAX_M, BCH_MAX_T);
        return MXST_ERR_ECC;
    }

    CALLOC_BCH(bch, 1, sizeof(bch_t));

    bch->le        = (*p == 1);
    bch->size_step = size_step;
    bch->m         = m;
    bch->t         = t;
    bch->n         = (1 << m) - 1;
    bch->ecc_words = (m * t + 31) / 32;
    bch->len       = (bch->n + 1) / 8;

    mxic_pr_dbg("This system is %s endian\r\n", bch->le ? "Little" : "Big");

    CALLOC_BCH(bch->mod_tab, bch->ecc_words * 16 * 4, sizeof(*bch->mod_tab));
    CALLOC_BCH(bch->ecc, bch->ecc_words, sizeof(*bch->ecc));
    MALLOC_BCH(bch->buf, (t + 1) * sizeof(*bch->buf));
    MALLOC_BCH(bch->buf2, (t + 1) * sizeof(*bch->buf2));
    MALLOC_BCH(bch->syn, t * 2 * sizeof(*bch->syn));
    MALLOC_BCH(bch->elp, (t + 1) * sizeof(*bch->elp));
    CALLOC_BCH(bch->g, bch->ecc_words + 1, sizeof(*bch->g));

#ifdef CONF_BCH_SMALL_HEAP
    bch->a_pow = (uint16_t *)pow_m;
    bch->a_log = (uint16_t *)log_m;
#else
    MALLOC_BCH(bch->a_pow, (bch->n + 1) * sizeof(*bch->a_pow));
    MALLOC_BCH(bch->a_log, (bch->n + 1) * sizeof(*bch->a_log));
    build_gf_table(bch);
#endif

    build_generator_poly(bch);

    bch->ecc_bytes = (bch->ecc_bits + 7) / 8;
    build_mod_tables(bch, bch->g);
    FREE_BCH(bch->g);

    *bch_ret = bch;
    return MXST_SUCCESS;
}

void bch_free(bch_t *bch)
{
    if (bch != NULL) {
#ifndef CONF_BCH_SMALL_HEAP
        FREE_BCH(bch->a_pow);
        FREE_BCH(bch->a_log);
#endif
        FREE_BCH(bch->mod_tab);
        FREE_BCH(bch->ecc);
        FREE_BCH(bch->syn);
        FREE_BCH(bch->elp);
        FREE_BCH(bch->buf);
        FREE_BCH(bch->buf2);
        FREE_BCH(bch);
    }
}
#endif //#if (CONF_RAWNAND_ECC_TYPE == ECC_SW_BCH) || (CONF_SPINAND_ECC_TYPE == ECC_SW_BCH)
