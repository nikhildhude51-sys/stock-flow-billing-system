#include "Bill.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cmath>

// Helper function to generate a human-readable local timestamp
static std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
#if defined(_WIN32)
    localtime_s(&tm_buf, &now_time);
#else
    localtime_r(&now_time, &tm_buf);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// --- BillItem Implementation ---
BillItem::BillItem() : productId(0), productName(""), unitPrice(0.0), quantity(0), subtotal(0.0) {}

BillItem::BillItem(int prodId, const std::string& name, double price, int qty)
    : productId(prodId), productName(name), unitPrice(price), quantity(qty), 
      subtotal(std::round(price * qty * 100.0) / 100.0) {}

crow::json::wvalue BillItem::toJson() const {
    crow::json::wvalue json;
    json["productId"] = productId;
    json["productName"] = productName;
    json["unitPrice"] = unitPrice;
    json["quantity"] = quantity;
    json["subtotal"] = subtotal;
    return json;
}

BillItem BillItem::fromJson(const crow::json::rvalue& json) {
    BillItem item;
    item.productId = json.has("productId") ? json["productId"].i() : 0;
    item.productName = json.has("productName") ? std::string(json["productName"].s()) : "";
    item.unitPrice = json.has("unitPrice") ? json["unitPrice"].d() : 0.0;
    item.quantity = json.has("quantity") ? json["quantity"].i() : 0;
    item.subtotal = json.has("subtotal") ? json["subtotal"].d() : (std::round(item.unitPrice * item.quantity * 100.0) / 100.0);
    return item;
}

// --- Bill Implementation ---
Bill::Bill()
    : billId_(""), customerName_("Walk-in Customer"), paymentMethod_("Cash"),
      timestamp_(getCurrentTimestamp()), subtotal_(0.0), taxAmount_(0.0), totalAmount_(0.0), isPaid_(false) {}

Bill::Bill(const std::string& billId, const std::string& customerName, const std::string& paymentMethod)
    : billId_(billId), customerName_(customerName.empty() ? "Walk-in Customer" : customerName),
      paymentMethod_(paymentMethod.empty() ? "Cash" : paymentMethod),
      timestamp_(getCurrentTimestamp()), subtotal_(0.0), taxAmount_(0.0), totalAmount_(0.0), isPaid_(false) {}

void Bill::addItem(const BillItem& item) {
    items_.push_back(item);
    calculateTotals();
}

void Bill::addItem(int productId, const std::string& name, double unitPrice, int quantity) {
    items_.emplace_back(productId, name, unitPrice, quantity);
    calculateTotals();
}

void Bill::clearItems() {
    items_.clear();
    subtotal_ = 0.0;
    taxAmount_ = 0.0;
    totalAmount_ = 0.0;
}

void Bill::calculateTotals(double taxRatePercent) {
    subtotal_ = 0.0;
    for (const auto& item : items_) {
        subtotal_ += item.subtotal;
    }
    subtotal_ = std::round(subtotal_ * 100.0) / 100.0;
    taxAmount_ = std::round(((subtotal_ * taxRatePercent) / 100.0) * 100.0) / 100.0;
    totalAmount_ = std::round((subtotal_ + taxAmount_) * 100.0) / 100.0;
}

bool Bill::markPaid() {
    // Prevent duplicate payment processing
    if (isPaid_) {
        return false;
    }
    isPaid_ = true;
    return true;
}

// Getters
std::string Bill::getBillId() const { return billId_; }
std::string Bill::getCustomerName() const { return customerName_; }
std::string Bill::getPaymentMethod() const { return paymentMethod_; }
std::string Bill::getTimestamp() const { return timestamp_; }
const std::vector<BillItem>& Bill::getItems() const { return items_; }
double Bill::getSubtotal() const { return subtotal_; }
double Bill::getTaxAmount() const { return taxAmount_; }
double Bill::getTotalAmount() const { return totalAmount_; }
bool Bill::isPaid() const { return isPaid_; }

// Setters
void Bill::setTimestamp(const std::string& timestamp) { timestamp_ = timestamp; }
void Bill::setCustomerName(const std::string& name) { customerName_ = name; }
void Bill::setPaymentMethod(const std::string& method) { paymentMethod_ = method; }

crow::json::wvalue Bill::toJson() const {
    crow::json::wvalue json;
    json["billId"] = billId_;
    json["customerName"] = customerName_;
    json["paymentMethod"] = paymentMethod_;
    json["timestamp"] = timestamp_;
    json["subtotal"] = subtotal_;
    json["taxAmount"] = taxAmount_;
    json["totalAmount"] = totalAmount_;
    json["isPaid"] = isPaid_;

    std::vector<crow::json::wvalue> itemsJson;
    itemsJson.reserve(items_.size());
    for (const auto& item : items_) {
        itemsJson.push_back(item.toJson());
    }
    json["items"] = std::move(itemsJson);

    return json;
}

Bill Bill::fromJson(const crow::json::rvalue& json) {
    Bill bill;
    bill.billId_ = json.has("billId") ? std::string(json["billId"].s()) : "";
    bill.customerName_ = json.has("customerName") ? std::string(json["customerName"].s()) : "Walk-in Customer";
    bill.paymentMethod_ = json.has("paymentMethod") ? std::string(json["paymentMethod"].s()) : "Cash";
    bill.timestamp_ = json.has("timestamp") ? std::string(json["timestamp"].s()) : "";
    bill.subtotal_ = json.has("subtotal") ? json["subtotal"].d() : 0.0;
    bill.taxAmount_ = json.has("taxAmount") ? json["taxAmount"].d() : 0.0;
    bill.totalAmount_ = json.has("totalAmount") ? json["totalAmount"].d() : 0.0;
    bill.isPaid_ = json.has("isPaid") ? json["isPaid"].b() : false;

    if (json.has("items")) {
        for (const auto& itemJson : json["items"]) {
            bill.items_.push_back(BillItem::fromJson(itemJson));
        }
    }
    return bill;
}
