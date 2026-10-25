# Inventory Management and Billing System

**Subject:** Object-Oriented Programming (OOP) with C++  
**Technology Stack:**
* **Backend:** C++17 with Crow Web Framework (`crow.h`, standalone Asio, Winsock)
* **Frontend:** HTML5, CSS3, Vanilla JavaScript (Modern Minimal UI based on Admin Panel specifications)
* **Storage:** Local Persistent JSON files (`data/inventory.json` & `data/sales.json`)

---

## 🏗️ Architecture & OOP Implementation

This project implements all core inventory management, billing calculations, stock validation, and payment verification directly in C++ using Object-Oriented Programming principles:

| Class | OOP Concept | Description |
| :--- | :--- | :--- |
| **`Product`** | **Encapsulation & Data Hiding** | Stores `id`, `name`, `price`, `stockQuantity` in private data members. Validates stock sufficiency and handles atomic decrements. |
| **`Inventory`** | **Collection Management** | Manages catalog storage (`std::map<int, Product>`), supports search, add, update, delete, and persists to `data/inventory.json`. |
| **`Bill` & `BillItem`** | **Composition & State Tracking** | `Bill` is composed of multiple `BillItem` line objects. Computes subtotals, taxes, totals, and prevents duplicate payments via `markPaid()`. |
| **`BillingSystem`** | **Facade / Central Coordinator** | Orchestrates checkout transactions: pre-validates all requested item stocks, requires explicit payment confirmation before deducting inventory, ensures transaction atomicity, and updates `data/sales.json`. |

---

## 🚀 How to Run the Application

### Option A: One-Click Launch (Recommended)
Double-click `run_server.bat` in the project folder.  
It will automatically launch the C++ server on port 8080 and open the web application in your default browser at:  
👉 **`http://localhost:8080`**

### Option B: Terminal Command
From PowerShell or Command Prompt inside the project directory:
```powershell
.\build\server.exe 8080
```
Then navigate to `http://localhost:8080` in any web browser.

---

## 🛠️ Rebuilding from Source
If you ever edit C++ code, recompile simply using:
```powershell
.\build.ps1
```
Or with CMake:
```powershell
cmake -B build -G "MinGW Makefiles"
cmake --build build
```

---

## 🧪 Running Automated Tests
Run the comprehensive test suites anytime:
```powershell
.\test_full_system.ps1
```
