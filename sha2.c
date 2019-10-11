/*
 * Copyright (C) 2005-2007, 2009  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: sha2.c,v 1.13.332.2 2009/01/18 23:47:41 tbox Exp $ */

/*	$FreeBSD: src/sys/crypto/sha2/sha2.c,v 1.2.2.2 2002/03/05 08:36:47 ume Exp $	*/
/*	$KAME: sha2.c,v 1.8 2001/11/08 01:07:52 itojun Exp $	*/

/*
 * sha2.c
 *
 * Version 1.0.0beta1
 *
 * Written by Aaron D. Gifford <me@aarongifford.com>
 *
 * Copyright 2000 Aaron D. Gifford.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) AND CONTRIBUTOR(S) ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) OR CONTRIBUTOR(S) BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <string.h>
//#include <assert.h>
#include "hmacsha256.h"
#include "sha2.h"

/*
 * Constant used by SHA256/384/512_End() functions for converting the
 * digest to a readable hexadecimal character string:
 */
static const char *sha2_hex_digits = "0123456789abcdef";

/*** SHA-256: *********************************************************/
void
isc_sha256_init(isc_sha256_t *context) {
    if (context == (isc_sha256_t *)0) {
        return;
    }
    memcpy(context->state, sha256_initial_hash_value,
           ISC_SHA256_DIGESTLENGTH);
    memset(context->buffer, 0, ISC_SHA256_BLOCK_LENGTH);
    context->bitcount = 0;
}

#ifdef ISC_SHA2_UNROLL_TRANSFORM

/* Unrolled SHA-256 round macros: */

#if BYTE_ORDER == LITTLE_ENDIAN

#define ROUND256_0_TO_15(a,b,c,d,e,f,g,h)	\
	REVERSE32(*data++, W256[j]); \
	T1 = (h) + Sigma1_256(e) + Ch((e), (f), (g)) + \
	     K256[j] + W256[j]; \
	(d) += T1; \
	(h) = T1 + Sigma0_256(a) + Maj((a), (b), (c)); \
	j++


#else /* BYTE_ORDER == LITTLE_ENDIAN */

#define ROUND256_0_TO_15(a,b,c,d,e,f,g,h)	\
	T1 = (h) + Sigma1_256(e) + Ch((e), (f), (g)) + \
	     K256[j] + (W256[j] = *data++); \
	(d) += T1; \
	(h) = T1 + Sigma0_256(a) + Maj((a), (b), (c)); \
	j++

#endif /* BYTE_ORDER == LITTLE_ENDIAN */

#define ROUND256(a,b,c,d,e,f,g,h)	\
	s0 = W256[(j+1)&0x0f]; \
	s0 = sigma0_256(s0); \
	s1 = W256[(j+14)&0x0f]; \
	s1 = sigma1_256(s1); \
	T1 = (h) + Sigma1_256(e) + Ch((e), (f), (g)) + K256[j] + \
	     (W256[j&0x0f] += s1 + W256[(j+9)&0x0f] + s0); \
	(d) += T1; \
	(h) = T1 + Sigma0_256(a) + Maj((a), (b), (c)); \
	j++

void isc_sha256_transform(isc_sha256_t *context, const isc_uint32_t* data) {
	isc_uint32_t	a, b, c, d, e, f, g, h, s0, s1;
	isc_uint32_t	T1, *W256;
	int		j;

	W256 = (isc_uint32_t*)context->buffer;

	/* Initialize registers with the prev. intermediate value */
	a = context->state[0];
	b = context->state[1];
	c = context->state[2];
	d = context->state[3];
	e = context->state[4];
	f = context->state[5];
	g = context->state[6];
	h = context->state[7];

	j = 0;
	do {
		/* Rounds 0 to 15 (unrolled): */
		ROUND256_0_TO_15(a,b,c,d,e,f,g,h);
		ROUND256_0_TO_15(h,a,b,c,d,e,f,g);
		ROUND256_0_TO_15(g,h,a,b,c,d,e,f);
		ROUND256_0_TO_15(f,g,h,a,b,c,d,e);
		ROUND256_0_TO_15(e,f,g,h,a,b,c,d);
		ROUND256_0_TO_15(d,e,f,g,h,a,b,c);
		ROUND256_0_TO_15(c,d,e,f,g,h,a,b);
		ROUND256_0_TO_15(b,c,d,e,f,g,h,a);
	} while (j < 16);

	/* Now for the remaining rounds to 64: */
	do {
		ROUND256(a,b,c,d,e,f,g,h);
		ROUND256(h,a,b,c,d,e,f,g);
		ROUND256(g,h,a,b,c,d,e,f);
		ROUND256(f,g,h,a,b,c,d,e);
		ROUND256(e,f,g,h,a,b,c,d);
		ROUND256(d,e,f,g,h,a,b,c);
		ROUND256(c,d,e,f,g,h,a,b);
		ROUND256(b,c,d,e,f,g,h,a);
	} while (j < 64);

	/* Compute the current intermediate hash value */
	context->state[0] += a;
	context->state[1] += b;
	context->state[2] += c;
	context->state[3] += d;
	context->state[4] += e;
	context->state[5] += f;
	context->state[6] += g;
	context->state[7] += h;

	/* Clean up */
	a = b = c = d = e = f = g = h = T1 = 0;
}

