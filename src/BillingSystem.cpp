#include "BillingSystem.h"
#include <fstream>
#include <sstream>
#include <iostream>

BillingSystem::BillingSystem(const std::string& inventoryFilePath, const std::string& salesFilePath)
    : inventoryFilePath_(inventoryFilePath), salesFilePath_(salesFilePath), nextBillSequence_(1001) {
    loadAllData();
}

std::string BillingSystem::generateBillId() {
    return "INV-" + std::to_string(nextBillSequence_++);
}

bool BillingSystem::loadAllData() {
    std::lock_guard<std::mutex> lock(systemMutex_);

    // 1. Load Inventory
    bool invLoaded = inventory_.loadFromFile(inventoryFilePath_);
    if (!invLoaded) {
        std::cout << "[BillingSystem] Could not load inventory from " << inventoryFilePath_
                  << ". Existing file will be used or defaults created." << std::endl;
    }

    // 2. Load Sales History
    std::ifstream file(salesFilePath_);
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();

        std::string content = buffer.str();
        if (!content.empty()) {
            auto json = crow::json::load(content);
            if (json && json.t() == crow::json::type::List) {
                salesHistory_.clear();
                for (const auto& item : json) {
                    salesHistory_.push_back(Bill::fromJson(item));
                }
                nextBillSequence_ = 1001 + static_cast<int>(salesHistory_.size());
            }
        }
    }

    return true;
}

bool BillingSystem::saveInventory() const {
    return inventory_.saveToFile(inventoryFilePath_);
}

bool BillingSystem::saveSalesHistory() const {
    std::lock_guard<std::mutex> lock(systemMutex_);
    std::ofstream file(salesFilePath_, std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "[BillingSystem] Error: Could not write sales history to " << salesFilePath_ << std::endl;
        return false;
    }

    std::vector<crow::json::wvalue> list;
    list.reserve(salesHistory_.size());
    for (const auto& bill : salesHistory_) {
        list.push_back(bill.toJson());
    }

    crow::json::wvalue root(list);
    file << root.dump();
    file.close();
    return true;
}

Inventory& BillingSystem::getInventory() {
    return inventory_;
}

const Inventory& BillingSystem::getInventory() const {
    return inventory_;
}

const std::vector<Bill>& BillingSystem::getSalesHistory() const {
    std::lock_guard<std::mutex> lock(systemMutex_);
    return salesHistory_;
}

BillingSystem::CheckoutResult BillingSystem::processCheckout(
    const std::string& customerName,
    const std::string& paymentMethod,
    const std::vector<CartItemRequest>& cartItems,
    bool paymentConfirmed) {

    std::lock_guard<std::mutex> lock(systemMutex_);

    CheckoutResult result;
    result.success = false;

    // Validation 1: Cart must not be empty
    if (cartItems.empty()) {
        result.message = "Checkout failed: Cart is empty.";
        return result;
    }

    // Validation 2: Pre-validate ALL requested products and their stocks before deducting anything.
    // This ensures atomic execution: either the entire sale succeeds or no inventory is changed.
    for (const auto& req : cartItems) {
        if (req.quantity <= 0) {
            result.message = "Checkout failed: Invalid quantity (" + std::to_string(req.quantity) + ") requested.";
            return result;
        }

        Product* product = inventory_.findProductById(req.productId);
        if (!product) {
            result.message = "Checkout failed: Product with ID " + std::to_string(req.productId) + " not found.";
            return result;
        }

        if (!product->hasSufficientStock(req.quantity)) {
            result.message = "Checkout failed: Insufficient stock for '" + product->getName() + 
                             "'. Requested: " + std::to_string(req.quantity) + 
                             ", Available: " + std::to_string(product->getStockQuantity()) + ".";
            return result;
        }
    }

    // Validation 3: Payment confirmation check.
    // Stock must ONLY be deducted after successful payment confirmation!
    if (!paymentConfirmed) {
        result.message = "Checkout failed: Payment was not confirmed. No stock was deducted.";
        return result;
    }

    // Step 4: All checks passed and payment is confirmed.
    // Now deduct inventory and compose the bill.
    std::string newBillId = generateBillId();
    Bill bill(newBillId, customerName, paymentMethod);

    for (const auto& req : cartItems) {
        Product* product = inventory_.findProductById(req.productId);
        // Deduct from inventory
        product->reduceStock(req.quantity);
        // Add to bill items
        bill.addItem(product->getId(), product->getName(), product->getPrice(), req.quantity);
    }

    // Calculate totals (0% tax for straightforward college project, or easily configurable)
    bill.calculateTotals(0.0);

    // Mark as paid
    bill.markPaid();

    // Append to sales history
    salesHistory_.push_back(bill);

    // Persist changes to disk immediately
    inventory_.saveToFile(inventoryFilePath_);
    
    // Save sales history (already under systemMutex_)
    std::ofstream file(salesFilePath_, std::ios::trunc);
    if (file.is_open()) {
        std::vector<crow::json::wvalue> list;
        list.reserve(salesHistory_.size());
        for (const auto& b : salesHistory_) {
            list.push_back(b.toJson());
        }
        crow::json::wvalue root(list);
        file << root.dump();
        file.close();
    }

    result.success = true;
    result.message = "Payment confirmed. Sale processed successfully.";
    result.bill = bill;
    return result;
}

crow::json::wvalue BillingSystem::getSalesHistoryJson() const {
    std::lock_guard<std::mutex> lock(systemMutex_);
    std::vector<crow::json::wvalue> list;
    list.reserve(salesHistory_.size());
    for (const auto& bill : salesHistory_) {
        list.push_back(bill.toJson());
    }
    return crow::json::wvalue(list);
}
