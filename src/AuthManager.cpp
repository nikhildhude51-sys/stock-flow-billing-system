#include "AuthManager.h"
#include "CryptoUtils.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>

AuthManager::AuthManager(const std::string& authFilePath, int sessionLifespanSeconds)
    : authFilePath_(authFilePath), sessionLifespanSeconds_(sessionLifespanSeconds) {
    if (!loadCredentials()) {
        initializeDefaultCredentials();
    }
}

bool AuthManager::loadCredentials() {
    std::ifstream file(authFilePath_);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    std::string content = buffer.str();
    if (content.empty()) return false;

    auto json = crow::json::load(content);
    if (!json || !json.has("username") || !json.has("salt") || !json.has("passwordHash")) {
        return false;
    }

    adminUsername_ = std::string(json["username"].s());
    adminSalt_ = std::string(json["salt"].s());
    adminPasswordHash_ = std::string(json["passwordHash"].s());

    return true;
}

bool AuthManager::saveCredentials() {
    std::ofstream file(authFilePath_, std::ios::trunc);
    if (file.is_open()) {
        crow::json::wvalue root;
        root["username"] = adminUsername_;
        root["salt"] = adminSalt_;
        root["passwordHash"] = adminPasswordHash_;
        file << root.dump();
        file.close();
        return true;
    }
    std::cerr << "[AuthManager] Error: Unable to write credentials to " << authFilePath_ << "\n";
    return false;
}

bool AuthManager::initializeDefaultCredentials() {
    // Check environment variables for configurable initial credentials
    const char* envUser = std::getenv("ADMIN_USERNAME");
    const char* envPass = std::getenv("ADMIN_PASSWORD");

    adminUsername_ = (envUser && std::strlen(envUser) > 0) ? std::string(envUser) : "admin";
    std::string rawPassword = (envPass && std::strlen(envPass) > 0) ? std::string(envPass) : "admin123";

    // Generate cryptographically random 128-bit salt (32 hex characters)
    adminSalt_ = CryptoUtils::generateRandomToken(16);

    // Compute multi-round salted hash
    adminPasswordHash_ = CryptoUtils::hashPassword(rawPassword, adminSalt_, 1000);

    // Save to local auth configuration file
    if (saveCredentials()) {
        std::cout << "[AuthManager] Initialized admin credentials in " << authFilePath_ << ".\n";
        std::cout << "[AuthManager] Default credentials -> Username: " << adminUsername_ 
                  << " | Password: (Configured/Default)\n";
        return true;
    }

    return false;
}

bool AuthManager::isClientLocked(const std::string& clientKey, int& remainingLockSeconds) {
    auto now = std::chrono::system_clock::now();
    auto it = rateLimits_.find(clientKey);
    if (it == rateLimits_.end()) {
        return false;
    }

    if (it->second.lockUntil > now) {
        remainingLockSeconds = static_cast<int>(
            std::chrono::duration_cast<std::chrono::seconds>(it->second.lockUntil - now).count()
        );
        return true;
    }

    // Lockout expired, reset if count was exhausted
    if (it->second.failedAttempts >= MAX_FAILED_ATTEMPTS) {
        it->second.failedAttempts = 0;
    }
    return false;
}

