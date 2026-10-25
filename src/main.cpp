#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include "crow.h"
#include "BillingSystem.h"
#include <iostream>
#include <fstream>
#include <filesystem>

// Helper to attach standard CORS headers to responses
void addCorsHeaders(crow::response& res) {
    res.add_header("Access-Control-Allow-Origin", "*");
    res.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

// Helper function for C++17 string suffix check
static bool endsWith(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

// Helper to serve static frontend files with correct MIME types
void serveStaticFile(crow::response& res, const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
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

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc > 1) {
        port = std::stoi(argv[1]);
    }

    std::cout << "========================================================\n";
    std::cout << " Inventory Management & Billing System - C++ Backend    \n";
    std::cout << " Subject: Object-Oriented Programming (OOP) with C++    \n";
    std::cout << " Framework: Crow C++ Microframework                     \n";
    std::cout << " Port: " << port << "\n";
    std::cout << "========================================================\n";

    // Instantiate central BillingSystem object (loads local JSON files)
    BillingSystem billingSystem("data/inventory.json", "data/sales.json");

    crow::SimpleApp app;

    // --- Global OPTIONS Preflight Handler for CORS ---
    CROW_ROUTE(app, "/api/<path>")
        .methods(crow::HTTPMethod::Options)([](const crow::request&, crow::response& res, std::string) {
            addCorsHeaders(res);
            res.code = 204;
            res.end();
        });

    // --- ENDPOINT 1: GET /api/products ---
    // Returns the complete product catalog and current stock
    CROW_ROUTE(app, "/api/products")
        .methods(crow::HTTPMethod::Get)([&billingSystem](const crow::request&, crow::response& res) {
            addCorsHeaders(res);
            crow::json::wvalue productList = billingSystem.getInventory().toJson();
            res.code = 200;
            res.set_header("Content-Type", "application/json");
            res.write(productList.dump());
            res.end();
        });

    // --- ENDPOINT 2: POST /api/products ---
    // Adds a new product or updates an existing product
    CROW_ROUTE(app, "/api/products")
        .methods(crow::HTTPMethod::Post)([&billingSystem](const crow::request& req, crow::response& res) {
            addCorsHeaders(res);
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
    // Updates an existing product
    CROW_ROUTE(app, "/api/products/<int>")
        .methods(crow::HTTPMethod::Put)([&billingSystem](const crow::request& req, crow::response& res, int id) {
            addCorsHeaders(res);
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
    // Deletes a product from the catalog
    CROW_ROUTE(app, "/api/products/<int>")
        .methods(crow::HTTPMethod::Delete)([&billingSystem](const crow::request&, crow::response& res, int id) {
            addCorsHeaders(res);
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
    // Validates stock, processes payment confirmation, deducts inventory atomically,
    // generates the invoice, and stores sales history
    CROW_ROUTE(app, "/api/checkout")
        .methods(crow::HTTPMethod::Post)([&billingSystem](const crow::request& req, crow::response& res) {
            addCorsHeaders(res);
            auto body = crow::json::load(req.body);
            if (!body) {
                res.code = 400;
                res.set_header("Content-Type", "application/json");
                res.write("{\"success\": false, \"message\": \"Invalid JSON payload.\"}");
                res.end();
                return;
            }

            std::string customerName = body.has("customerName") ? std::string(body["customerName"].s()) : "Walk-in Customer";
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

            // Execute transactional checkout inside BillingSystem
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
    // Returns full history of all completed transactions
    CROW_ROUTE(app, "/api/sales")
        .methods(crow::HTTPMethod::Get)([&billingSystem](const crow::request&, crow::response& res) {
            addCorsHeaders(res);
            crow::json::wvalue salesJson = billingSystem.getSalesHistoryJson();
            res.code = 200;
            res.set_header("Content-Type", "application/json");
            res.write(salesJson.dump());
            res.end();
        });

    // --- Health Check / Ping ---
    CROW_ROUTE(app, "/api/health")
        .methods(crow::HTTPMethod::Get)([](const crow::request&, crow::response& res) {
            addCorsHeaders(res);
            res.code = 200;
            res.set_header("Content-Type", "application/json");
            res.write("{\"status\": \"healthy\", \"server\": \"Crow C++ Backend\"}");
            res.end();
        });

    // --- STATIC FRONTEND ROUTES ---
    CROW_ROUTE(app, "/")
        ([](const crow::request&, crow::response& res) {
            serveStaticFile(res, "public/index.html");
        });

    CROW_ROUTE(app, "/<string>")
        ([](const crow::request&, crow::response& res, std::string filename) {
            std::string path = "public/" + filename;
            if (std::filesystem::exists(path) && !std::filesystem::is_directory(path)) {
                serveStaticFile(res, path);
            } else {
                // SPA fallback for /pos, /products, /sales, /dashboard, etc.
                serveStaticFile(res, "public/index.html");
            }
        });

    std::cout << "[Server] Listening on http://127.0.0.1:" << port << "\n";
    app.port(port).multithreaded().run();

    return 0;
}