#else /* ISC_SHA2_UNROLL_TRANSFORM */

static void
isc_sha256_transform(isc_sha256_t *context, const isc_uint32_t* data) {
    isc_uint32_t	a, b, c, d, e, f, g, h, s0, s1;
    isc_uint32_t	T1, T2, *W256;
    int		j;

    W256 = (isc_uint32_t*)context->buffer;

    /* Initialize registers with the prev. intermediate value */
    a = context->state[0];
    b = context->state[1];
    c = context->state[2];
    d = context->state[3];
    e = context->state[4];
    f = context->state[5];
    g = context->state[6];
    h = context->state[7];

    j = 0;
    do {
#if BYTE_ORDER == LITTLE_ENDIAN
        /* Copy data while converting to host byte order */
        REVERSE32(*data++,W256[j]);
        /* Apply the SHA-256 compression function to update a..h */
        T1 = h + Sigma1_256(e) + Ch(e, f, g) + K256[j] + W256[j];
#else /* BYTE_ORDER == LITTLE_ENDIAN */
        /* Apply the SHA-256 compression function to update a..h with copy */
		T1 = h + Sigma1_256(e) + Ch(e, f, g) + K256[j] + (W256[j] = *data++);
#endif /* BYTE_ORDER == LITTLE_ENDIAN */
        T2 = Sigma0_256(a) + Maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;

        j++;
    } while (j < 16);

    do {
        /* Part of the message block expansion: */
        s0 = W256[(j+1)&0x0f];
        s0 = sigma0_256(s0);
        s1 = W256[(j+14)&0x0f];
        s1 = sigma1_256(s1);

        /* Apply the SHA-256 compression function to update a..h */
        T1 = h + Sigma1_256(e) + Ch(e, f, g) + K256[j] +
             (W256[j&0x0f] += s1 + W256[(j+9)&0x0f] + s0);
        T2 = Sigma0_256(a) + Maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;

        j++;
    } while (j < 64);

    /* Compute the current intermediate hash value */
    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;
    context->state[4] += e;
    context->state[5] += f;
    context->state[6] += g;
    context->state[7] += h;

    /* Clean up */
    a = b = c = d = e = f = g = h = T1 = T2 = 0;
}

#endif /* ISC_SHA2_UNROLL_TRANSFORM */

void
isc_sha256_update(isc_sha256_t *context, const isc_uint8_t *data, size_t len) {
    unsigned int	freespace, usedspace;

    if (len == 0U) {
        /* Calling with no data is valid - we do nothing */
        return;
    }

    /* Sanity check: */
//    assert(context != (isc_sha256_t *)0 && data != (isc_uint8_t*)0);

    usedspace = (unsigned int)((context->bitcount >> 3) %
                               ISC_SHA256_BLOCK_LENGTH);
    if (usedspace > 0) {
        /* Calculate how much free space is available in the buffer */
        freespace = ISC_SHA256_BLOCK_LENGTH - usedspace;

        if (len >= freespace) {
            /* Fill the buffer completely and process it */
            memcpy(&context->buffer[usedspace], data, freespace);
            context->bitcount += freespace << 3;
            len -= freespace;
            data += freespace;
            isc_sha256_transform(context,
                                 (isc_uint32_t*)context->buffer);
        } else {
            /* The buffer is not yet full */
            memcpy(&context->buffer[usedspace], data, len);
            context->bitcount += len << 3;
            /* Clean up: */
            usedspace = freespace = 0;
            return;
        }
    }
    while (len >= ISC_SHA256_BLOCK_LENGTH) {
        /* Process as many complete blocks as we can */
        memcpy(context->buffer, data, ISC_SHA256_BLOCK_LENGTH);
        isc_sha256_transform(context, (isc_uint32_t*)context->buffer);
        context->bitcount += ISC_SHA256_BLOCK_LENGTH << 3;
        len -= ISC_SHA256_BLOCK_LENGTH;
        data += ISC_SHA256_BLOCK_LENGTH;
    }
    if (len > 0U) {
        /* There's left-overs, so save 'em */
        memcpy(context->buffer, data, len);
        context->bitcount += len << 3;
    }
    /* Clean up: */
    usedspace = freespace = 0;
}