AuthManager::LoginResult AuthManager::authenticate(
    const std::string& username,
    const std::string& password,
    const std::string& clientKey) {

    std::lock_guard<std::mutex> lock(authMutex_);

    LoginResult result;
    result.success = false;

    // 1. Check Rate Limiting / Brute-force lockout
    int remainingSeconds = 0;
    if (isClientLocked(clientKey, remainingSeconds)) {
        result.errorMessage = "Too many failed login attempts. Account locked for " + 
                              std::to_string(remainingSeconds) + " seconds.";
        return result;
    }

    // 2. Salt and hash supplied password
    std::string suppliedHash = CryptoUtils::hashPassword(password, adminSalt_, 1000);

    // 3. Constant-time comparison to prevent timing attacks
    bool userValid = CryptoUtils::constantTimeEquals(username, adminUsername_);
    bool passValid = CryptoUtils::constantTimeEquals(suppliedHash, adminPasswordHash_);

    if (!userValid || !passValid) {
        // Record failed attempt
        auto& entry = rateLimits_[clientKey];
        entry.failedAttempts++;
        if (entry.failedAttempts >= MAX_FAILED_ATTEMPTS) {
            entry.lockUntil = std::chrono::system_clock::now() + std::chrono::seconds(LOCKOUT_SECONDS);
            result.errorMessage = "Too many failed attempts. Temporary lockout for 5 minutes.";
        } else {
            int remaining = MAX_FAILED_ATTEMPTS - entry.failedAttempts;
            result.errorMessage = "Invalid username or password. (" + std::to_string(remaining) + " attempt(s) remaining)";
        }
        return result;
    }

    // 4. Successful Login: Reset failed attempt counter
    rateLimits_[clientKey].failedAttempts = 0;

    // 5. Session Fixation Protection: Invalidate previous sessions for this username
    for (auto it = activeSessions_.begin(); it != activeSessions_.end(); ) {
        if (it->second.username == adminUsername_) {
            it = activeSessions_.erase(it);
        } else {
            ++it;
        }
    }

    // 6. Generate cryptographically random 256-bit session token
    std::string sessionToken = CryptoUtils::generateRandomToken(32);
    auto now = std::chrono::system_clock::now();

    SessionInfo session;
    session.username = adminUsername_;
    session.createdAt = now;
    session.expiresAt = now + std::chrono::seconds(sessionLifespanSeconds_);

    activeSessions_[sessionToken] = session;

    result.success = true;
    result.sessionToken = sessionToken;
    result.errorMessage = "";
    return result;
}

bool AuthManager::logout(const std::string& sessionToken) {
    std::lock_guard<std::mutex> lock(authMutex_);
    return activeSessions_.erase(sessionToken) > 0;
}

bool AuthManager::isValidSession(const std::string& sessionToken) {
    if (sessionToken.empty()) return false;

    std::lock_guard<std::mutex> lock(authMutex_);
    auto it = activeSessions_.find(sessionToken);
    if (it == activeSessions_.end()) {
        return false;
    }

    auto now = std::chrono::system_clock::now();
    if (it->second.expiresAt < now) {
        // Expired session: cleanup
        activeSessions_.erase(it);
        return false;
    }

    return true;
}

std::string AuthManager::getSessionUsername(const std::string& sessionToken) {
    std::lock_guard<std::mutex> lock(authMutex_);
    auto it = activeSessions_.find(sessionToken);
    if (it != activeSessions_.end()) {
        return it->second.username;
    }
    return "";
}

void AuthManager::cleanupExpiredSessions() {
    std::lock_guard<std::mutex> lock(authMutex_);
    auto now = std::chrono::system_clock::now();
    for (auto it = activeSessions_.begin(); it != activeSessions_.end(); ) {
        if (it->second.expiresAt < now) {
            it = activeSessions_.erase(it);
        } else {
            ++it;
        }
    }
}

std::string AuthManager::getAdminUsername() const {
    std::lock_guard<std::mutex> lock(authMutex_);
    return adminUsername_;
}

