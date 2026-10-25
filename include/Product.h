#ifndef PRODUCT_H
#define PRODUCT_H

#include <string>
#include "crow/json.h"

/**
 * ============================================================================
 * CLASS: Product
 * OOP Concept: Encapsulation & Data Hiding
 * 
 * Description:
 * Represents a single sellable product in the store catalog.
 * All attributes (id, name, price, stockQuantity) are declared private
 * to protect their state and maintain class invariants (e.g., price and
 * stock cannot be negative).
 * ============================================================================
 */
class Product {
private:
    int id_;
    std::string name_;
    double price_;
    int stockQuantity_;

public:
    // Default constructor (required for container allocations)
    Product();

    // Parameterized constructor
    Product(int id, const std::string& name, double price, int stockQuantity);

    // --- Getters (Encapsulation: Read-only access) ---
    int getId() const;
    std::string getName() const;
    double getPrice() const;
    int getStockQuantity() const;

    // --- Setters (Encapsulation: Validated state mutation) ---
    void setName(const std::string& name);
    bool setPrice(double price);
    bool setStockQuantity(int quantity);

    // --- Business Logic Methods ---
    // Checks if the product has at least 'quantity' units in stock
    bool hasSufficientStock(int quantity) const;

    // Decreases stock by 'quantity' units. Returns false if stock is insufficient.
    bool reduceStock(int quantity);

    // Restores stock by 'quantity' units (e.g., in case of cancellation)
    void addStock(int quantity);

    // Converts Product object to a Crow JSON object for API responses
    crow::json::wvalue toJson() const;

    // Factory method: Constructs a Product from a parsed JSON value
    static Product fromJson(const crow::json::rvalue& json);
};

#endif // PRODUCT_H
