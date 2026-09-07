#ifndef TRIBUNAL_HASH_H
#define TRIBUNAL_HASH_H

/**
 * util/Hash.h - Hashing utilities
 *
 * Phase 1.3.3: Wrappers for SHA256 (openssl) and FNV hashing
 *
 * Used for:
 * - SHA256: File content hashing (for caching in Phase 2)
 * - FNV: Quick hash for task/role identification (decision trees)
 *
 * Both produce hex-encoded output suitable for database storage.
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace tribunal {
namespace util {

/**
 * sha256 - Compute SHA256 hash of data
 *
 * Uses OpenSSL EVP interface. Produces 64-character hex string.
 *
 * @param data   Input bytes to hash
 * @param size   Number of bytes
 * @return  64-character lowercase hex SHA256 hash
 * @throws std::runtime_error if hashing fails
 *
 * Example:
 *   std::string hash = sha256("hello", 5);
 *   // hash == "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824"
 */
std::string sha256(const void* data, size_t size);

/**
 * sha256 - Compute SHA256 hash of string
 *
 * Convenience overload for std::string input.
 *
 * @param str   Input string to hash
 * @return  64-character lowercase hex SHA256 hash
 */
inline std::string sha256(const std::string& str) {
    return sha256(str.data(), str.size());
}

/**
 * sha256_file - Compute SHA256 hash of file contents
 *
 * Reads entire file and computes hash. Suitable for caching.
 *
 * @param path   File path to hash
 * @return  64-character lowercase hex SHA256 hash
 * @throws std::runtime_error if file cannot be read
 */
std::string sha256_file(const std::string& path);

/**
 * fnv1a - Compute FNV-1a hash (32-bit)
 *
 * Fast non-cryptographic hash for quick lookups.
 * Returns as 8-character hex string.
 *
 * Used for:
 * - Task hash identification
 * - Role identification
 * - Quick cache key generation
 *
 * @param data   Input bytes to hash
 * @param size   Number of bytes
 * @return  8-character lowercase hex FNV-1a hash
 *
 * Example:
 *   uint32_t h = fnv1a_uint32("hello", 5);  // Fast version (unsigned int)
 *   std::string s = fnv1a("hello", 5);       // String version
 */
std::string fnv1a(const void* data, size_t size);

/**
 * fnv1a - Compute FNV-1a hash of string
 *
 * Convenience overload.
 *
 * @param str   Input string to hash
 * @return  8-character lowercase hex FNV-1a hash
 */
inline std::string fnv1a(const std::string& str) {
    return fnv1a(str.data(), str.size());
}

/**
 * fnv1a_uint32 - Compute FNV-1a hash (raw 32-bit integer)
 *
 * Faster version for internal use (no hex encoding).
 *
 * @param data   Input bytes to hash
 * @param size   Number of bytes
 * @return  32-bit FNV-1a hash value
 */
uint32_t fnv1a_uint32(const void* data, size_t size);

/**
 * fnv1a_uint32 - Compute FNV-1a hash of string (raw value)
 *
 * @param str   Input string to hash
 * @return  32-bit FNV-1a hash value
 */
inline uint32_t fnv1a_uint32(const std::string& str) {
    return fnv1a_uint32(str.data(), str.size());
}

/**
 * hex_encode - Convert binary data to hex string
 *
 * Utility for manually creating hex output.
 *
 * @param data   Input bytes
 * @param size   Number of bytes
 * @return  Lowercase hex-encoded string (2*size characters)
 */
std::string hex_encode(const void* data, size_t size);

/**
 * hex_decode - Convert hex string back to binary
 *
 * @param hex    Hex-encoded string (must be even length)
 * @return  Decoded bytes (or empty vector on error)
 */
std::vector<uint8_t> hex_decode(const std::string& hex);

}  /* namespace util */
}  /* namespace tribunal */

#endif /* TRIBUNAL_HASH_H */
