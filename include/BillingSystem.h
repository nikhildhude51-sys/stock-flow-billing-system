#ifndef BILLING_SYSTEM_H
#define BILLING_SYSTEM_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include "Inventory.h"
#include "Bill.h"
#include "crow/json.h"

/**
 * ============================================================================
 * CLASS: BillingSystem
 * OOP Concept: Composition, Facade & Business Orchestration
 * 
 * Description:
 * Coordinates the entire billing and checkout workflow:
 * 1. Coordinates with the Inventory class.
 * 2. Validates stock availability for every cart item.
 * 3. Enforces that stock is ONLY deducted after explicit payment confirmation.
 * 4. Ensures atomic transactions (no partial deductions if payment fails).
 * 5. Prevents duplicate payments and manages sales history persistence.
 * ============================================================================
 */
class BillingSystem {
public:
    // Result of a checkout attempt
    struct CheckoutResult {
        bool success;
        std::string message;
        Bill bill;
    };

    // Representation of a requested purchase item
    struct CartItemRequest {
        int productId;
        int quantity;
    };

private:
    Inventory inventory_;
    std::vector<Bill> salesHistory_;
    std::string inventoryFilePath_;
    std::string salesFilePath_;
    int nextBillSequence_;
    mutable std::mutex systemMutex_;

    // Generates a unique sequential Bill ID (e.g., "INV-1001")
    std::string generateBillId();

public:
    // Constructor initializes paths and loads persistent records
    BillingSystem(const std::string& inventoryFilePath = "data/inventory.json",
                  const std::string& salesFilePath = "data/sales.json");

    // --- Data Persistence ---
    bool loadAllData();
    bool saveInventory() const;
    bool saveSalesHistory() const;

    // --- Accessors ---
    Inventory& getInventory();
    const Inventory& getInventory() const;
    const std::vector<Bill>& getSalesHistory() const;

    // --- Core Billing Transaction Workflow ---
    // Processes checkout atomically. Stock is updated ONLY if paymentConfirmed is true
    // and all items have sufficient stock.
    CheckoutResult processCheckout(const std::string& customerName,
                                   const std::string& paymentMethod,
                                   const std::vector<CartItemRequest>& cartItems,
                                   bool paymentConfirmed);

    // Returns sales history serialized into Crow JSON
    crow::json::wvalue getSalesHistoryJson() const;
};

#endif // BILLING_SYSTEM_H
