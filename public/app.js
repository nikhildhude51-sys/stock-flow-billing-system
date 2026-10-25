/**
 * ============================================================================
 * Inventory Management & Billing System - Frontend Application
 * Vanilla JavaScript interacting with C++ Crow REST API
 * ============================================================================
 */

(function () {
  'use strict';

  // --- Application State ---
  const state = {
    products: [],
    sales: [],
    cart: [],
    activePage: 'dashboard',
    searchKeywords: {
      pos: '',
      products: ''
    },
    activeReceipt: null
  };

  // --- API Client ---
  const API = {
    async getProducts() {
      const res = await fetch('/api/products');
      if (!res.ok) throw new Error('Failed to load products');
      return await res.json();
    },

    async addOrUpdateProduct(productData) {
      const res = await fetch('/api/products', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(productData)
      });
      const data = await res.json();
      if (!res.ok) throw new Error(data.error || 'Failed to save product');
      return data;
    },

    async deleteProduct(id) {
      const res = await fetch(`/api/products/${id}`, {
        method: 'DELETE'
      });
      const data = await res.json();
      if (!res.ok) throw new Error(data.error || 'Failed to delete product');
      return data;
    },

    async getSales() {
      const res = await fetch('/api/sales');
      if (!res.ok) throw new Error('Failed to load sales history');
      return await res.json();
    },

    async checkout(checkoutData) {
      const res = await fetch('/api/checkout', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(checkoutData)
      });
      const data = await res.json();
      if (!res.ok) throw new Error(data.message || 'Checkout failed');
      return data;
    }
  };

  // --- Utilities ---
  function formatCurrency(amount) {
    const num = Number(amount) || 0;
    return '$' + num.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 });
  }

  function formatProductId(id) {
    return 'P' + String(id).padStart(3, '0');
  }

  function showToast(message, type = 'info') {
    const container = document.getElementById('toastContainer');
    if (!container) return;

    const toast = document.createElement('div');
    toast.className = `toast toast-${type}`;
    toast.textContent = message;

    container.appendChild(toast);

    setTimeout(() => {
      toast.style.animation = 'slideOut 0.2s forwards';
      setTimeout(() => toast.remove(), 200);
    }, 3200);
  }

  // --- DOM Elements ---
  const dom = {
    // Nav
    navItems: document.querySelectorAll('.sidebar-nav .nav-item'),
    signOutBtn: document.getElementById('signOutBtn'),
    themeToggleBtn: document.getElementById('themeToggleBtn'),
    pageViews: document.querySelectorAll('.page-view'),

    // Dashboard
    dashTotalProducts: document.getElementById('dashTotalProducts'),
    dashUnitsInStock: document.getElementById('dashUnitsInStock'),
    dashCompletedSales: document.getElementById('dashCompletedSales'),
    dashRecentSalesBody: document.getElementById('dashRecentSalesBody'),
    dashViewAllSales: document.getElementById('dashViewAllSales'),

    // POS
    posSearchInput: document.getElementById('posSearchInput'),
    posProductGrid: document.getElementById('posProductGrid'),
    posCustomerSelect: document.getElementById('posCustomerSelect'),
    posCartList: document.getElementById('posCartList'),
    posCartTotal: document.getElementById('posCartTotal'),
    posProcessSaleBtn: document.getElementById('posProcessSaleBtn'),

    // Products
    productsSearchInput: document.getElementById('productsSearchInput'),
    productsTableBody: document.getElementById('productsTableBody'),
    openAddProductBtn: document.getElementById('openAddProductBtn'),
    closeProductFormBtn: document.getElementById('closeProductFormBtn'),
    productFormPanel: document.getElementById('productFormPanel'),
    productForm: document.getElementById('productForm'),
    productFormTitle: document.getElementById('productFormTitle'),
    editProductId: document.getElementById('editProductId'),
    productNameInput: document.getElementById('productNameInput'),
    productPriceInput: document.getElementById('productPriceInput'),
    productStockInput: document.getElementById('productStockInput'),

    // Sales
    salesTotalRevenue: document.getElementById('salesTotalRevenue'),
    salesTotalTransactions: document.getElementById('salesTotalTransactions'),
    salesTotalItemsSold: document.getElementById('salesTotalItemsSold'),
    salesTableBody: document.getElementById('salesTableBody'),

    // Customers
    customersSalesCount: document.getElementById('customersSalesCount'),

    // Payment Modal
    paymentModal: document.getElementById('paymentModal'),
    closePaymentModalBtn: document.getElementById('closePaymentModalBtn'),
    cancelPaymentBtn: document.getElementById('cancelPaymentBtn'),
    confirmPaymentBtn: document.getElementById('confirmPaymentBtn'),
    modalCustomerName: document.getElementById('modalCustomerName'),
    modalItemCount: document.getElementById('modalItemCount'),
    modalTotalAmount: document.getElementById('modalTotalAmount'),
    modalPaymentMethod: document.getElementById('modalPaymentMethod'),

    // Receipt Modal
    receiptModal: document.getElementById('receiptModal'),
    closeReceiptModalBtn: document.getElementById('closeReceiptModalBtn'),
    closeReceiptBtn: document.getElementById('closeReceiptBtn'),
    printReceiptBtn: document.getElementById('printReceiptBtn'),
    receiptBillId: document.getElementById('receiptBillId'),
    receiptDate: document.getElementById('receiptDate'),
    receiptCustomer: document.getElementById('receiptCustomer'),
    receiptMethod: document.getElementById('receiptMethod'),
    receiptItemsBody: document.getElementById('receiptItemsBody'),
    receiptSubtotal: document.getElementById('receiptSubtotal'),
    receiptTax: document.getElementById('receiptTax'),
    receiptGrandTotal: document.getElementById('receiptGrandTotal')
  };

  // --- Router / Page Switcher ---
  function navigateTo(pageId) {
    const validPages = ['dashboard', 'pos', 'products', 'customers', 'sales'];
    if (!validPages.includes(pageId)) {
      pageId = 'dashboard';
    }

    state.activePage = pageId;

    // Update active class on nav items
    dom.navItems.forEach(item => {
      if (item.dataset.page === pageId) {
        item.classList.add('active');
      } else {
        item.classList.remove('active');
      }
    });

    // Update active page view
    dom.pageViews.forEach(view => {
      if (view.id === `${pageId}Page`) {
        view.classList.add('active-page');
      } else {
        view.classList.remove('active-page');
      }
    });

    // Re-render current page
    renderCurrentPage();
  }

  function renderCurrentPage() {
    switch (state.activePage) {
      case 'dashboard':
        renderDashboard();
        break;
      case 'pos':
        renderPOS();
        break;
      case 'products':
        renderProducts();
        break;
      case 'sales':
        renderSales();
        break;
      case 'customers':
        renderCustomers();
        break;
    }
  }

  // --- Data Loading ---
  async function refreshData() {
    try {
      const [products, sales] = await Promise.all([
        API.getProducts(),
        API.getSales()
      ]);
      state.products = products;
      state.sales = sales;
      renderCurrentPage();
    } catch (err) {
      console.error('Data refresh error:', err);
      showToast('Error syncing with backend: ' + err.message, 'error');
    }
  }

  // ==========================================================================
  // PAGE RENDERERS
  // ==========================================================================

  // --- 1. Dashboard Renderer ---
  function renderDashboard() {
    // 1. Total products
    dom.dashTotalProducts.textContent = state.products.length;

    // 2. Units in stock
    const totalUnits = state.products.reduce((sum, p) => sum + (Number(p.stock) || 0), 0);
    dom.dashUnitsInStock.textContent = totalUnits;

    // 3. Completed sales
    dom.dashCompletedSales.textContent = state.sales.length;

    // 4. Recent sales table (top 5 sorted newest first)
    const recent = [...state.sales].reverse().slice(0, 5);
    dom.dashRecentSalesBody.innerHTML = '';

    if (recent.length === 0) {
      dom.dashRecentSalesBody.innerHTML = `
        <tr>
          <td colspan="5" class="text-center" style="padding: 24px; color: var(--text-muted);">
            No sales recorded yet. Use the POS tab to process new transactions.
          </td>
        </tr>`;
      return;
    }

    recent.forEach(sale => {
      const tr = document.createElement('tr');
      const itemCount = (sale.items && sale.items.length) ? sale.items.length : 0;
      tr.innerHTML = `
        <td class="font-medium">${sale.billId || 'INV-0000'}</td>
        <td style="color: var(--text-muted);">${sale.timestamp || 'N/A'}</td>
        <td>${itemCount}</td>
        <td class="font-bold">${formatCurrency(sale.totalAmount)}</td>
        <td><span class="badge badge-paid">Paid</span></td>
      `;
      dom.dashRecentSalesBody.appendChild(tr);
    });
  }

  // --- 2. Point of Sale (POS) Renderer ---
  function renderPOS() {
    // Filter products based on search keyword
    const keyword = state.searchKeywords.pos.toLowerCase().trim();
    const filtered = state.products.filter(p => 
      !keyword || p.name.toLowerCase().includes(keyword)
    );

    dom.posProductGrid.innerHTML = '';

    if (filtered.length === 0) {
      dom.posProductGrid.innerHTML = `
        <div style="grid-column: 1 / -1; text-align: center; padding: 48px 16px; color: var(--text-muted);">
          No products found matching "${keyword}".
        </div>`;
    } else {
      filtered.forEach(product => {
        const isOutOfStock = product.stock <= 0;
        const card = document.createElement('div');
        card.className = `product-card ${isOutOfStock ? 'out-of-stock' : ''}`;
        card.innerHTML = `
          <div>
            <div class="product-card-title" title="${product.name}">${product.name}</div>
            <div class="product-card-meta">
              <span class="product-card-stock">Stock: ${product.stock}</span>
              <span class="product-card-price">${formatCurrency(product.price)}</span>
            </div>
          </div>
          <button class="btn btn-add" data-id="${product.id}" ${isOutOfStock ? 'disabled' : ''}>
            <span>${isOutOfStock ? 'Out of Stock' : '+ Add'}</span>
          </button>
        `;

        const addBtn = card.querySelector('.btn-add');
        if (!isOutOfStock) {
          addBtn.addEventListener('click', () => addToCart(product));
        }

        dom.posProductGrid.appendChild(card);
      });
    }

    renderCart();
  }

  // --- Cart Management in POS ---
  function addToCart(product) {
    if (product.stock <= 0) {
      showToast(`${product.name} is out of stock!`, 'error');
      return;
    }

    const existing = state.cart.find(item => item.product.id === product.id);
    if (existing) {
      if (existing.quantity >= product.stock) {
        showToast(`Cannot add more. Only ${product.stock} unit(s) available in stock.`, 'error');
        return;
      }
      existing.quantity++;
    } else {
      state.cart.push({ product, quantity: 1 });
    }

    renderCart();
  }

  function updateCartQuantity(productId, delta) {
    const item = state.cart.find(i => i.product.id === productId);
    if (!item) return;

    const freshProduct = state.products.find(p => p.id === productId) || item.product;

    if (delta > 0) {
      if (item.quantity >= freshProduct.stock) {
        showToast(`Cannot exceed available stock (${freshProduct.stock}).`, 'error');
        return;
      }
      item.quantity++;
    } else if (delta < 0) {
      if (item.quantity > 1) {
        item.quantity--;
      } else {
        removeFromCart(productId);
        return;
      }
    }

    renderCart();
  }

  function removeFromCart(productId) {
    state.cart = state.cart.filter(i => i.product.id !== productId);
    renderCart();
  }

  function renderCart() {
    dom.posCartList.innerHTML = '';

    if (state.cart.length === 0) {
      dom.posCartList.innerHTML = `
        <div class="cart-empty-state">
          Your cart is empty.<br>Click <strong>+ Add</strong> on products to begin.
        </div>`;
      dom.posCartTotal.textContent = '$0.00';
      dom.posProcessSaleBtn.disabled = true;
      dom.posProcessSaleBtn.style.opacity = '0.5';
      dom.posProcessSaleBtn.style.cursor = 'not-allowed';
      return;
    }

    dom.posProcessSaleBtn.disabled = false;
    dom.posProcessSaleBtn.style.opacity = '1';
    dom.posProcessSaleBtn.style.cursor = 'pointer';

    let total = 0;

    state.cart.forEach(item => {
      const subtotal = item.product.price * item.quantity;
      total += subtotal;

      const card = document.createElement('div');
      card.className = 'cart-item-card';
      card.innerHTML = `
        <div class="cart-item-top">
          <span class="cart-item-name">${item.product.name}</span>
          <button class="btn-remove-item" title="Remove item" data-id="${item.product.id}">&times;</button>
        </div>
        <div class="cart-item-bottom">
          <div class="qty-stepper">
            <button class="qty-btn btn-minus" data-id="${item.product.id}">-</button>
            <span class="qty-value">${item.quantity}</span>
            <button class="qty-btn btn-plus" data-id="${item.product.id}">+</button>
          </div>
          <span class="unit-price-label">${formatCurrency(item.product.price)} each</span>
          <span class="cart-item-subtotal">${formatCurrency(subtotal)}</span>
        </div>
      `;

      card.querySelector('.btn-remove-item').addEventListener('click', () => removeFromCart(item.product.id));
      card.querySelector('.btn-minus').addEventListener('click', () => updateCartQuantity(item.product.id, -1));
      card.querySelector('.btn-plus').addEventListener('click', () => updateCartQuantity(item.product.id, 1));

      dom.posCartList.appendChild(card);
    });

    dom.posCartTotal.textContent = formatCurrency(total);
  }

  // --- 3. Products Page Renderer ---
  function renderProducts() {
    const keyword = state.searchKeywords.products.toLowerCase().trim();
    const filtered = state.products.filter(p =>
      !keyword || p.name.toLowerCase().includes(keyword) || formatProductId(p.id).toLowerCase().includes(keyword)
    );

    dom.productsTableBody.innerHTML = '';

    if (filtered.length === 0) {
      dom.productsTableBody.innerHTML = `
        <tr>
          <td colspan="5" class="text-center" style="padding: 28px; color: var(--text-muted);">
            No products found matching "${keyword}".
          </td>
        </tr>`;
      return;
    }

    filtered.forEach(product => {
      const tr = document.createElement('tr');
      const isOut = product.stock <= 0;
      tr.innerHTML = `
        <td class="font-medium">${formatProductId(product.id)}</td>
        <td class="font-bold">${product.name}</td>
        <td>${formatCurrency(product.price)}</td>
        <td>
          <span style="${isOut ? 'color: var(--accent-red-text); font-weight: 600;' : ''}">
            ${product.stock} ${isOut ? '(Out of stock)' : ''}
          </span>
        </td>
        <td class="text-center">
          <div class="action-buttons">
            <button class="btn-action-icon btn-edit" title="Edit product" data-id="${product.id}">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                <path d="M17 3a2.85 2.83 0 1 1 4 4L7.5 20.5 2 22l1.5-5.5Z"/>
                <path d="m15 5 4 4"/>
              </svg>
            </button>
            <button class="btn-action-icon btn-delete" title="Delete product" data-id="${product.id}">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                <path d="M3 6h18"/>
                <path d="M19 6v14c0 1-1 2-2 2H7c-1 0-2-1-2-2V6"/>
                <path d="M8 6V4c0-1 1-2 2-2h4c1 0 2 1 2 2v2"/>
              </svg>
            </button>
          </div>
        </td>
      `;

      tr.querySelector('.btn-edit').addEventListener('click', () => openEditProductForm(product));
      tr.querySelector('.btn-delete').addEventListener('click', () => handleDeleteProduct(product));

      dom.productsTableBody.appendChild(tr);
    });
  }

  function openAddProductForm() {
    dom.productFormTitle.textContent = 'Add Product';
    dom.editProductId.value = '';
    dom.productNameInput.value = '';
    dom.productPriceInput.value = '';
    dom.productStockInput.value = '';
    dom.productFormPanel.style.display = 'block';
    dom.productNameInput.focus();
  }

  function openEditProductForm(product) {
    dom.productFormTitle.textContent = 'Edit Product';
    dom.editProductId.value = product.id;
    dom.productNameInput.value = product.name;
    dom.productPriceInput.value = product.price;
    dom.productStockInput.value = product.stock;
    dom.productFormPanel.style.display = 'block';
    dom.productNameInput.focus();
  }

  async function handleProductFormSubmit(e) {
    e.preventDefault();

    const name = dom.productNameInput.value.trim();
    const price = parseFloat(dom.productPriceInput.value);
    const stock = parseInt(dom.productStockInput.value, 10);
    const editId = dom.editProductId.value ? parseInt(dom.editProductId.value, 10) : null;

    if (!name || isNaN(price) || isNaN(stock)) {
      showToast('Please fill in all product fields correctly.', 'error');
      return;
    }

    if (price < 0 || stock < 0) {
      showToast('Price and stock cannot be negative.', 'error');
      return;
    }

    const payload = { name, price, stock };
    if (editId) payload.id = editId;

    try {
      const res = await API.addOrUpdateProduct(payload);
      showToast(editId ? 'Product updated successfully.' : 'Product added successfully.', 'success');
      dom.productFormPanel.style.display = 'none';
      await refreshData();
    } catch (err) {
      showToast('Save failed: ' + err.message, 'error');
    }
  }

  async function handleDeleteProduct(product) {
    const confirmMsg = `Are you sure you want to delete "${product.name}"?`;
    if (!confirm(confirmMsg)) return;

    try {
      await API.deleteProduct(product.id);
      showToast(`"${product.name}" deleted successfully.`, 'success');
      // If product was in cart, remove it
      state.cart = state.cart.filter(item => item.product.id !== product.id);
      await refreshData();
    } catch (err) {
      showToast('Delete failed: ' + err.message, 'error');
    }
  }

  // --- 4. Sales Page Renderer ---
  function renderSales() {
    // 1. Total revenue
    const totalRev = state.sales.reduce((sum, s) => sum + (Number(s.totalAmount) || 0), 0);
    dom.salesTotalRevenue.textContent = formatCurrency(totalRev);

    // 2. Total transactions
    dom.salesTotalTransactions.textContent = state.sales.length;

    // 3. Total items sold
    let totalItems = 0;
    state.sales.forEach(sale => {
      if (sale.items) {
        sale.items.forEach(it => {
          totalItems += Number(it.quantity) || 1;
        });
      }
    });
    dom.salesTotalItemsSold.textContent = totalItems;

    // 4. Sales Invoices Table (sorted newest first)
    const sorted = [...state.sales].reverse();
    dom.salesTableBody.innerHTML = '';

    if (sorted.length === 0) {
      dom.salesTableBody.innerHTML = `
        <tr>
          <td colspan="6" class="text-center" style="padding: 32px; color: var(--text-muted);">
            No sales recorded yet. Process new sales from the POS tab.
          </td>
        </tr>`;
      return;
    }

    sorted.forEach(sale => {
      const tr = document.createElement('tr');
      const itemCount = (sale.items && sale.items.length) ? sale.items.length : 0;
      tr.innerHTML = `
        <td class="font-medium">${sale.billId || 'INV-0000'}</td>
        <td style="color: var(--text-muted);">${sale.timestamp || 'N/A'}</td>
        <td>${itemCount}</td>
        <td class="font-bold">${formatCurrency(sale.totalAmount)}</td>
        <td><span class="badge badge-paid">Paid</span></td>
        <td class="text-center">
          <button class="btn btn-outline btn-sm btn-view-receipt" data-billid="${sale.billId}">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="width: 14px; height: 14px;">
              <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/>
              <polyline points="14 2 14 8 20 8"/>
              <line x1="16" x2="8" y1="13" y2="13"/>
              <line x1="16" x2="8" y1="17" y2="17"/>
            </svg>
            <span>View Receipt</span>
          </button>
        </td>
      `;

      tr.querySelector('.btn-view-receipt').addEventListener('click', () => openReceiptModal(sale));
      dom.salesTableBody.appendChild(tr);
    });
  }

  // --- 5. Customers Page Renderer ---
  function renderCustomers() {
    const walkinSalesCount = state.sales.filter(s => 
      !s.customerName || s.customerName.toLowerCase() === 'walk-in customer'
    ).length;
    dom.customersSalesCount.textContent = walkinSalesCount;
  }

  // ==========================================================================
  // CHECKOUT & PAYMENT MODAL WORKFLOW
  // ==========================================================================
  function openPaymentModal() {
    if (state.cart.length === 0) {
      showToast('Cart is empty. Add products to process a sale.', 'error');
      return;
    }

    // Pre-validate stock locally before opening modal
    for (const item of state.cart) {
      const live = state.products.find(p => p.id === item.product.id);
      if (!live || live.stock < item.quantity) {
        showToast(`Insufficient stock for "${item.product.name}". Available: ${live ? live.stock : 0}`, 'error');
        return;
      }
    }

    const total = state.cart.reduce((sum, item) => sum + (item.product.price * item.quantity), 0);
    const totalItems = state.cart.reduce((sum, item) => sum + item.quantity, 0);

    dom.modalCustomerName.textContent = dom.posCustomerSelect.value || 'Walk-in Customer';
    dom.modalItemCount.textContent = totalItems;
    dom.modalTotalAmount.textContent = formatCurrency(total);
    dom.paymentModal.classList.add('active');
  }

  function closePaymentModal() {
    dom.paymentModal.classList.remove('active');
  }

  async function handleConfirmPayment() {
    if (dom.confirmPaymentBtn.disabled) return;

    dom.confirmPaymentBtn.disabled = true;
    dom.confirmPaymentBtn.textContent = 'Processing...';

    const customerName = dom.posCustomerSelect.value || 'Walk-in Customer';
    const paymentMethod = dom.modalPaymentMethod.value || 'Cash';

    const checkoutPayload = {
      customerName,
      paymentMethod,
      paymentConfirmed: true, // Crucial: C++ verifies payment confirmation
      items: state.cart.map(item => ({
        productId: item.product.id,
        quantity: item.quantity
      }))
    };

    try {
      const result = await API.checkout(checkoutPayload);
      closePaymentModal();
      showToast('Payment confirmed! Sale processed successfully.', 'success');

      // Clear cart
      state.cart = [];
      renderCart();

      // Refresh products and sales from backend
      await refreshData();

      // Open printable receipt modal for the new invoice
      if (result.bill) {
        openReceiptModal(result.bill);
      }
    } catch (err) {
      console.error('Checkout failed:', err);
      showToast('Checkout Failed: ' + err.message, 'error');
    } finally {
      dom.confirmPaymentBtn.disabled = false;
      dom.confirmPaymentBtn.innerHTML = '<span>Confirm & Pay</span>';
    }
  }

  // ==========================================================================
  // RECEIPT MODAL
  // ==========================================================================
  function openReceiptModal(bill) {
    state.activeReceipt = bill;

    dom.receiptBillId.textContent = bill.billId || 'INV-0000';
    dom.receiptDate.textContent = bill.timestamp || new Date().toLocaleString();
    dom.receiptCustomer.textContent = bill.customerName || 'Walk-in Customer';
    dom.receiptMethod.textContent = bill.paymentMethod || 'Cash';

    dom.receiptItemsBody.innerHTML = '';
    if (bill.items && bill.items.length) {
      bill.items.forEach(item => {
        const tr = document.createElement('tr');
        tr.innerHTML = `
          <td>${item.productName || 'Item'}</td>
          <td class="text-center">${item.quantity}</td>
          <td class="text-right">${formatCurrency(item.unitPrice)}</td>
          <td class="text-right font-medium">${formatCurrency(item.subtotal)}</td>
        `;
        dom.receiptItemsBody.appendChild(tr);
      });
    }

    dom.receiptSubtotal.textContent = formatCurrency(bill.subtotal || bill.totalAmount);
    dom.receiptTax.textContent = formatCurrency(bill.taxAmount || 0);
    dom.receiptGrandTotal.textContent = formatCurrency(bill.totalAmount);

    dom.receiptModal.classList.add('active');
  }

  function closeReceiptModal() {
    dom.receiptModal.classList.remove('active');
    state.activeReceipt = null;
  }

  // --- Theme Management ---
  function initTheme() {
    const saved = localStorage.getItem('theme') || 'light';
    document.documentElement.setAttribute('data-theme', saved);
  }

  function toggleTheme() {
    const current = document.documentElement.getAttribute('data-theme');
    const next = current === 'dark' ? 'light' : 'dark';
    document.documentElement.setAttribute('data-theme', next);
    localStorage.setItem('theme', next);
  }

  // ==========================================================================
  // EVENT LISTENERS & INITIALIZATION
  // ==========================================================================
  function bindEvents() {
    // Nav links
    dom.navItems.forEach(item => {
      item.addEventListener('click', (e) => {
        e.preventDefault();
        const page = item.dataset.page;
        if (page) {
          window.location.hash = page;
          navigateTo(page);
        }
      });
    });

    // View All link in Dashboard
    if (dom.dashViewAllSales) {
      dom.dashViewAllSales.addEventListener('click', (e) => {
        e.preventDefault();
        window.location.hash = 'sales';
        navigateTo('sales');
      });
    }

    // Sign Out button
    if (dom.signOutBtn) {
      dom.signOutBtn.addEventListener('click', (e) => {
        e.preventDefault();
        showToast('Signed out of Admin Panel session.', 'info');
      });
    }

    // Theme Toggle
    if (dom.themeToggleBtn) {
      dom.themeToggleBtn.addEventListener('click', toggleTheme);
    }

    // POS Search Input
    if (dom.posSearchInput) {
      dom.posSearchInput.addEventListener('input', (e) => {
        state.searchKeywords.pos = e.target.value;
        renderPOS();
      });
    }

    // Process Sale Button
    if (dom.posProcessSaleBtn) {
      dom.posProcessSaleBtn.addEventListener('click', openPaymentModal);
    }

    // Products Search Input
    if (dom.productsSearchInput) {
      dom.productsSearchInput.addEventListener('input', (e) => {
        state.searchKeywords.products = e.target.value;
        renderProducts();
      });
    }

    // Add Product Form Toggle & Submit
    if (dom.openAddProductBtn) {
      dom.openAddProductBtn.addEventListener('click', openAddProductForm);
    }
    if (dom.closeProductFormBtn) {
      dom.closeProductFormBtn.addEventListener('click', () => {
        dom.productFormPanel.style.display = 'none';
      });
    }
    if (dom.productForm) {
      dom.productForm.addEventListener('submit', handleProductFormSubmit);
    }

    // Payment Modal Buttons
    if (dom.closePaymentModalBtn) {
      dom.closePaymentModalBtn.addEventListener('click', closePaymentModal);
    }
    if (dom.cancelPaymentBtn) {
      dom.cancelPaymentBtn.addEventListener('click', closePaymentModal);
    }
    if (dom.confirmPaymentBtn) {
      dom.confirmPaymentBtn.addEventListener('click', handleConfirmPayment);
    }

    // Receipt Modal Buttons
    if (dom.closeReceiptModalBtn) {
      dom.closeReceiptModalBtn.addEventListener('click', closeReceiptModal);
    }
    if (dom.closeReceiptBtn) {
      dom.closeReceiptBtn.addEventListener('click', closeReceiptModal);
    }
    if (dom.printReceiptBtn) {
      dom.printReceiptBtn.addEventListener('click', () => {
        window.print();
      });
    }

    // Hash change handler for browser back/forward buttons
    window.addEventListener('hashchange', () => {
      const hash = window.location.hash.replace('#', '') || 'dashboard';
      navigateTo(hash);
    });
  }

  // --- Bootstrap ---
  async function init() {
    initTheme();
    bindEvents();

    const initialPage = window.location.hash.replace('#', '') || 'dashboard';
    navigateTo(initialPage);

    await refreshData();
  }

  // Start app on DOM ready
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }

})();
