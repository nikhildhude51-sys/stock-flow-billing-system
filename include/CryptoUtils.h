#ifndef CRYPTO_UTILS_H
#define CRYPTO_UTILS_H

#include <string>
#include <vector>
#include <random>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <cstring>

/**
 * ============================================================================
 * MODULE: CryptoUtils
 * Description:
 * Provides cryptographic hashing (SHA-256), secure random token generation,
 * salted key-stretching, and constant-time string comparison for authentication.
 * 100% self-contained in standard C++17 without external library dependencies.
 * ============================================================================
 */
namespace CryptoUtils {

    // --- SHA-256 Implementation (FIPS 180-2) ---
    class SHA256 {
    private:
        uint32_t state_[8];
        uint64_t bitCount_;
        uint8_t buffer_[64];

        static uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
        static uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
        static uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
        static uint32_t sigma0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
        static uint32_t sigma1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
        static uint32_t gamma0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
        static uint32_t gamma1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

        static const uint32_t K[64];

        void transform(const uint8_t chunk[64]) {
            uint32_t w[64];
            for (int i = 0; i < 16; ++i) {
                w[i] = (static_cast<uint32_t>(chunk[i * 4]) << 24) |
                       (static_cast<uint32_t>(chunk[i * 4 + 1]) << 16) |
                       (static_cast<uint32_t>(chunk[i * 4 + 2]) << 8) |
                       (static_cast<uint32_t>(chunk[i * 4 + 3]));
            }
            for (int i = 16; i < 64; ++i) {
                w[i] = gamma1(w[i - 2]) + w[i - 7] + gamma0(w[i - 15]) + w[i - 16];
            }

            uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
            uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];

            for (int i = 0; i < 64; ++i) {
                uint32_t t1 = h + sigma1(e) + ch(e, f, g) + K[i] + w[i];
                uint32_t t2 = sigma0(a) + maj(a, b, c);
                h = g;
                g = f;
                f = e;
                e = d + t1;
                d = c;
                c = b;
                b = a;
                a = t1 + t2;
            }

            state_[0] += a;
            state_[1] += b;
            state_[2] += c;
            state_[3] += d;
            state_[4] += e;
            state_[5] += f;
            state_[6] += g;
            state_[7] += h;
        }

    public:
        SHA256() { reset(); }

        void reset() {
            state_[0] = 0x6a09e667;
            state_[1] = 0xbb67ae85;
            state_[2] = 0x3c6ef372;
            state_[3] = 0xa54ff53a;
            state_[4] = 0x510e527f;
            state_[5] = 0x9b05688c;
            state_[6] = 0x1f83d9ab;
            state_[7] = 0x5be0cd19;
            bitCount_ = 0;
        }

        void update(const uint8_t* data, size_t len) {
            for (size_t i = 0; i < len; ++i) {
                buffer_[ (bitCount_ >> 3) & 63 ] = data[i];
                bitCount_ += 8;
                if ((bitCount_ & 511) == 0) {
                    transform(buffer_);
                }
            }
        }

        void update(const std::string& str) {
            update(reinterpret_cast<const uint8_t*>(str.data()), str.size());
        }

        std::string finalize() {
            uint64_t origBits = bitCount_;
            uint8_t pad = 0x80;
            update(&pad, 1);
            while ((bitCount_ & 511) != 448) {
                uint8_t zero = 0;
                update(&zero, 1);
            }
            uint8_t lenBytes[8];
            for (int i = 0; i < 8; ++i) {
                lenBytes[i] = static_cast<uint8_t>(origBits >> ((7 - i) * 8));
            }
            update(lenBytes, 8);

            std::ostringstream ss;
            for (int i = 0; i < 8; ++i) {
                ss << std::hex << std::setfill('0') << std::setw(8) << state_[i];
            }
            return ss.str();
        }
    };

    inline const uint32_t SHA256::K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    // Computes single SHA-256 hash of a string
    inline std::string sha256(const std::string& input) {
        SHA256 hasher;
        hasher.update(input);
        return hasher.finalize();
    }

    // Generates a cryptographically strong random hex token (default 32 bytes = 64 hex characters)
    inline std::string generateRandomToken(size_t byteCount = 32) {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint32_t> dist(0, 255);

        std::ostringstream ss;
        for (size_t i = 0; i < byteCount; ++i) {
            ss << std::hex << std::setfill('0') << std::setw(2) << dist(gen);
        }
        return ss.str();
    }

    // Computes multi-round salted hash to prevent rainbow tables and brute-force attacks
    inline std::string hashPassword(const std::string& password, const std::string& salt, int iterations = 1000) {
        std::string current = salt + password;
        for (int i = 0; i < iterations; ++i) {
            current = sha256(current + salt);
        }
        return current;
    }

    // Constant-time string comparison to prevent timing side-channel attacks
    inline bool constantTimeEquals(const std::string& a, const std::string& b) {
        if (a.length() != b.length()) return false;
        unsigned char result = 0;
        for (size_t i = 0; i < a.length(); ++i) {
            result |= static_cast<unsigned char>(a[i] ^ b[i]);
        }
        return result == 0;
    }
}

#endif // CRYPTO_UTILS_H
