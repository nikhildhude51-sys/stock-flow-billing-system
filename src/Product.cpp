#include "Product.h"
#include <stdexcept>

// Default constructor initializing default valid state
Product::Product() : id_(0), name_(""), price_(0.0), stockQuantity_(0) {}

// Parameterized constructor initializing attributes with validation
Product::Product(int id, const std::string& name, double price, int stockQuantity)
    : id_(id), name_(name), price_(price >= 0.0 ? price : 0.0), 
      stockQuantity_(stockQuantity >= 0 ? stockQuantity : 0) {}

// Getters
int Product::getId() const { return id_; }
std::string Product::getName() const { return name_; }
double Product::getPrice() const { return price_; }
int Product::getStockQuantity() const { return stockQuantity_; }

// Setters
void Product::setName(const std::string& name) {
    name_ = name;
}

bool Product::setPrice(double price) {
    if (price >= 0.0) {
        price_ = price;
        return true;
    }
    return false;
}

bool Product::setStockQuantity(int quantity) {
    if (quantity >= 0) {
        stockQuantity_ = quantity;
        return true;
    }
    return false;
}

// Business logic: check if there is enough stock
bool Product::hasSufficientStock(int quantity) const {
    return quantity > 0 && stockQuantity_ >= quantity;
}

// Business logic: atomically reduce stock if sufficient
bool Product::reduceStock(int quantity) {
    if (hasSufficientStock(quantity)) {
        stockQuantity_ -= quantity;
        return true;
    }
    return false;
}

// Business logic: add stock back
void Product::addStock(int quantity) {
    if (quantity > 0) {
        stockQuantity_ += quantity;
    }
}

// Convert Product to JSON object
crow::json::wvalue Product::toJson() const {
    crow::json::wvalue json;
    json["id"] = id_;
    json["name"] = name_;
    json["price"] = price_;
    json["stock"] = stockQuantity_;
    return json;
}

// Parse Product from JSON object
Product Product::fromJson(const crow::json::rvalue& json) {
    int id = json.has("id") ? json["id"].i() : 0;
    std::string name = json.has("name") ? std::string(json["name"].s()) : "";
    double price = json.has("price") ? json["price"].d() : 0.0;
    int stock = json.has("stock") ? json["stock"].i() : 0;
    return Product(id, name, price, stock);
}
