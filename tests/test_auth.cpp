/**
 * Comprehensive C++ Unit and Regression Test Suite
 * Tests:
 * 1. CryptoUtils (SHA-256 test vectors, salted password hashing, constant-time comparison, token generation)
 * 2. AuthManager (Initialization, Login, Session Management, Logout, Rate-limiting & Brute Force Lockout)
 * 3. Business Logic (Product, Inventory, Bill, BillingSystem with verified stock deduction and persistence)
 */

#include "CryptoUtils.h"
#include "AuthManager.h"
#include "Product.h"
#include "Inventory.h"
#include "Bill.h"
#include "BillingSystem.h"

#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <cmath>

int g_passedTests = 0;
int g_failedTests = 0;

void report(const std::string& name, bool condition, const std::string& details = "") {
    if (condition) {
        std::cout << "  [PASS] " << name << "\n";
        g_passedTests++;
    } else {
        std::cerr << "  [FAIL] " << name;
        if (!details.empty()) {
            std::cerr << " (" << details << ")";
        }
        std::cerr << "\n";
        g_failedTests++;
    }
}

// ============================================================================
// 1. CRYPTO UTILS TESTS
// ============================================================================
void testCryptoUtils() {
    std::cout << "\n=== [TEST GROUP 1] CryptoUtils & Hashing ===\n";

    // Standard FIPS 180-2 SHA-256 Empty String Hash
    std::string emptySha = CryptoUtils::sha256("");
    report("SHA-256 empty string test vector",
           emptySha == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
           "Got: " + emptySha);

    // Standard NIST SHA-256 test vector for "abc"
    std::string abcSha = CryptoUtils::sha256("abc");
    report("SHA-256 'abc' NIST test vector",
           abcSha == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
           "Got: " + abcSha);

    // Constant-time string equals
    report("constantTimeEquals on identical strings",
           CryptoUtils::constantTimeEquals("my_secret_token_123", "my_secret_token_123") == true);
    report("constantTimeEquals on distinct strings",
           CryptoUtils::constantTimeEquals("my_secret_token_123", "my_secret_token_456") == false);
    report("constantTimeEquals on different lengths",
           CryptoUtils::constantTimeEquals("short", "much_longer_string") == false);

    // Random Token Generation
    std::string token1 = CryptoUtils::generateRandomToken(32);
    std::string token2 = CryptoUtils::generateRandomToken(32);
    report("generateRandomToken produces non-empty token", !token1.empty());
    report("generateRandomToken generates 64-char hex string (32 bytes)", token1.length() == 64);
    report("generateRandomToken produces unique values", token1 != token2);

    // Multi-round Salted Password Hashing
    std::string plain = "SuperSecretPassword123!";
    std::string salt = CryptoUtils::generateRandomToken(16);
    std::string hash1 = CryptoUtils::hashPassword(plain, salt, 1000);
    std::string hash2 = CryptoUtils::hashPassword(plain, salt, 1000);
    std::string wrongHash = CryptoUtils::hashPassword("WrongPassword", salt, 1000);

    report("hashPassword produces consistent hash for same password and salt",
           hash1 == hash2);
    report("hashPassword produces completely different hash for wrong password",
           hash1 != wrongHash);
    report("constantTimeEquals confirms valid password hash",
           CryptoUtils::constantTimeEquals(hash1, hash2));
}