AuthManager::ChangePasswordResult AuthManager::changePassword(
    const std::string& currentPassword,
    const std::string& newPassword,
    const std::string& confirmPassword) {

    std::lock_guard<std::mutex> lock(authMutex_);
    ChangePasswordResult result;
    result.success = false;

    // 1. Verify current password using constant-time comparison
    std::string currentHash = CryptoUtils::hashPassword(currentPassword, adminSalt_, 1000);
    if (!CryptoUtils::constantTimeEquals(currentHash, adminPasswordHash_)) {
        result.errorMessage = "Current password is incorrect.";
        return result;
    }

    // 2. Check confirmation match
    if (newPassword != confirmPassword) {
        result.errorMessage = "New password and confirmation do not match.";
        return result;
    }

    // 3. Password strength policy:
    // Minimum 8 characters
    if (newPassword.length() < 8) {
        result.errorMessage = "New password must be at least 8 characters long.";
        return result;
    }

    // Must contain at least one letter and at least one digit or special character
    bool hasLetter = false;
    bool hasDigitOrSymbol = false;
    for (char c : newPassword) {
        if (std::isalpha(static_cast<unsigned char>(c))) hasLetter = true;
        else if (std::isdigit(static_cast<unsigned char>(c)) || std::ispunct(static_cast<unsigned char>(c))) hasDigitOrSymbol = true;
    }
    if (!hasLetter || !hasDigitOrSymbol) {
        result.errorMessage = "New password must contain at least one letter and one number or special character.";
        return result;
    }

    // Cannot be identical to current password
    if (CryptoUtils::constantTimeEquals(newPassword, currentPassword)) {
        result.errorMessage = "New password must be different from current password.";
        return result;
    }

    // 4. Generate new cryptographic salt and compute new hash
    std::string newSalt = CryptoUtils::generateRandomToken(16);
    std::string newHash = CryptoUtils::hashPassword(newPassword, newSalt, 1000);

    adminSalt_ = newSalt;
    adminPasswordHash_ = newHash;

    // 5. Persist to storage
    std::ofstream file(authFilePath_, std::ios::trunc);
    if (!file.is_open()) {
        result.errorMessage = "Failed to write updated credentials to storage.";
        return result;
    }
    crow::json::wvalue root;
    root["username"] = adminUsername_;
    root["salt"] = adminSalt_;
    root["passwordHash"] = adminPasswordHash_;
    file << root.dump();
    file.close();

    // 6. Invalidate all active sessions (forcing re-login across all devices)
    activeSessions_.clear();

    result.success = true;
    result.errorMessage = "";
    return result;
}

AuthManager::ChangeUsernameResult AuthManager::changeUsername(
    const std::string& currentPassword,
    const std::string& newUsername,
    const std::string& sessionToken) {

    (void)sessionToken; // May be used for audit or session-specific checks
    std::lock_guard<std::mutex> lock(authMutex_);
    ChangeUsernameResult result;
    result.success = false;

    // 1. Verify current password using constant-time comparison
    std::string currentHash = CryptoUtils::hashPassword(currentPassword, adminSalt_, 1000);
    if (!CryptoUtils::constantTimeEquals(currentHash, adminPasswordHash_)) {
        result.errorMessage = "Current password is incorrect.";
        return result;
    }

    // 2. Validate new username
    if (newUsername.empty()) {
        result.errorMessage = "New username cannot be empty.";
        return result;
    }

    if (newUsername.length() < 3 || newUsername.length() > 32) {
        result.errorMessage = "Username must be between 3 and 32 characters long.";
        return result;
    }

    // Ensure valid characters: alphanumeric, underscores, hyphens, periods
    for (char c : newUsername) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-' && c != '.') {
            result.errorMessage = "Username can only contain alphanumeric characters, underscores, hyphens, and periods.";
            return result;
        }
    }

    // Reject if identical to current username
    if (CryptoUtils::constantTimeEquals(newUsername, adminUsername_)) {
        result.errorMessage = "New username must be different from current username.";
        return result;
    }

    // 3. Update username while keeping existing salt and password hash intact
    adminUsername_ = newUsername;

    // 4. Persist to storage safely
    std::ofstream file(authFilePath_, std::ios::trunc);
    if (!file.is_open()) {
        result.errorMessage = "Failed to write updated credentials to storage.";
        return result;
    }
    crow::json::wvalue root;
    root["username"] = adminUsername_;
    root["salt"] = adminSalt_;
    root["passwordHash"] = adminPasswordHash_;
    file << root.dump();
    file.close();

    // 5. Update active sessions so existing sessions reflect the updated username
    for (auto& pair : activeSessions_) {
        pair.second.username = adminUsername_;
    }

    result.success = true;
    result.errorMessage = "";
    return result;
}
