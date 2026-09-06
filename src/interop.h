#ifndef INTEROP_H
#define INTEROP_H

/*
 * C↔C++ Interoperability Header
 *
 * This header provides seamless interop between C and C++ code during the
 * migration phase. Wraps external C libraries (libcurl, sqlite3, pthreads)
 * and internal C functions so they can be called from C++ code.
 *
 * Phase 0: Build infrastructure for parallel C/C++ compilation
 * Phase 1+: Gradually replace C code with C++ equivalents
 */

#ifdef __cplusplus
extern "C" {
#endif

/* External C libraries (libcurl, sqlite3, pthreads, openssl) */
#include <curl/curl.h>
#include <sqlite3.h>
#include <pthread.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

/* Internal C functions from council.c, ollama.c, db.c */
#include "council.h"

/*
 * C++ wrappers for C library functions will be added here as needed.
 * For now, the C standard library is directly usable from C++.
 *
 * Example wrapper (to be added in Phase 1):
 *
 * namespace interop {
 *   class Database {
 *   public:
 *     Database(const char* path);
 *     ~Database();
 *     int execute(const char* sql);
 *   private:
 *     sqlite3* db;
 *   };
 * }
 */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* INTEROP_H */