// ============================================================================
// 2. AUTH MANAGER & SESSION TESTS
// ============================================================================
void testAuthManager() {
    std::cout << "\n=== [TEST GROUP 2] AuthManager Authentication & Sessions ===\n";

    std::string testAuthFile = "data/auth_test_runner.json";
    if (std::filesystem::exists(testAuthFile)) {
        std::filesystem::remove(testAuthFile);
    }

    // Initialize AuthManager with custom test file
    AuthManager auth(testAuthFile);

    report("AuthManager creates and saves test credentials file",
           std::filesystem::exists(testAuthFile));

    // Test successful login with default admin credentials
    auto loginOk = auth.authenticate("admin", "admin123", "192.168.1.100");
    report("Login with correct credentials succeeds",
           loginOk.success == true && !loginOk.sessionToken.empty());
    report("Login generates 64-char hex session token",
           loginOk.sessionToken.length() == 64);

    std::string activeToken = loginOk.sessionToken;

    // Test session validation
    report("isValidSession returns true for active session",
           auth.isValidSession(activeToken) == true);
    report("getSessionUsername returns 'admin'",
           auth.getSessionUsername(activeToken) == "admin");

    // Test session validation with fake / invalid token
    report("isValidSession returns false for bogus token",
           auth.isValidSession("bogus_token_xyz_12345678901234567890") == false);
    report("isValidSession returns false for empty token",
           auth.isValidSession("") == false);

    // Test wrong password
    auto loginBadPass = auth.authenticate("admin", "incorrect_password", "192.168.1.101");
    report("Login with incorrect password fails",
           loginBadPass.success == false && !loginBadPass.errorMessage.empty());

    // Test non-existent user
    auto loginBadUser = auth.authenticate("hacker", "password123", "192.168.1.102");
    report("Login with unknown username fails",
           loginBadUser.success == false && !loginBadUser.errorMessage.empty());

    // Test Logout / Session Invalidation
    bool logoutSuccess = auth.logout(activeToken);
    report("auth.logout returns true for active session", logoutSuccess);
    report("Session is invalid after logout",
           auth.isValidSession(activeToken) == false);
    report("getSessionUsername returns empty after logout",
           auth.getSessionUsername(activeToken).empty());

    // Clean up test file
    if (std::filesystem::exists(testAuthFile)) {
        std::filesystem::remove(testAuthFile);
    }
}

// ============================================================================
// 3. RATE LIMITING & BRUTE FORCE LOCKOUT TESTS
// ============================================================================
void testBruteForceLockout() {
    std::cout << "\n=== [TEST GROUP 3] Brute-Force Rate Limiting Lockout ===\n";

    std::string testAuthFile = "data/auth_test_lockout.json";
    if (std::filesystem::exists(testAuthFile)) {
        std::filesystem::remove(testAuthFile);
    }

    AuthManager auth(testAuthFile);
    std::string attackerIp = "10.0.0.99";
    int remainingSec = 0;

    // First 4 failed attempts should fail normally without lockout
    for (int i = 1; i <= 4; ++i) {
        auto res = auth.authenticate("admin", "wrong_attempt_" + std::to_string(i), attackerIp);
        assert(res.success == false);
        assert(!auth.isClientLocked(attackerIp, remainingSec));
    }
    report("First 4 failed attempts do not lock out", !auth.isClientLocked(attackerIp, remainingSec));

    // 5th failed attempt triggers lockout
    auto attempt5 = auth.authenticate("admin", "wrong_attempt_5", attackerIp);
    bool locked = auth.isClientLocked(attackerIp, remainingSec);
    report("5th failed attempt triggers lockout status",
           attempt5.success == false && locked);
    report("isClientLocked returns positive countdown (> 0)",
           remainingSec > 0);

    // 6th attempt from the same IP must be rejected immediately due to lockout
    auto attempt6 = auth.authenticate("admin", "wrong_attempt_6", attackerIp);
    report("Subsequent attempt from locked IP is rejected with lockout message",
           attempt6.success == false && attempt6.errorMessage.find("locked") != std::string::npos);

    // Even correct password from locked IP must be rejected while lockout is active
    auto attemptWithCorrectPass = auth.authenticate("admin", "admin123", attackerIp);
    report("Locked IP cannot login even with valid password during lockout window",
           attemptWithCorrectPass.success == false && attemptWithCorrectPass.errorMessage.find("locked") != std::string::npos);

    // Another IP should NOT be affected by the lockout
    std::string innocentIp = "10.0.0.50";
    auto innocentLogin = auth.authenticate("admin", "admin123", innocentIp);
    report("Unrelated IP can login successfully (lockout is IP-scoped)",
           innocentLogin.success == true && !innocentLogin.sessionToken.empty());

    // Clean up
    if (std::filesystem::exists(testAuthFile)) {
        std::filesystem::remove(testAuthFile);
    }
}

