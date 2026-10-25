#include "Inventory.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

// Helper function: lowercase conversion for case-insensitive search
static std::string toLower(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return lower;
}

Inventory::Inventory() : nextId_(1) {}

int Inventory::addProduct(const std::string& name, double price, int stock) {
    std::lock_guard<std::mutex> lock(inventoryMutex_);
    int newId = nextId_++;
    Product p(newId, name, price, stock);
    products_[newId] = p;
    return newId;
}

bool Inventory::updateProduct(int id, const std::string& name, double price, int stock) {
    std::lock_guard<std::mutex> lock(inventoryMutex_);
    auto it = products_.find(id);
    if (it != products_.end()) {
        it->second.setName(name);
        it->second.setPrice(price);
        it->second.setStockQuantity(stock);
        return true;
    }
    return false;
}

bool Inventory::deleteProduct(int id) {
    std::lock_guard<std::mutex> lock(inventoryMutex_);
    return products_.erase(id) > 0;
}

Product* Inventory::findProductById(int id) {
    std::lock_guard<std::mutex> lock(inventoryMutex_);
    auto it = products_.find(id);
    if (it != products_.end()) {
        return &(it->second);
    }
    return nullptr;
}

const Product* Inventory::findProductById(int id) const {
    std::lock_guard<std::mutex> lock(inventoryMutex_);
    auto it = products_.find(id);
    if (it != products_.end()) {
        return &(it->second);
    }
    return nullptr;
}

std::vector<Product> Inventory::getAllProducts() const {
    std::lock_guard<std::mutex> lock(inventoryMutex_);
    std::vector<Product> list;
    list.reserve(products_.size());
    for (const auto& pair : products_) {
        list.push_back(pair.second);
    }
    return list;
}

std::vector<Product> Inventory::searchProducts(const std::string& keyword) const {
    std::lock_guard<std::mutex> lock(inventoryMutex_);
    std::vector<Product> results;
    std::string lowerKeyword = toLower(keyword);

    for (const auto& pair : products_) {
        std::string lowerName = toLower(pair.second.getName());
        if (lowerName.find(lowerKeyword) != std::string::npos) {
            results.push_back(pair.second);
        }
    }
    return results;
}

bool Inventory::hasProduct(int id) const {
    std::lock_guard<std::mutex> lock(inventoryMutex_);
    return products_.find(id) != products_.end();
}

size_t Inventory::getProductCount() const {
    std::lock_guard<std::mutex> lock(inventoryMutex_);
    return products_.size();
}

bool Inventory::loadFromFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(inventoryMutex_);
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[Inventory] Warning: Could not open file for loading: " << filepath << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    std::string content = buffer.str();
    if (content.empty()) {
        return false;
    }

    auto json = crow::json::load(content);
    if (!json || json.t() != crow::json::type::List) {
        std::cerr << "[Inventory] Error: Invalid JSON structure in " << filepath << std::endl;
        return false;
    }

    products_.clear();
    int maxId = 0;

    for (const auto& item : json) {
        Product p = Product::fromJson(item);
        if (p.getId() > 0) {
            products_[p.getId()] = p;
            if (p.getId() > maxId) {
                maxId = p.getId();
            }
        }
    }

    nextId_ = maxId + 1;
    return true;
}

bool Inventory::saveToFile(const std::string& filepath) const {
    std::lock_guard<std::mutex> lock(inventoryMutex_);
    std::ofstream file(filepath, std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "[Inventory] Error: Could not open file for writing: " << filepath << std::endl;
        return false;
    }

    std::vector<crow::json::wvalue> items;
    items.reserve(products_.size());
    for (const auto& pair : products_) {
        items.push_back(pair.second.toJson());
    }

    crow::json::wvalue root(items);
    file << root.dump();
    file.close();
    return true;
}

crow::json::wvalue Inventory::toJson() const {
    std::lock_guard<std::mutex> lock(inventoryMutex_);
    std::vector<crow::json::wvalue> items;
    items.reserve(products_.size());
    for (const auto& pair : products_) {
        items.push_back(pair.second.toJson());
    }
    return crow::json::wvalue(items);
}
