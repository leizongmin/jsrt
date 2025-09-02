#ifndef __JSRT_STD_CRYPTO_DIGEST_H__
#define __JSRT_STD_CRYPTO_DIGEST_H__

#include "crypto_subtle.h"

// Digest algorithm implementation
typedef struct {
  void (*init)(void *ctx);
  int (*update)(void *ctx, const void *data, size_t len);
  int (*final)(unsigned char *md, void *ctx);
  size_t (*size)(void);
  void *(*ctx_new)(void);
  void (*ctx_free)(void *ctx);
} jsrt_digest_impl_t;

// Get digest implementation for algorithm
const jsrt_digest_impl_t *jsrt_get_digest_impl(jsrt_crypto_algorithm_t alg);

// Perform digest operation (blocking, for worker thread)
int jsrt_crypto_digest_data(jsrt_crypto_algorithm_t alg, const uint8_t *input, size_t input_len, uint8_t **output,
                            size_t *output_len);

#endif