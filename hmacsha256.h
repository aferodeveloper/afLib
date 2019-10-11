/*
 * Copyright (C) 2005-2007  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: hmacsha.h,v 1.7 2007/06/19 23:47:18 tbox Exp $ */

/*! \file isc/hmacsha.h
 * This is the header file for the HMAC-SHA1, HMAC-SHA224, HMAC-SHA256,
 * HMAC-SHA334 and HMAC-SHA512 hash algorithm described in RFC 2104.
 */

#ifndef HMACSHA256_H__
#define HMACSHA256_H__

#include <stdint.h>
#include <stddef.h>

#ifdef  __cplusplus
extern "C" {
#endif

#define IPAD 0x36
#define OPAD 0x5C

#define ISC_SHA256_BLOCK_LENGTH         64U
#define ISC_SHA256_DIGESTLENGTH         32U
#define ISC_SHA256_DIGESTSTRINGLENGTH	(ISC_SHA256_DIGESTLENGTH * 2 + 1)

#define ISC_HMACSHA256_KEYLENGTH ISC_SHA256_BLOCK_LENGTH

typedef uint8_t     isc_uint8_t;
typedef uint32_t    isc_uint32_t;
typedef uint64_t    isc_uint64_t;

/*
 * Keep buffer immediately after bitcount to preserve alignment.
 */
typedef struct {
    isc_uint32_t	state[8];
    isc_uint64_t	bitcount;
    isc_uint8_t	buffer[ISC_SHA256_BLOCK_LENGTH];
} isc_sha256_t;

typedef struct {
    isc_sha256_t sha256ctx;
    unsigned char key[ISC_HMACSHA256_KEYLENGTH];
} isc_hmacsha256_t;

typedef enum { isc_boolean_false = 0, isc_boolean_true = 1 } isc_boolean_t;

#define ISC_FALSE isc_boolean_false
#define ISC_TRUE isc_boolean_true
#define ISC_TF(x) ((x) ? ISC_TRUE : ISC_FALSE)

void isc_hmacsha256_init(isc_hmacsha256_t *ctx, const unsigned char *key, unsigned int len);
void isc_hmacsha256_invalidate(isc_hmacsha256_t *ctx);
void isc_hmacsha256_update(isc_hmacsha256_t *ctx, const unsigned char *buf, unsigned int len);
void isc_hmacsha256_sign(isc_hmacsha256_t *ctx, unsigned char *digest, size_t len);
isc_boolean_t isc_hmacsha256_verify(isc_hmacsha256_t *ctx, unsigned char *digest, size_t len);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif // HMACSHA256_H__
