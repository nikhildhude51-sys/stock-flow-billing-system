#ifndef INVENTORY_H
#define INVENTORY_H

#include <vector>
#include <map>
#include <string>
#include <mutex>
#include "Product.h"
#include "crow/json.h"

/**
 * ============================================================================
 * CLASS: Inventory
 * OOP Concept: Encapsulation & Collection Management
 * 
 * Description:
 * Manages the repository of all products in the store.
 * Responsible for adding, updating, searching, and persisting products
 * to/from the local JSON file.
 * ============================================================================
 */
class Inventory {
private:
    // Internal data store: Map product ID -> Product object
    std::map<int, Product> products_;
    
    // Auto-incrementing ID tracker
    int nextId_;

    // Mutex to protect concurrent access to inventory
    mutable std::mutex inventoryMutex_;

public:
    // Constructor initializes empty inventory
    Inventory();

    // --- Core Operations ---
    // Adds a new product to the inventory (auto-generates ID if id <= 0)
    int addProduct(const std::string& name, double price, int stock);

    // Updates an existing product's details
    bool updateProduct(int id, const std::string& name, double price, int stock);

    // Deletes a product from inventory
    bool deleteProduct(int id);

    // Finds a product pointer by ID (returns nullptr if not found)
    Product* findProductById(int id);
    const Product* findProductById(int id) const;

    // Returns a copy of all products currently in inventory
    std::vector<Product> getAllProducts() const;

    // Searches products by keyword (case-insensitive substring match)
    std::vector<Product> searchProducts(const std::string& keyword) const;

    // Checks if a product exists
    bool hasProduct(int id) const;

    // Total number of unique products
    size_t getProductCount() const;

    // --- Persistence (Local JSON File) ---
    // Loads inventory from a local JSON file path
    bool loadFromFile(const std::string& filepath);

    // Saves current inventory to a local JSON file path
    bool saveToFile(const std::string& filepath) const;

    // Serializes the entire inventory to a Crow JSON array
    crow::json::wvalue toJson() const;
};

#endif // INVENTORY_H