// ============================================================================
// 4. BUSINESS LOGIC & DATA INTEGRITY TESTS
// ============================================================================
void testBusinessLogic() {
    std::cout << "\n=== [TEST GROUP 4] Business Logic, Billing & Inventory Integration ===\n";

    std::string testInvFile = "data/test_inv.json";
    std::string testSalesFile = "data/test_sales.json";

    // Create fresh test inventory
    {
        std::ofstream invOut(testInvFile);
        invOut << R"([
            {"id": 1, "name": "Test Wireless Mouse", "price": 25.50, "stock": 10},
            {"id": 2, "name": "Test Mechanical Keyboard", "price": 75.00, "stock": 3}
        ])";
    }
    {
        std::ofstream salesOut(testSalesFile);
        salesOut << "[]";
    }

    BillingSystem billing(testInvFile, testSalesFile);

    // Verify initial stock
    auto p1 = billing.getInventory().findProductById(1);
    auto p2 = billing.getInventory().findProductById(2);
    report("Initial inventory loaded properly",
           p1 != nullptr && p1->getStockQuantity() == 10 && p2 != nullptr && p2->getStockQuantity() == 3);

    // Test Checkout: purchase 2 units of product 1 and 1 unit of product 2
    std::vector<BillingSystem::CartItemRequest> cart = {
        {1, 2}, // 2 * $25.50 = $51.00
        {2, 1}  // 1 * $75.00 = $75.00
    };          // Subtotal = $126.00, Tax (8%) = $10.08, Grand Total = $136.08

    auto checkoutResult = billing.processCheckout("Alice Customer", "Credit Card", cart, true);
    report("Checkout processes successfully with valid stock and confirmed payment",
           checkoutResult.success == true && checkoutResult.bill.isPaid() == true);

    report("Checkout captures specific customer name 'Alice Customer'",
           checkoutResult.bill.getCustomerName() == "Alice Customer",
           "Got: " + checkoutResult.bill.getCustomerName());

    report("Checkout records complete timestamp (YYYY-MM-DD HH:MM:SS)",
           checkoutResult.bill.getTimestamp().length() == 19 && checkoutResult.bill.getTimestamp()[10] == ' ',
           "Got: " + checkoutResult.bill.getTimestamp());

    report("Bill total calculated correctly ($126.00)",
           std::abs(checkoutResult.bill.getTotalAmount() - 126.00) < 0.01,
           "Total: " + std::to_string(checkoutResult.bill.getTotalAmount()));

    // Verify stock is deducted
    p1 = billing.getInventory().findProductById(1);
    p2 = billing.getInventory().findProductById(2);
    report("Product 1 stock decremented from 10 to 8",
           p1->getStockQuantity() == 8, "Got: " + std::to_string(p1->getStockQuantity()));
    report("Product 2 stock decremented from 3 to 2",
           p2->getStockQuantity() == 2, "Got: " + std::to_string(p2->getStockQuantity()));

    // Verify sale added to sales history
    report("Sales history contains recorded bill",
           billing.getSalesHistory().size() == 1);

    // Test checkout with empty customer name defaults to Walk-in Customer
    std::vector<BillingSystem::CartItemRequest> walkinCart = {{1, 1}};
    auto walkinResult = billing.processCheckout("", "Cash", walkinCart, true);
    report("Checkout with empty customer name defaults to 'Walk-in Customer'",
           walkinResult.success && walkinResult.bill.getCustomerName() == "Walk-in Customer",
           "Got: " + walkinResult.bill.getCustomerName());

    // Test checkout with complex customer name (commas, quotes)
    std::vector<BillingSystem::CartItemRequest> complexCart = {{1, 1}};
    auto complexResult = billing.processCheckout("Smith, John \"The Boss\"", "Card", complexCart, true);
    report("Checkout captures complex customer name with commas and quotes",
           complexResult.success && complexResult.bill.getCustomerName() == "Smith, John \"The Boss\"",
           "Got: " + complexResult.bill.getCustomerName());

    // Verify each transaction retains its own distinct customer name
    const auto& history = billing.getSalesHistory();
    report("Each sales transaction retains its own distinct customer name",
           history.size() == 3 &&
           history[0].getCustomerName() == "Alice Customer" &&
           history[1].getCustomerName() == "Walk-in Customer" &&
           history[2].getCustomerName() == "Smith, John \"The Boss\"");

    // Test stock deduction protection: Request more items than available in stock
    std::vector<BillingSystem::CartItemRequest> excessiveCart = {
        {2, 10} // only 2 left in stock
    };
    auto overStockResult = billing.processCheckout("Bob", "Cash", excessiveCart, true);
    report("Purchase exceeding stock is rejected",
           overStockResult.success == false);
    report("Stock is NOT deducted when checkout fails",
           billing.getInventory().findProductById(2)->getStockQuantity() == 2);

    // Test unconfirmed payment: Inventory should NOT be deducted
    int p1StockBefore = billing.getInventory().findProductById(1)->getStockQuantity();
    std::vector<BillingSystem::CartItemRequest> unconfirmedCart = {
        {1, 1}
    };
    auto unconfirmedResult = billing.processCheckout("Charlie", "Cash", unconfirmedCart, false);
    report("Unconfirmed payment does NOT deduct stock",
           billing.getInventory().findProductById(1)->getStockQuantity() == p1StockBefore);

    // Clean up
    if (std::filesystem::exists(testInvFile)) std::filesystem::remove(testInvFile);
    if (std::filesystem::exists(testSalesFile)) std::filesystem::remove(testSalesFile);
}

