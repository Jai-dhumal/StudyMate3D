#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#ifndef CALG_SHA_256
#define CALG_SHA_256 0x0000800c
#define PROV_RSA_AES 24
#endif
#else
#include <random>
#endif

namespace studymate {

class CryptoUtils {
public:
    static std::string generate_random_hex(size_t num_bytes) {
        std::vector<unsigned char> bytes(num_bytes);
#ifdef _WIN32
        HCRYPTPROV hProv = 0;
        if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
            CryptGenRandom(hProv, static_cast<DWORD>(num_bytes), &bytes[0]);
            CryptReleaseContext(hProv, 0);
        } else {
            for (size_t i = 0; i < num_bytes; ++i) {
                bytes[i] = static_cast<unsigned char>(rand() % 256);
            }
        }
#else
        std::random_device rd;
        for (size_t i = 0; i < num_bytes; ++i) {
            bytes[i] = static_cast<unsigned char>(rd() & 0xFF);
        }
#endif
        std::ostringstream ss;
        for (size_t i = 0; i < bytes.size(); ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]);
        }
        return ss.str();
    }

    // Pure C++ Portable standard SHA-256 implementation (works on Windows, Linux, Docker, MacOS)
    static std::string sha256_hex(const std::string& input) {
        // Initial hash values: first 32 bits of the fractional parts of the square roots of the first 8 primes
        uint32_t h0 = 0x6a09e667, h1 = 0xbb67ae85, h2 = 0x3c6ef372, h3 = 0xa54ff53a;
        uint32_t h4 = 0x510e527f, h5 = 0x9b05688c, h6 = 0x1f83d9ab, h7 = 0x5be0cd19;

        static const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
        };

        // Pre-processing
        std::vector<uint8_t> msg(input.begin(), input.end());
        uint64_t bit_len = static_cast<uint64_t>(msg.size()) * 8;
        msg.push_back(0x80);
        while ((msg.size() % 64) != 56) {
            msg.push_back(0x00);
        }
        for (int i = 7; i >= 0; --i) {
            msg.push_back(static_cast<uint8_t>((bit_len >> (i * 8)) & 0xFF));
        }

        auto rotr = [](uint32_t x, uint32_t n) -> uint32_t {
            return (x >> n) | (x << (32 - n));
        };

        // Process each 64-byte chunk
        for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
            uint32_t w[64];
            for (size_t i = 0; i < 16; ++i) {
                w[i] = (static_cast<uint32_t>(msg[chunk + i * 4]) << 24) |
                       (static_cast<uint32_t>(msg[chunk + i * 4 + 1]) << 16) |
                       (static_cast<uint32_t>(msg[chunk + i * 4 + 2]) << 8) |
                       (static_cast<uint32_t>(msg[chunk + i * 4 + 3]));
            }
            for (size_t i = 16; i < 64; ++i) {
                uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
                uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
                w[i] = w[i - 16] + s0 + w[i - 7] + s1;
            }

            uint32_t a = h0, b = h1, c = h2, d = h3, e = h4, f = h5, g = h6, h = h7;

            for (size_t i = 0; i < 64; ++i) {
                uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
                uint32_t ch = (e & f) ^ ((~e) & g);
                uint32_t temp1 = h + S1 + ch + k[i] + w[i];
                uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
                uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
                uint32_t temp2 = S0 + maj;

                h = g;
                g = f;
                f = e;
                e = d + temp1;
                d = c;
                c = b;
                b = a;
                a = temp1 + temp2;
            }

            h0 += a; h1 += b; h2 += c; h3 += d;
            h4 += e; h5 += f; h6 += g; h7 += h;
        }

        std::ostringstream ss;
        uint32_t parts[8] = { h0, h1, h2, h3, h4, h5, h6, h7 };
        for (int i = 0; i < 8; ++i) {
            ss << std::hex << std::setw(8) << std::setfill('0') << parts[i];
        }
        return ss.str();
    }

    // Secure salted PBKDF-like iterative hashing (1,000 iterations of SHA-256 with salt)
    static std::string hash_password(const std::string& password, const std::string& existing_salt = "") {
        std::string salt = existing_salt.empty() ? generate_random_hex(16) : existing_salt;
        std::string current = password + salt;
        for (int i = 0; i < 1000; ++i) {
            current = sha256_hex(current + salt);
        }
        return salt + "$" + current;
    }

    static bool verify_password(const std::string& password, const std::string& stored_hash) {
        size_t delim = stored_hash.find('$');
        if (delim == std::string::npos) return false;
        std::string salt = stored_hash.substr(0, delim);
        std::string expected = hash_password(password, salt);
        if (expected.length() != stored_hash.length()) return false;
        int diff = 0;
        for (size_t i = 0; i < expected.length(); ++i) {
            diff |= (expected[i] ^ stored_hash[i]);
        }
        return diff == 0;
    }

    static std::string generate_id(const std::string& prefix = "id") {
        return prefix + "_" + generate_random_hex(8);
    }

    static std::string get_current_iso_time() {
        time_t now = time(nullptr);
        tm* gmt = gmtime(&now);
        char buf[64];
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", gmt);
        return std::string(buf);
    }

    static std::string get_current_date() {
        time_t now = time(nullptr);
        tm* lt = localtime(&now);
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d", lt);
        return std::string(buf);
    }

    static long long get_current_epoch_seconds() {
        return static_cast<long long>(time(nullptr));
    }
};

} // namespace studymate
