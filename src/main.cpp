#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include "crow.h"
#include "BillingSystem.h"
#include "AuthManager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>

// Helper to attach standard CORS headers to responses
// Helper function to attach standard-compliant CORS headers when an Origin is present
void addCorsHeaders(const crow::request& req, crow::response& res) {
    std::string origin = req.get_header_value("Origin");
    if (!origin.empty()) {
        // Echo the exact requesting origin and allow credentials (RFC-6454 / W3C CORS compliant)
        res.add_header("Access-Control-Allow-Origin", origin);
        res.add_header("Access-Control-Allow-Credentials", "true");
        res.add_header("Vary", "Origin");
    }
    res.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization, Cookie");
}

// Helper function for C++17 string suffix check
static bool endsWith(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

// Helper to serve static frontend files with correct MIME types and path validation
void serveStaticFile(crow::response& res, const std::string& filepath) {
    std::filesystem::path p = std::filesystem::weakly_canonical(filepath);
    std::filesystem::path publicBase = std::filesystem::weakly_canonical("public");
    auto [rootEnd, nothing] = std::mismatch(publicBase.begin(), publicBase.end(), p.begin(), p.end());
    if (rootEnd != publicBase.end() || !std::filesystem::exists(p) || std::filesystem::is_directory(p)) {
        res.code = 404;
        res.write("404 Not Found");
        res.end();
        return;
    }
    std::ifstream file(p.string(), std::ios::binary);
    if (!file.is_open()) {
        res.code = 404;
        res.write("404 Not Found");
        res.end();
        return;
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    res.code = 200;
    if (endsWith(filepath, ".html")) res.set_header("Content-Type", "text/html; charset=utf-8");
    else if (endsWith(filepath, ".css")) res.set_header("Content-Type", "text/css; charset=utf-8");
    else if (endsWith(filepath, ".js")) res.set_header("Content-Type", "application/javascript; charset=utf-8");
    else if (endsWith(filepath, ".json")) res.set_header("Content-Type", "application/json; charset=utf-8");
    else if (endsWith(filepath, ".svg")) res.set_header("Content-Type", "image/svg+xml");
    else if (endsWith(filepath, ".png")) res.set_header("Content-Type", "image/png");
    res.write(content);
    res.end();
}

// Extracts session token from either Cookie or Authorization header
std::string extractSessionToken(const crow::request& req) {
    // 1. Try Cookie header: session_id=<token>
    std::string cookieHeader = req.get_header_value("Cookie");
    if (!cookieHeader.empty()) {
        size_t pos = cookieHeader.find("session_id=");
        if (pos != std::string::npos) {
            size_t start = pos + 11;
            size_t end = cookieHeader.find(";", start);
            if (end == std::string::npos) end = cookieHeader.length();
            std::string token = cookieHeader.substr(start, end - start);
            while (!token.empty() && (token.front() == ' ' || token.front() == '\t')) token.erase(0, 1);
            while (!token.empty() && (token.back() == ' ' || token.back() == '\t')) token.pop_back();
            if (!token.empty()) return token;
        }
    }

    // 2. Try Authorization: Bearer <token>
    std::string authHeader = req.get_header_value("Authorization");
    if (!authHeader.empty() && authHeader.rfind("Bearer ", 0) == 0) {
        std::string token = authHeader.substr(7);
        while (!token.empty() && (token.front() == ' ' || token.front() == '\t')) token.erase(0, 1);
        while (!token.empty() && (token.back() == ' ' || token.back() == '\t')) token.pop_back();
        if (!token.empty()) return token;
    }

    return "";
}

// Server-side authentication guard for protected API endpoints
bool requireAuth(const crow::request& req, crow::response& res, AuthManager& authManager) {
    std::string token = extractSessionToken(req);
    if (token.empty() || !authManager.isValidSession(token)) {
        addCorsHeaders(req, res);
        res.code = 401;
        res.set_header("Content-Type", "application/json");
        res.write("{\"error\": \"Unauthorized. Please sign in to access this resource.\", \"authenticated\": false}");
        res.end();
        return false;
    }
    return true;
}

int main(int argc, char* argv[]) {
    int port = 8080;
    const char* envPort = std::getenv("PORT");
    if (envPort && std::strlen(envPort) > 0) {
        try {
            port = std::stoi(envPort);
        } catch (...) {
            std::cerr << "[Server] Warning: Invalid PORT environment variable: " << envPort << ". Using default.\n";
        }
    }
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            std::cerr << "[Server] Warning: Invalid port argument: " << argv[1] << ". Using " << port << ".\n";
        }
    }

    std::string bindAddress = "0.0.0.0";
    const char* envHost = std::getenv("HOST");
    if (!envHost) envHost = std::getenv("BIND_ADDRESS");
    if (envHost && std::strlen(envHost) > 0) {
        bindAddress = envHost;
    }

    std::cout << "========================================================\n";
    std::cout << " Stock Flow - Inventory & Billing System Backend        \n";
    std::cout << " Subject: Object-Oriented Programming (OOP) with C++    \n";
    std::cout << " Framework: Crow C++ Microframework                     \n";
    std::cout << " Authentication: Salted SHA-256 + Server-Side Sessions  \n";
    std::cout << " Host: " << bindAddress << " | Port: " << port << "\n";
    std::cout << "========================================================\n";

    // Ensure runtime data directory exists
    std::filesystem::create_directories("data");

    // 1. Initialize central BillingSystem object (loads local JSON files)
    BillingSystem billingSystem("data/inventory.json", "data/sales.json");

    // 2. Initialize AuthManager (salted SHA-256 password hash + server sessions)
    AuthManager authManager("data/auth.json", 86400);

    crow::SimpleApp app;

    // --- Global OPTIONS Preflight Handler for CORS ---
    CROW_ROUTE(app, "/api/<path>")
        .methods(crow::HTTPMethod::Options)([](const crow::request& req, crow::response& res, std::string) {
            addCorsHeaders(req, res);
            res.code = 204;
            res.end();
        });

    // ========================================================================
    // AUTHENTICATION ENDPOINTS
    // ========================================================================

    // --- POST /api/auth/login ---
    // Authenticates username and password, generates secure session token, and sets HttpOnly cookie
    CROW_ROUTE(app, "/api/auth/login")
        .methods(crow::HTTPMethod::Post)([&authManager](const crow::request& req, crow::response& res) {
            addCorsHeaders(req, res);
            auto body = crow::json::load(req.body);
            if (!body || !body.has("username") || !body.has("password")) {
                res.code = 400;
                res.set_header("Content-Type", "application/json");
                res.write("{\"success\": false, \"message\": \"Username and password are required.\"}");
                res.end();
                return;
            }

            std::string username = std::string(body["username"].s());
            std::string password = std::string(body["password"].s());
            std::string clientIp = req.remote_ip_address.empty() ? "127.0.0.1" : req.remote_ip_address;

            auto result = authManager.authenticate(username, password, clientIp);

            crow::json::wvalue resp;
            resp["success"] = result.success;

            if (result.success) {
                // Set secure HttpOnly session cookie
                std::string cookieDirective = "session_id=" + result.sessionToken + "; Path=/; HttpOnly; SameSite=Lax";
                if (req.get_header_value("X-Forwarded-Proto") == "https") {
                    cookieDirective += "; Secure";
                }
                res.add_header("Set-Cookie", cookieDirective);

                resp["message"] = "Login successful.";
                resp["token"] = result.sessionToken; // Also provided for API clients/tests
                resp["user"]["username"] = username;
                resp["user"]["role"] = "Administrator";
                res.code = 200;
            } else {
                resp["message"] = result.errorMessage;
                res.code = 401;
            }

            res.set_header("Content-Type", "application/json");
            res.write(resp.dump());
            res.end();
        });

    // --- POST /api/auth/logout ---
    // Destroys active session server-side and invalidates the session cookie
    CROW_ROUTE(app, "/api/auth/logout")
        .methods(crow::HTTPMethod::Post)([&authManager](const crow::request& req, crow::response& res) {
            addCorsHeaders(req, res);
            std::string token = extractSessionToken(req);
            if (!token.empty()) {
                authManager.logout(token);
            }

            // Invalidate cookie
            res.add_header("Set-Cookie", "session_id=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0; Expires=Thu, 01 Jan 1970 00:00:00 GMT");

            crow::json::wvalue resp;
            resp["success"] = true;
            resp["message"] = "Logged out successfully.";

            res.code = 200;
            res.set_header("Content-Type", "application/json");
            res.write(resp.dump());
            res.end();
        });

    // --- GET /api/auth/me ---
    // Returns current session authentication status without exposing credentials
    CROW_ROUTE(app, "/api/auth/me")
        .methods(crow::HTTPMethod::Get)([&authManager](const crow::request& req, crow::response& res) {
            addCorsHeaders(req, res);
            std::string token = extractSessionToken(req);
            bool valid = authManager.isValidSession(token);

            crow::json::wvalue resp;
            resp["authenticated"] = valid;

            if (valid) {
                resp["user"]["username"] = authManager.getSessionUsername(token);
                resp["user"]["role"] = "Administrator";
            }
            res.code = 200;

            res.set_header("Content-Type", "application/json");
            res.write(resp.dump());
            res.end();
        });

    // --- POST /api/auth/change-password ---
    // Changes admin password, updates salted hash in storage, invalidates all active sessions
    CROW_ROUTE(app, "/api/auth/change-password")
        .methods(crow::HTTPMethod::Post)([&authManager](const crow::request& req, crow::response& res) {
            if (!requireAuth(req, res, authManager)) return;

            addCorsHeaders(req, res);
            auto body = crow::json::load(req.body);
            if (!body || !body.has("currentPassword") || !body.has("newPassword") || !body.has("confirmPassword")) {
                res.code = 400;
                res.set_header("Content-Type", "application/json");
                res.write("{\"success\": false, \"message\": \"Missing required password fields.\"}");
                res.end();
                return;
            }

            std::string currentPass = std::string(body["currentPassword"].s());
            std::string newPass = std::string(body["newPassword"].s());
            std::string confirmPass = std::string(body["confirmPassword"].s());

            auto result = authManager.changePassword(currentPass, newPass, confirmPass);

            crow::json::wvalue resp;
            resp["success"] = result.success;
            resp["message"] = result.success 
                ? "Password changed successfully. Please sign in again with your new password."
                : result.errorMessage;

            if (result.success) {
                // Clear session cookie since all active sessions are invalidated
                res.add_header("Set-Cookie", "session_id=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
                res.code = 200;
            } else {
                res.code = 400;
            }

            res.set_header("Content-Type", "application/json");
            res.write(resp.dump());
            res.end();
        });

    // --- POST /api/auth/change-username ---
    // Changes admin username after validating current password, preserving password hash & active sessions
    CROW_ROUTE(app, "/api/auth/change-username")
        .methods(crow::HTTPMethod::Post)([&authManager](const crow::request& req, crow::response& res) {
            if (!requireAuth(req, res, authManager)) return;

            addCorsHeaders(req, res);
            auto body = crow::json::load(req.body);
            if (!body || !body.has("currentPassword") || !body.has("newUsername")) {
                res.code = 400;
                res.set_header("Content-Type", "application/json");
                res.write("{\"success\": false, \"message\": \"Missing required fields: currentPassword and newUsername.\"}");
                res.end();
                return;
            }

            std::string currentPass = std::string(body["currentPassword"].s());
            std::string newUsername = std::string(body["newUsername"].s());
            std::string token = extractSessionToken(req);

            auto result = authManager.changeUsername(currentPass, newUsername, token);

            crow::json::wvalue resp;
            resp["success"] = result.success;
            resp["message"] = result.success
                ? "Username updated successfully."
                : result.errorMessage;

            if (result.success) {
                resp["user"]["username"] = newUsername;
                resp["user"]["role"] = "Administrator";
                res.code = 200;
            } else {
                res.code = 400;
            }

            res.set_header("Content-Type", "application/json");
            res.write(resp.dump());
            res.end();
        });

    // ========================================================================
    // PROTECTED API ENDPOINTS (Require Active Admin Session)
    // ========================================================================

    // --- ENDPOINT 1: GET /api/products ---
    CROW_ROUTE(app, "/api/products")
        .methods(crow::HTTPMethod::Get)([&billingSystem, &authManager](const crow::request& req, crow::response& res) {
            if (!requireAuth(req, res, authManager)) return;

            addCorsHeaders(req, res);
            crow::json::wvalue productList = billingSystem.getInventory().toJson();
            res.code = 200;
            res.set_header("Content-Type", "application/json");
            res.write(productList.dump());
            res.end();
        });

    // --- ENDPOINT 2: POST /api/products ---
    CROW_ROUTE(app, "/api/products")
        .methods(crow::HTTPMethod::Post)([&billingSystem, &authManager](const crow::request& req, crow::response& res) {
            if (!requireAuth(req, res, authManager)) return;

            addCorsHeaders(req, res);
            auto body = crow::json::load(req.body);
            if (!body) {
                res.code = 400;
                res.set_header("Content-Type", "application/json");
                res.write("{\"error\": \"Invalid JSON body\"}");
                res.end();
                return;
            }

            if (!body.has("name") || !body.has("price") || !body.has("stock")) {
                res.code = 400;
                res.set_header("Content-Type", "application/json");
                res.write("{\"error\": \"Missing required fields: name, price, stock\"}");
                res.end();
                return;
            }

            std::string name = std::string(body["name"].s());
            double price = body["price"].d();
            int stock = body["stock"].i();

            if (price < 0.0 || stock < 0) {
                res.code = 400;
                res.set_header("Content-Type", "application/json");
                res.write("{\"error\": \"Price and stock quantity cannot be negative\"}");
                res.end();
                return;
            }

            // Check if editing existing product
            if (body.has("id") && body["id"].i() > 0) {
                int id = static_cast<int>(body["id"].i());
                bool updated = billingSystem.getInventory().updateProduct(id, name, price, stock);
                if (updated) {
                    billingSystem.saveInventory();
                    crow::json::wvalue resp;
                    resp["success"] = true;
                    resp["message"] = "Product updated successfully";
                    resp["product"] = billingSystem.getInventory().findProductById(id)->toJson();
                    res.code = 200;
                    res.set_header("Content-Type", "application/json");
                    res.write(resp.dump());
                    res.end();
                    return;
                }
            }

            // Otherwise, add new product
            int newId = billingSystem.getInventory().addProduct(name, price, stock);
            billingSystem.saveInventory();

            crow::json::wvalue resp;
            resp["success"] = true;
            resp["message"] = "Product added successfully";
            resp["product"] = billingSystem.getInventory().findProductById(newId)->toJson();

            res.code = 201;
            res.set_header("Content-Type", "application/json");
            res.write(resp.dump());
            res.end();
        });

    // --- ENDPOINT: PUT /api/products/<int> ---
    CROW_ROUTE(app, "/api/products/<int>")
        .methods(crow::HTTPMethod::Put)([&billingSystem, &authManager](const crow::request& req, crow::response& res, int id) {
            if (!requireAuth(req, res, authManager)) return;

            addCorsHeaders(req, res);
            auto body = crow::json::load(req.body);
            if (!body || !body.has("name") || !body.has("price") || !body.has("stock")) {
                res.code = 400;
                res.set_header("Content-Type", "application/json");
                res.write("{\"error\": \"Missing required fields: name, price, stock\"}");
                res.end();
                return;
            }

            std::string name = std::string(body["name"].s());
            double price = body["price"].d();
            int stock = body["stock"].i();

            bool updated = billingSystem.getInventory().updateProduct(id, name, price, stock);
            if (updated) {
                billingSystem.saveInventory();
                crow::json::wvalue resp;
                resp["success"] = true;
                resp["message"] = "Product updated successfully";
                resp["product"] = billingSystem.getInventory().findProductById(id)->toJson();
                res.code = 200;
                res.set_header("Content-Type", "application/json");
                res.write(resp.dump());
            } else {
                res.code = 404;
                res.set_header("Content-Type", "application/json");
                res.write("{\"error\": \"Product not found\"}");
            }
            res.end();
        });

    // --- ENDPOINT: DELETE /api/products/<int> ---
    CROW_ROUTE(app, "/api/products/<int>")
        .methods(crow::HTTPMethod::Delete)([&billingSystem, &authManager](const crow::request& req, crow::response& res, int id) {
            if (!requireAuth(req, res, authManager)) return;

            addCorsHeaders(req, res);
            bool deleted = billingSystem.getInventory().deleteProduct(id);
            if (deleted) {
                billingSystem.saveInventory();
                res.code = 200;
                res.set_header("Content-Type", "application/json");
                res.write("{\"success\": true, \"message\": \"Product deleted successfully\"}");
            } else {
                res.code = 404;
                res.set_header("Content-Type", "application/json");
                res.write("{\"error\": \"Product not found\"}");
            }
            res.end();
        });

    // --- ENDPOINT 3: POST /api/checkout ---
    CROW_ROUTE(app, "/api/checkout")
        .methods(crow::HTTPMethod::Post)([&billingSystem, &authManager](const crow::request& req, crow::response& res) {
            if (!requireAuth(req, res, authManager)) return;

            addCorsHeaders(req, res);
            auto body = crow::json::load(req.body);
            if (!body) {
                res.code = 400;
                res.set_header("Content-Type", "application/json");
                res.write("{\"success\": false, \"message\": \"Invalid JSON payload.\"}");
                res.end();
                return;
            }

            // Helper lambda to trim leading and trailing whitespace
            auto trimString = [](const std::string& str) -> std::string {
                size_t first = str.find_first_not_of(" \t\r\n");
                if (first == std::string::npos) return "";
                size_t last = str.find_last_not_of(" \t\r\n");
                return str.substr(first, (last - first + 1));
            };

            std::string customerName = "Walk-in Customer";
            if (body.has("customerName")) {
                std::string rawName = trimString(std::string(body["customerName"].s()));
                if (!rawName.empty()) {
                    if (rawName.length() > 100) {
                        res.code = 400;
                        res.set_header("Content-Type", "application/json");
                        res.write("{\"success\": false, \"message\": \"Customer name is too long (maximum 100 characters).\"}");
                        res.end();
                        return;
                    }
                    // Validate: disallow control characters that could corrupt data
                    bool hasControl = false;
                    for (unsigned char c : rawName) {
                        if (c < 32 && c != '\t') {
                            hasControl = true;
                            break;
                        }
                    }
                    if (hasControl) {
                        res.code = 400;
                        res.set_header("Content-Type", "application/json");
                        res.write("{\"success\": false, \"message\": \"Customer name contains invalid characters.\"}");
                        res.end();
                        return;
                    }
                    customerName = rawName;
                }
            }

            std::string paymentMethod = body.has("paymentMethod") ? std::string(body["paymentMethod"].s()) : "Cash";
            bool paymentConfirmed = body.has("paymentConfirmed") ? body["paymentConfirmed"].b() : false;

            if (!body.has("items") || body["items"].t() != crow::json::type::List) {
                res.code = 400;
                res.set_header("Content-Type", "application/json");
                res.write("{\"success\": false, \"message\": \"Missing or invalid 'items' list in request.\"}");
                res.end();
                return;
            }

            std::vector<BillingSystem::CartItemRequest> cart;
            for (const auto& item : body["items"]) {
                if (!item.has("productId") || !item.has("quantity")) {
                    res.code = 400;
                    res.set_header("Content-Type", "application/json");
                    res.write("{\"success\": false, \"message\": \"Cart items must include 'productId' and 'quantity'.\"}");
                    res.end();
                    return;
                }
                cart.push_back({
                    static_cast<int>(item["productId"].i()),
                    static_cast<int>(item["quantity"].i())
                });
            }

            auto result = billingSystem.processCheckout(customerName, paymentMethod, cart, paymentConfirmed);

            crow::json::wvalue responseJson;
            responseJson["success"] = result.success;
            responseJson["message"] = result.message;

            if (result.success) {
                responseJson["bill"] = result.bill.toJson();
                res.code = 200;
            } else {
                res.code = 400;
            }

            res.set_header("Content-Type", "application/json");
            res.write(responseJson.dump());
            res.end();
        });

    // --- ENDPOINT 4: GET /api/sales ---
    CROW_ROUTE(app, "/api/sales")
        .methods(crow::HTTPMethod::Get)([&billingSystem, &authManager](const crow::request& req, crow::response& res) {
            if (!requireAuth(req, res, authManager)) return;

            addCorsHeaders(req, res);
            crow::json::wvalue salesJson = billingSystem.getSalesHistoryJson();
            res.code = 200;
            res.set_header("Content-Type", "application/json");
            res.write(salesJson.dump());
            res.end();
        });

    // --- ENDPOINT 5: GET /api/sales/export-csv ---
    // Exports completed sales transactions as an RFC-4180 compliant CSV file for authenticated admin
    CROW_ROUTE(app, "/api/sales/export-csv")
        .methods(crow::HTTPMethod::Get)([&billingSystem, &authManager](const crow::request& req, crow::response& res) {
            if (!requireAuth(req, res, authManager)) return;

            addCorsHeaders(req, res);
            
            // Helper lambda for CSV cell escaping (RFC-4180 standard)
            auto escapeCsvCell = [](const std::string& input) -> std::string {
                bool needsQuotes = false;
                if (input.find(',') != std::string::npos ||
                    input.find('"') != std::string::npos ||
                    input.find('\n') != std::string::npos ||
                    input.find('\r') != std::string::npos) {
                    needsQuotes = true;
                }
                std::string escaped = "";
                for (char c : input) {
                    if (c == '"') escaped += "\"\"";
                    else escaped += c;
                }
                if (needsQuotes) {
                    return "\"" + escaped + "\"";
                }
                return escaped;
            };

            std::stringstream csv;
            // Prepend UTF-8 Byte Order Mark (BOM) so Microsoft Excel for Windows opens UTF-8 text cleanly
            csv << "\xEF\xBB\xBF";
            // CSV Column Headers
            csv << "Bill ID,Date & Time,Customer Name,Payment Method,Purchased Items,Total Amount (INR),Payment Status\r\n";

            const auto& sales = billingSystem.getSalesHistory();
            for (const auto& bill : sales) {
                std::string billId = bill.getBillId();
                std::string date = bill.getTimestamp();
                // Ensure complete timestamp representation without altering historical date/time
                if (date.empty()) {
                    date = "N/A";
                } else if (date.length() == 16 && date[4] == '-' && date[7] == '-' && date[10] == ' ' && date[13] == ':') {
                    // Standardize legacy "YYYY-MM-DD HH:MM" to "YYYY-MM-DD HH:MM:00"
                    date += ":00";
                }

                std::string customer = bill.getCustomerName();
                if (customer.empty()) {
                    customer = "Walk-in Customer";
                }

                std::string method = bill.getPaymentMethod();
                if (method.empty()) {
                    method = "Cash";
                }
                
                // Format purchased items summary: e.g., "Chips (x2); Coffee (x1)"
                std::stringstream itemsSummary;
                const auto& items = bill.getItems();
                for (size_t i = 0; i < items.size(); ++i) {
                    itemsSummary << items[i].productName << " (x" << items[i].quantity << ")";
                    if (i + 1 < items.size()) {
                        itemsSummary << "; ";
                    }
                }

                // Format amount with 2 decimal places
                char amountBuf[64];
                std::snprintf(amountBuf, sizeof(amountBuf), "%.2f", bill.getTotalAmount());

                std::string status = bill.isPaid() ? "Paid" : "Pending";

                csv << escapeCsvCell(billId) << ","
                    << escapeCsvCell(date) << ","
                    << escapeCsvCell(customer) << ","
                    << escapeCsvCell(method) << ","
                    << escapeCsvCell(itemsSummary.str()) << ","
                    << escapeCsvCell(amountBuf) << ","
                    << escapeCsvCell(status) << "\r\n";
            }

            res.code = 200;
            res.set_header("Content-Type", "text/csv; charset=utf-8");
            
            // Format current date: stock-flow-sales-YYYY-MM-DD.csv
            auto now = std::chrono::system_clock::now();
            std::time_t tt = std::chrono::system_clock::to_time_t(now);
            std::tm tm{};
            #if defined(_WIN32)
            localtime_s(&tm, &tt);
            #else
            localtime_r(&tt, &tm);
            #endif
            char dateBuf[32];
            std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &tm);
            std::string filename = "stock-flow-sales-" + std::string(dateBuf) + ".csv";
            res.set_header("Content-Disposition", "attachment; filename=\"" + filename + "\"");

            res.write(csv.str());
            res.end();
        });

    // --- Health Check / Ping (Public) ---
    CROW_ROUTE(app, "/api/health")
        .methods(crow::HTTPMethod::Get)([](const crow::request& req, crow::response& res) {
            addCorsHeaders(req, res);
            res.code = 200;
            res.set_header("Content-Type", "application/json");
            res.write("{\"status\": \"healthy\", \"server\": \"Crow C++ Backend\"}");
            res.end();
        });

    // --- STATIC FRONTEND ROUTES (Public) ---
    CROW_ROUTE(app, "/")
        ([](const crow::request&, crow::response& res) {
            serveStaticFile(res, "public/index.html");
        });

    CROW_ROUTE(app, "/<string>")
        ([](const crow::request&, crow::response& res, std::string filename) {
            if (filename.empty() || 
                filename.find("..") != std::string::npos ||
                filename.find('/') != std::string::npos ||
                filename.find('\\') != std::string::npos ||
                filename.front() == '.') {
                serveStaticFile(res, "public/index.html");
                return;
            }
            std::filesystem::path basePath = std::filesystem::weakly_canonical("public");
            std::filesystem::path targetPath = std::filesystem::weakly_canonical(basePath / filename);
            auto [rootEnd, nothing] = std::mismatch(basePath.begin(), basePath.end(), targetPath.begin(), targetPath.end());
            if (rootEnd == basePath.end() && std::filesystem::exists(targetPath) && !std::filesystem::is_directory(targetPath)) {
                serveStaticFile(res, targetPath.string());
            } else {
                serveStaticFile(res, "public/index.html");
            }
        });

    std::cout << "[Server] Listening on http://" << bindAddress << ":" << port << "\n";
    app.bindaddr(bindAddress).port(port).multithreaded().run();

    return 0;
}
