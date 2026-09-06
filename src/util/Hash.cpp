#include "Hash.h"
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

namespace tribunal {
namespace util {

std::string sha256(const void* data, size_t size) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create EVP_MD_CTX");
    }

    int rc = EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    if (rc != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_DigestInit_ex failed");
    }

    rc = EVP_DigestUpdate(ctx, data, size);
    if (rc != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_DigestUpdate failed");
    }

    rc = EVP_DigestFinal_ex(ctx, hash, &hash_len);
    if (rc != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_DigestFinal_ex failed");
    }

    EVP_MD_CTX_free(ctx);

    /* Convert to hex string */
    std::ostringstream oss;
    for (unsigned int i = 0; i < hash_len; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(hash[i]);
    }
    return oss.str();
}

std::string sha256_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create EVP_MD_CTX");
    }

    int rc = EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    if (rc != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_DigestInit_ex failed");
    }

    /* Read file in 4KB chunks */
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        rc = EVP_DigestUpdate(ctx, buffer, file.gcount());
        if (rc != 1) {
            EVP_MD_CTX_free(ctx);
            throw std::runtime_error("EVP_DigestUpdate failed");
        }
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    rc = EVP_DigestFinal_ex(ctx, hash, &hash_len);
    if (rc != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_DigestFinal_ex failed");
    }

    EVP_MD_CTX_free(ctx);

    /* Convert to hex string */
    std::ostringstream oss;
    for (unsigned int i = 0; i < hash_len; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(hash[i]);
    }
    return oss.str();
}

/* ====================================================================
 * FNV-1a Hash
 * ==================================================================== */

uint32_t fnv1a_uint32(const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t hash = 2166136261u;  /* FNV offset basis for 32-bit */

    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= 16777619u;  /* FNV prime for 32-bit */
    }

    return hash;
}

std::string fnv1a(const void* data, size_t size) {
    uint32_t hash = fnv1a_uint32(data, size);

    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << hash;
    return oss.str();
}

/* ====================================================================
 * Hex Encoding/Decoding
 * ==================================================================== */

std::string hex_encode(const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    std::ostringstream oss;

    for (size_t i = 0; i < size; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(bytes[i]);
    }

    return oss.str();
}

std::vector<uint8_t> hex_decode(const std::string& hex) {
    std::vector<uint8_t> result;

    if (hex.size() % 2 != 0) {
        return result;  /* Invalid hex (odd length) */
    }

    for (size_t i = 0; i < hex.size(); i += 2) {
        try {
            uint8_t byte = static_cast<uint8_t>(
                std::stoi(hex.substr(i, 2), nullptr, 16));
            result.push_back(byte);
        } catch (...) {
            return std::vector<uint8_t>();  /* Invalid hex character */
        }
    }

    return result;
}

}  /* namespace util */
}  /* namespace tribunal */
