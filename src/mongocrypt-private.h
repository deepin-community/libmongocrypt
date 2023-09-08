/*
 * Copyright 2019-present MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MONGOCRYPT_PRIVATE_H
#define MONGOCRYPT_PRIVATE_H

#include "mongocrypt.h"
#include "mongocrypt-config.h"
#include "bson/bson.h"

#include "mongocrypt-dll-private.h"
#include "mongocrypt-log-private.h"
#include "mongocrypt-buffer-private.h"
#include "mongocrypt-cache-private.h"
#include "mongocrypt-cache-key-private.h"
#include "mongocrypt-mutex-private.h"
#include "mongocrypt-opts-private.h"
#include "mongocrypt-crypto-private.h"
#include "mongocrypt-cache-oauth-private.h"

#include "mongo_csfle-v1.h"


#define MONGOCRYPT_GENERIC_ERROR_CODE 1

#define CLIENT_ERR_W_CODE(code, ...) \
   _mongocrypt_set_error (           \
      status, MONGOCRYPT_STATUS_ERROR_CLIENT, code, __VA_ARGS__)

#define CLIENT_ERR(...) \
   CLIENT_ERR_W_CODE (MONGOCRYPT_GENERIC_ERROR_CODE, __VA_ARGS__)

#define KMS_ERR_W_CODE(code, ...) \
   _mongocrypt_set_error (        \
      status, MONGOCRYPT_STATUS_ERROR_KMS, code, __VA_ARGS__)

#define KMS_ERR(...) KMS_ERR_W_CODE (MONGOCRYPT_GENERIC_ERROR_CODE, __VA_ARGS__)

#define MONGOCRYPT_STR_AND_LEN(x) (x), (sizeof (x) / sizeof ((x)[0]) - 1)

#define MONGOCRYPT_DATA_AND_LEN(x) \
   ((uint8_t *) x), (sizeof (x) / sizeof ((x)[0]) - 1)

/* TODO: remove after integrating into libmongoc */
#define BSON_SUBTYPE_ENCRYPTED 6

/* TODO: Move these to mongocrypt-log-private.h? */
const char *
tmp_json (const bson_t *bson);

const char *
tmp_buf (const _mongocrypt_buffer_t *buf);


void
_mongocrypt_set_error (mongocrypt_status_t *status,
                       mongocrypt_status_type_t type,
                       uint32_t code,
                       const char *format,
                       ...);

typedef struct _mcr_csfle_v1_vtable {
#define MONGOC_CSFLE_FUNCTIONS_X                                             \
   /* status methods */                                                      \
   X_FUNC (status_create, mongo_csfle_v1_status *, void)                     \
   X_FUNC (status_destroy, void, mongo_csfle_v1_status *status)              \
   X_FUNC (status_get_error, int, const mongo_csfle_v1_status *status)       \
   X_FUNC (status_get_explanation,                                           \
           const char *,                                                     \
           const mongo_csfle_v1_status *status)                              \
   X_FUNC (status_get_code, int, const mongo_csfle_v1_status *status)        \
   /* lib methods */                                                         \
   X_FUNC (lib_create, mongo_csfle_v1_lib *, mongo_csfle_v1_status *status)  \
   X_FUNC (lib_destroy,                                                      \
           int,                                                              \
           mongo_csfle_v1_lib *lib,                                          \
           mongo_csfle_v1_status *status)                                    \
   /* query_analyzer methods */                                              \
   X_FUNC (query_analyzer_create,                                            \
           mongo_csfle_v1_query_analyzer *,                                  \
           mongo_csfle_v1_lib *lib,                                          \
           mongo_csfle_v1_status *status)                                    \
   X_FUNC (                                                                  \
      query_analyzer_destroy, void, mongo_csfle_v1_query_analyzer *analyzer) \
   X_FUNC (analyze_query,                                                    \
           uint8_t *,                                                        \
           mongo_csfle_v1_query_analyzer *analyer,                           \
           const uint8_t *documentBSON,                                      \
           const char *ns_str,                                               \
           uint32_t ns_len,                                                  \
           uint32_t *bson_len,                                               \
           mongo_csfle_v1_status *status)                                    \
   X_FUNC (get_version, uint64_t, void)                                      \
   X_FUNC (get_version_str, const char *, void)                              \
   /* Free bson data created by csfle */                                     \
   X_FUNC (bson_free, void, uint8_t *bson)

#define X_FUNC(Name, RetType, ...) RetType (*Name) (__VA_ARGS__);
   MONGOC_CSFLE_FUNCTIONS_X
#undef X_FUNC

} _mcr_csfle_v1_vtable;


struct _mongocrypt_t {
   bool initialized;
   _mongocrypt_opts_t opts;
   mongocrypt_mutex_t mutex;
   /* The collinfo and key cache are protected with an internal mutex. */
   _mongocrypt_cache_t cache_collinfo;
   _mongocrypt_cache_t cache_key;
   _mongocrypt_log_t log;
   mongocrypt_status_t *status;
   _mongocrypt_crypto_t *crypto;
   /* A counter, protected by mutex, for generating unique context ids */
   uint32_t ctx_counter;
   _mongocrypt_cache_oauth_t *cache_oauth_azure;
   _mongocrypt_cache_oauth_t *cache_oauth_gcp;
   /// A CSFLE DLL, initialized by mongocrypt_init
   mcr_dll csfle_lib;
   _mcr_csfle_v1_vtable csfle_vtable;
};

typedef enum {
   MONGOCRYPT_ENCRYPTION_ALGORITHM_NONE = 0,
   MONGOCRYPT_ENCRYPTION_ALGORITHM_DETERMINISTIC = 1,
   MONGOCRYPT_ENCRYPTION_ALGORITHM_RANDOM = 2
} mongocrypt_encryption_algorithm_t;


bool
_mongocrypt_validate_and_copy_string (const char *in,
                                      int32_t in_len,
                                      char **out) MONGOCRYPT_WARN_UNUSED_RESULT;

char *
_mongocrypt_new_string_from_bytes (const void *in, int len);

char *
_mongocrypt_new_json_string_from_binary (mongocrypt_binary_t *binary);


bool
_mongocrypt_parse_kms_providers (
   mongocrypt_binary_t *kms_providers_definition,
   _mongocrypt_opts_kms_providers_t *kms_providers,
   mongocrypt_status_t *status,
   _mongocrypt_log_t *log);

/* _mongocrypt_needs_credentials returns true if @crypt was configured to
 * request credentials for any KMS provider. */
bool
_mongocrypt_needs_credentials (mongocrypt_t *crypt);

/* _mongocrypt_needs_credentials returns true if @crypt was configured to
 * request credentials for @provider. */
bool
_mongocrypt_needs_credentials_for_provider (
   mongocrypt_t *crypt, _mongocrypt_kms_provider_t provider);

#endif /* MONGOCRYPT_PRIVATE_H */