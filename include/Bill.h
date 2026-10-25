#ifndef BILL_H
#define BILL_H

#include <string>
#include <vector>
#include "crow/json.h"

/**
 * ============================================================================
 * STRUCT / CLASS: BillItem
 * OOP Concept: Composition Component
 * 
 * Description:
 * Represents a line item inside an invoice/bill.
 * ============================================================================
 */
struct BillItem {
    int productId;
    std::string productName;
    double unitPrice;
    int quantity;
    double subtotal;

    BillItem();
    BillItem(int prodId, const std::string& name, double price, int qty);

    crow::json::wvalue toJson() const;
    static BillItem fromJson(const crow::json::rvalue& json);
};

/**
 * ============================================================================
 * CLASS: Bill
 * OOP Concept: Composition & Encapsulation
 * 
 * Description:
 * Encapsulates an entire sale invoice.
 * A Bill IS COMPOSED OF multiple BillItem objects.
 * Tracks invoice ID, customer name, timestamp, list of purchased items,
 * monetary totals, and payment status (preventing double billing).
 * ============================================================================
 */
class Bill {
private:
    std::string billId_;
    std::string customerName_;
    std::string paymentMethod_;
    std::string timestamp_;
    std::vector<BillItem> items_;
    double subtotal_;
    double taxAmount_;
    double totalAmount_;
    bool isPaid_;

public:
    // Constructors
    Bill();
    Bill(const std::string& billId, const std::string& customerName, const std::string& paymentMethod);

    // --- Item Management (Composition) ---
    void addItem(const BillItem& item);
    void addItem(int productId, const std::string& name, double unitPrice, int quantity);
    void clearItems();

    // Recalculates subtotal, taxes, and total amounts
    void calculateTotals(double taxRatePercent = 0.0);

    // Marks the bill as paid (Payment Confirmation)
    // Returns false if the bill was already paid (prevents duplicate payment)
    bool markPaid();

    // --- Getters ---
    std::string getBillId() const;
    std::string getCustomerName() const;
    std::string getPaymentMethod() const;
    std::string getTimestamp() const;
    const std::vector<BillItem>& getItems() const;
    double getSubtotal() const;
    double getTaxAmount() const;
    double getTotalAmount() const;
    bool isPaid() const;

    // --- Setters ---
    void setTimestamp(const std::string& timestamp);
    void setCustomerName(const std::string& name);
    void setPaymentMethod(const std::string& method);

    // --- Serialization ---
    crow::json::wvalue toJson() const;
    static Bill fromJson(const crow::json::rvalue& json);
};

#endif // BILL_H