// ============================================================================
// 5. CHANGE PASSWORD SECURITY TESTS
// ============================================================================
void testChangePassword() {
    std::cout << "\n=== [TEST GROUP 5] Change Password Security Tests ===\n";

    std::string testAuthFile = "data/auth_test_chgpass.json";
    if (std::filesystem::exists(testAuthFile)) {
        std::filesystem::remove(testAuthFile);
    }

    AuthManager auth(testAuthFile);

    // Initial login works with default password
    auto initLogin = auth.authenticate("admin", "admin123", "127.0.0.1");
    report("Initial login with default password succeeds", initLogin.success);
    std::string initialToken = initLogin.sessionToken;
    report("Session token is valid before password change", auth.isValidSession(initialToken));

    // 1. Incorrect current password rejected
    auto resWrongCurrent = auth.changePassword("wrongPass123", "NewSecretPass99#", "NewSecretPass99#");
    report("Incorrect current password rejected",
           !resWrongCurrent.success && resWrongCurrent.errorMessage.find("Current password is incorrect") != std::string::npos);

    // 2. Mismatched confirmation rejected
    auto resMismatch = auth.changePassword("admin123", "NewSecretPass99#", "DifferentPass99#");
    report("Mismatched new password confirmation rejected",
           !resMismatch.success && resMismatch.errorMessage.find("do not match") != std::string::npos);

    // 3. Weak password rejected (< 8 characters)
    auto resShort = auth.changePassword("admin123", "Short1!", "Short1!");
    report("Short password (< 8 chars) rejected",
           !resShort.success && resShort.errorMessage.find("at least 8 characters") != std::string::npos);

    // 4. Weak password rejected (no numbers or symbols, letters only)
    auto resLettersOnly = auth.changePassword("admin123", "AllLettersOnly", "AllLettersOnly");
    report("Letters-only password without numbers/symbols rejected",
           !resLettersOnly.success && resLettersOnly.errorMessage.find("number or special character") != std::string::npos);

    // 5. Identical to current password rejected
    auto resSame = auth.changePassword("admin123", "admin123", "admin123");
    report("Password identical to current password rejected",
           !resSame.success && resSame.errorMessage.find("different") != std::string::npos);

    // 6. Valid change password succeeds
    auto resValid = auth.changePassword("admin123", "NewSecurePass2026!", "NewSecurePass2026!");
    report("Valid password change succeeds",
           resValid.success && resValid.errorMessage.empty());

    // 7. Existing active sessions invalidated
    report("Existing session invalidated after password change",
           !auth.isValidSession(initialToken));

    // 8. Old password no longer works
    auto oldLogin = auth.authenticate("admin", "admin123", "127.0.0.1");
    report("Old password rejected after password change",
           !oldLogin.success);

    // 9. New password works
    auto newLogin = auth.authenticate("admin", "NewSecurePass2026!", "127.0.0.1");
    report("New password accepted after password change",
           newLogin.success && !newLogin.sessionToken.empty());

    // 10. Persistence verification: New instance loading file can login with new password
    {
        AuthManager reloadedAuth(testAuthFile);
        auto reloadLogin = reloadedAuth.authenticate("admin", "NewSecurePass2026!", "127.0.0.1");
        report("Persisted credentials load successfully in new AuthManager instance",
               reloadLogin.success);
    }

    // Cleanup
    if (std::filesystem::exists(testAuthFile)) {
        std::filesystem::remove(testAuthFile);
    }
}