void
isc_sha256_final(isc_uint8_t digest[], isc_sha256_t *context) {
    isc_uint32_t	*d = (isc_uint32_t*)digest;
    unsigned int	usedspace;
    isc_uint64_t    *p_bitcount;

    /* Sanity check: */
//    assert(context != (isc_sha256_t *)0);

    /* If no digest buffer is passed, we don't bother doing this: */
    if (digest != (isc_uint8_t*)0) {
        usedspace = (unsigned int)((context->bitcount >> 3) %
                                   ISC_SHA256_BLOCK_LENGTH);
#if BYTE_ORDER == LITTLE_ENDIAN
        /* Convert FROM host byte order */
        REVERSE64(context->bitcount,context->bitcount);
#endif
        if (usedspace > 0) {
            /* Begin padding with a 1 bit: */
            context->buffer[usedspace++] = 0x80;

            if (usedspace <= ISC_SHA256_SHORT_BLOCK_LENGTH) {
                /* Set-up for the last transform: */
                memset(&context->buffer[usedspace], 0,
                       ISC_SHA256_SHORT_BLOCK_LENGTH - usedspace);
            } else {
                if (usedspace < ISC_SHA256_BLOCK_LENGTH) {
                    memset(&context->buffer[usedspace], 0,
                           ISC_SHA256_BLOCK_LENGTH -
                           usedspace);
                }
                /* Do second-to-last transform: */
                isc_sha256_transform(context,
                                     (isc_uint32_t*)context->buffer);

                /* And set-up for the last transform: */
                memset(context->buffer, 0,
                       ISC_SHA256_SHORT_BLOCK_LENGTH);
            }
        } else {
            /* Set-up for the last transform: */
            memset(context->buffer, 0, ISC_SHA256_SHORT_BLOCK_LENGTH);

            /* Begin padding with a 1 bit: */
            *context->buffer = 0x80;
        }
        /* Set the bit count: */
        p_bitcount = (isc_uint64_t*)&context->buffer[ISC_SHA256_SHORT_BLOCK_LENGTH];
        *p_bitcount = context->bitcount;

        /* Final transform: */
        isc_sha256_transform(context, (isc_uint32_t*)context->buffer);

#if BYTE_ORDER == LITTLE_ENDIAN
        {
            /* Convert TO host byte order */
            int	j;
            for (j = 0; j < 8; j++) {
                REVERSE32(context->state[j],context->state[j]);
                *d++ = context->state[j];
            }
        }
#else
        memcpy(d, context->state, ISC_SHA256_DIGESTLENGTH);
#endif
    }

    /* Clean up state data: */
    memset(context, 0, sizeof(*context));
    usedspace = 0;
}

char *
isc_sha256_end(isc_sha256_t *context, char buffer[]) {
    isc_uint8_t	digest[ISC_SHA256_DIGESTLENGTH], *d = digest;
    unsigned int	i;

    /* Sanity check: */
//    assert(context != (isc_sha256_t *)0);

    if (buffer != (char*)0) {
        isc_sha256_final(digest, context);

        for (i = 0; i < ISC_SHA256_DIGESTLENGTH; i++) {
            *buffer++ = sha2_hex_digits[(*d & 0xf0) >> 4];
            *buffer++ = sha2_hex_digits[*d & 0x0f];
            d++;
        }
        *buffer = (char)0;
    } else {
        memset(context, 0, sizeof(*context));
    }
    memset(digest, 0, ISC_SHA256_DIGESTLENGTH);
    return buffer;
}

char *
isc_sha256_data(const isc_uint8_t* data, size_t len,
                char digest[ISC_SHA256_DIGESTSTRINGLENGTH])
{
    isc_sha256_t context;

    isc_sha256_init(&context);
    isc_sha256_update(&context, data, len);
    return (isc_sha256_end(&context, digest));
}
