#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include "crow/json.h"

/**
 * ============================================================================
 * CLASS: AuthManager
 * Description:
 * Manages admin authentication, salted password verification, secure session tokens,
 * session lifecycle, and brute-force rate limiting.
 * ============================================================================
 */
class AuthManager {
public:
    struct LoginResult {
        bool success;
        std::string sessionToken;
        std::string errorMessage;
    };

    struct ChangePasswordResult {
        bool success;
        std::string errorMessage;
    };

    struct ChangeUsernameResult {
        bool success;
        std::string errorMessage;
    };

    struct SessionInfo {
        std::string username;
        std::chrono::system_clock::time_point createdAt;
        std::chrono::system_clock::time_point expiresAt;
    };

    struct RateLimitEntry {
        int failedAttempts;
        std::chrono::system_clock::time_point lockUntil;
    };

private:
    std::string authFilePath_;
    std::string adminUsername_;
    std::string adminSalt_;
    std::string adminPasswordHash_;

    // Session store: token -> SessionInfo
    std::unordered_map<std::string, SessionInfo> activeSessions_;

    // Rate limiting: client key (IP) -> RateLimitEntry
    std::unordered_map<std::string, RateLimitEntry> rateLimits_;

    mutable std::mutex authMutex_;

    // Session lifespan (default 24 hours)
    int sessionLifespanSeconds_;

    // Brute-force protection settings
    static constexpr int MAX_FAILED_ATTEMPTS = 5;
    static constexpr int LOCKOUT_SECONDS = 300; // 5 minutes

    bool loadCredentials();
    bool initializeDefaultCredentials();

public:
    explicit AuthManager(const std::string& authFilePath = "data/auth.json", int sessionLifespanSeconds = 86400);

    // --- Authentication Operations ---
    LoginResult authenticate(const std::string& username, const std::string& password, const std::string& clientKey = "default");
    bool logout(const std::string& sessionToken);
    bool isValidSession(const std::string& sessionToken);
    std::string getSessionUsername(const std::string& sessionToken);
    ChangePasswordResult changePassword(const std::string& currentPassword,
                                        const std::string& newPassword,
                                        const std::string& confirmPassword);
    ChangeUsernameResult changeUsername(const std::string& currentPassword,
                                        const std::string& newUsername,
                                        const std::string& sessionToken = "");
    bool saveCredentials();

    // --- Rate Limiting Checks ---
    bool isClientLocked(const std::string& clientKey, int& remainingLockSeconds);

    // --- Maintenance ---
    void cleanupExpiredSessions();

    std::string getAdminUsername() const;
};

#endif // AUTH_MANAGER_H