// ============================================================================
// 6. CHANGE USERNAME TESTS
// ============================================================================
void testChangeUsername() {
    std::cout << "\n=== [TEST GROUP 6] Change Username Security & Persistence Tests ===\n";

    std::string testAuthFile = "data/auth_test_chguser.json";
    if (std::filesystem::exists(testAuthFile)) {
        std::filesystem::remove(testAuthFile);
    }

    AuthManager auth(testAuthFile);
    auto initialLogin = auth.authenticate("admin", "admin123", "127.0.0.1");
    std::string initialToken = initialLogin.sessionToken;

    report("Initial login before username change succeeds", initialLogin.success);
    report("Initial session token valid", auth.isValidSession(initialToken));
    report("Initial session username is 'admin'", auth.getSessionUsername(initialToken) == "admin");

    // 1. Wrong current password rejected
    auto resWrongPass = auth.changeUsername("wrongpass123", "new_manager");
    report("Incorrect password rejected for username change",
           !resWrongPass.success && resWrongPass.errorMessage.find("Current password is incorrect") != std::string::npos);

    // 2. Empty username rejected
    auto resEmpty = auth.changeUsername("admin123", "");
    report("Empty username rejected",
           !resEmpty.success && resEmpty.errorMessage.find("cannot be empty") != std::string::npos);

    // 3. Short username (< 3 chars) rejected
    auto resShort = auth.changeUsername("admin123", "ab");
    report("Short username (< 3 chars) rejected",
           !resShort.success && resShort.errorMessage.find("between 3 and 32") != std::string::npos);

    // 4. Invalid characters rejected
    auto resInvalidChar = auth.changeUsername("admin123", "user@name!");
    report("Username with illegal characters rejected",
           !resInvalidChar.success && resInvalidChar.errorMessage.find("only contain") != std::string::npos);

    // 5. Identical username rejected
    auto resSame = auth.changeUsername("admin123", "admin");
    report("Identical username rejected",
           !resSame.success && resSame.errorMessage.find("different") != std::string::npos);

    // 6. Valid username change succeeds
    auto resValid = auth.changeUsername("admin123", "super_admin");
    report("Valid username change succeeds",
           resValid.success && resValid.errorMessage.empty());

    // 7. Active session retains validity and username updated
    report("Active session remains valid after username change",
           auth.isValidSession(initialToken));
    report("Active session now returns new username 'super_admin'",
           auth.getSessionUsername(initialToken) == "super_admin");

    // 8. Old username can no longer log in
    auto oldLogin = auth.authenticate("admin", "admin123", "127.0.0.1");
    report("Login with old username rejected", !oldLogin.success);

    // 9. New username logs in with same password
    auto newLogin = auth.authenticate("super_admin", "admin123", "127.0.0.1");
    report("Login with new username and preserved password succeeds",
           newLogin.success && !newLogin.sessionToken.empty());

    // 10. Persistence across new AuthManager instance
    {
        AuthManager reloaded(testAuthFile);
        report("Reloaded AuthManager loads updated username from storage",
               reloaded.getAdminUsername() == "super_admin");
        auto reloadLogin = reloaded.authenticate("super_admin", "admin123", "127.0.0.1");
        report("Reloaded AuthManager authenticates new username successfully",
               reloadLogin.success);
    }

    // Cleanup
    if (std::filesystem::exists(testAuthFile)) {
        std::filesystem::remove(testAuthFile);
    }
}

int main() {
    std::cout << "========================================================\n";
    std::cout << " INVENTORY MANAGEMENT SYSTEM - AUTOMATED UNIT TEST RUNNER\n";
    std::cout << "========================================================\n";

    testCryptoUtils();
    testAuthManager();
    testBruteForceLockout();
    testBusinessLogic();
    testChangePassword();
    testChangeUsername();

    std::cout << "\n========================================================\n";
    std::cout << " SUMMARY: " << g_passedTests << " PASSED, " << g_failedTests << " FAILED\n";
    std::cout << "========================================================\n";

    return (g_failedTests == 0) ? 0 : 1;
}
