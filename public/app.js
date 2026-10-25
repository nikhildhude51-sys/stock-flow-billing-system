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
    isAuthenticated: false,
    currentUser: null,
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
  function handleUnauthorized() {
    if (state.isAuthenticated) {
      showToast('Session expired. Please sign in again.', 'error');
    }
    showLoginView('Session expired or unauthorized. Please sign in.');
  }

  const API = {
    async checkAuth() {
      try {
        const res = await fetch('/api/auth/me');
        if (!res.ok) return { authenticated: false };
        return await res.json();
      } catch (err) {
        return { authenticated: false };
      }
    },

    async login(username, password) {
      const res = await fetch('/api/auth/login', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ username, password })
      });
      const data = await res.json();
      if (!res.ok) {
        throw new Error(data.message || 'Login failed');
      }
      return data;
    },

    async logout() {
      try {
        await fetch('/api/auth/logout', { method: 'POST' });
      } catch (err) {
        console.error('Logout error:', err);
      }
    },

    async changePassword(currentPassword, newPassword, confirmPassword) {
      const res = await fetch('/api/auth/change-password', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ currentPassword, newPassword, confirmPassword })
      });
      if (res.status === 401) {
        handleUnauthorized();
        throw new Error('Unauthorized');
      }
      const data = await res.json();
      if (!res.ok) {
        throw new Error(data.message || 'Failed to change password');
      }
      return data;
    },

    async changeUsername(currentPassword, newUsername) {
      const res = await fetch('/api/auth/change-username', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ currentPassword, newUsername })
      });
      if (res.status === 401) {
        handleUnauthorized();
        throw new Error('Unauthorized');
      }
      const data = await res.json();
      if (!res.ok) {
        throw new Error(data.message || 'Failed to change username');
      }
      return data;
    },

    async getProducts() {
      const res = await fetch('/api/products');
      if (res.status === 401) {
        handleUnauthorized();
        throw new Error('Unauthorized');
      }
      if (!res.ok) throw new Error('Failed to load products');
      return await res.json();
    },

    async addOrUpdateProduct(productData) {
      const res = await fetch('/api/products', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(productData)
      });
      if (res.status === 401) {
        handleUnauthorized();
        throw new Error('Unauthorized');
      }
      const data = await res.json();
      if (!res.ok) throw new Error(data.error || 'Failed to save product');
      return data;
    },

    async deleteProduct(id) {
      const res = await fetch(`/api/products/${id}`, {
        method: 'DELETE'
      });
      if (res.status === 401) {
        handleUnauthorized();
        throw new Error('Unauthorized');
      }
      const data = await res.json();
      if (!res.ok) throw new Error(data.error || 'Failed to delete product');
      return data;
    },

    async getSales() {
      const res = await fetch('/api/sales');
      if (res.status === 401) {
        handleUnauthorized();
        throw new Error('Unauthorized');
      }
      if (!res.ok) throw new Error('Failed to load sales history');
      return await res.json();
    },

    async checkout(checkoutData) {
      const res = await fetch('/api/checkout', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(checkoutData)
      });
      if (res.status === 401) {
        handleUnauthorized();
        throw new Error('Unauthorized');
      }
      const data = await res.json();
      if (!res.ok) throw new Error(data.message || 'Checkout failed');
      return data;
    }
  };

  // --- Utilities ---
  function formatCurrency(amount) {
    const num = Number(amount) || 0;
    return '₹' + num.toLocaleString('en-IN', { minimumFractionDigits: 2, maximumFractionDigits: 2 });
  }

  function formatProductId(id) {
    return 'P' + String(id).padStart(3, '0');
  }

  function escapeHtml(str) {
    if (str == null) return '';
    return String(str)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#39;');
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
    // Auth
    loginContainer: document.getElementById('loginContainer'),
    appLayout: document.getElementById('appLayout'),
    loginForm: document.getElementById('loginForm'),
    loginUsername: document.getElementById('loginUsername'),
    loginPassword: document.getElementById('loginPassword'),
    togglePasswordBtn: document.getElementById('togglePasswordBtn'),
    eyeIcon: document.getElementById('eyeIcon'),
    loginErrorAlert: document.getElementById('loginErrorAlert'),
    loginErrorMessage: document.getElementById('loginErrorMessage'),
    loginSubmitBtn: document.getElementById('loginSubmitBtn'),
    headerAdminName: document.getElementById('headerAdminName'),

    // Nav & Sidebar
    appSidebar: document.getElementById('appSidebar'),
    sidebarToggleBtn: document.getElementById('sidebarToggleBtn'),
    sidebarBackdrop: document.getElementById('sidebarBackdrop'),
    navItems: document.querySelectorAll('.sidebar-nav .nav-item'),
    signOutBtn: document.getElementById('signOutBtn'),
    themeToggleBtn: document.getElementById('themeToggleBtn'),
    pageViews: document.querySelectorAll('.page-view'),

    // Dashboard
    dashTotalProducts: document.getElementById('dashTotalProducts'),
    dashUnitsInStock: document.getElementById('dashUnitsInStock'),
    dashCompletedSales: document.getElementById('dashCompletedSales'),
    dashLowStockCount: document.getElementById('dashLowStockCount'),
    dashOutOfStockCount: document.getElementById('dashOutOfStockCount'),
    dashRecentSalesBody: document.getElementById('dashRecentSalesBody'),
    dashViewAllSales: document.getElementById('dashViewAllSales'),

    // POS
    posSearchInput: document.getElementById('posSearchInput'),
    posProductGrid: document.getElementById('posProductGrid'),
    posCustomerSelect: document.getElementById('posCustomerSelect'),
    posCustomerNameInput: document.getElementById('posCustomerNameInput'),
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
    exportSalesCsvBtn: document.getElementById('exportSalesCsvBtn'),

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
    receiptGrandTotal: document.getElementById('receiptGrandTotal'),

    // Profile Menu
    profileMenuContainer: document.getElementById('profileMenuContainer'),
    profileAvatarBtn: document.getElementById('profileAvatarBtn'),
    profileDropdown: document.getElementById('profileDropdown'),
    dropdownUsername: document.getElementById('dropdownUsername'),
    dropdownRole: document.getElementById('dropdownRole'),
    dropdownChangeUsernameBtn: document.getElementById('dropdownChangeUsernameBtn'),
    dropdownChangePasswordBtn: document.getElementById('dropdownChangePasswordBtn'),
    dropdownSignOutBtn: document.getElementById('dropdownSignOutBtn'),

    // Change Username Modal
    changeUsernameModal: document.getElementById('changeUsernameModal'),
    changeUsernameForm: document.getElementById('changeUsernameForm'),
    closeChangeUsernameModalBtn: document.getElementById('closeChangeUsernameModalBtn'),
    cancelChangeUsernameBtn: document.getElementById('cancelChangeUsernameBtn'),
    saveChangeUsernameBtn: document.getElementById('saveChangeUsernameBtn'),
    changeUsernameErrorAlert: document.getElementById('changeUsernameErrorAlert'),
    changeUsernameErrorMessage: document.getElementById('changeUsernameErrorMessage'),
    displayCurrentUsername: document.getElementById('displayCurrentUsername'),
    newUsernameInput: document.getElementById('newUsernameInput'),
    authPasswordForUsernameInput: document.getElementById('authPasswordForUsernameInput'),

    // Change Password Modal
    changePasswordModal: document.getElementById('changePasswordModal'),
    changePasswordForm: document.getElementById('changePasswordForm'),
    closeChangePasswordModalBtn: document.getElementById('closeChangePasswordModalBtn'),
    cancelChangePasswordBtn: document.getElementById('cancelChangePasswordBtn'),
    saveChangePasswordBtn: document.getElementById('saveChangePasswordBtn'),
    changePasswordErrorAlert: document.getElementById('changePasswordErrorAlert'),
    changePasswordErrorMessage: document.getElementById('changePasswordErrorMessage'),
    currentPasswordInput: document.getElementById('currentPasswordInput'),
    newPasswordInput: document.getElementById('newPasswordInput'),
    confirmPasswordInput: document.getElementById('confirmPasswordInput')
  };

  // --- Authentication Views & Handlers ---
  function showLoginView(errorMessage = null) {
    closeProfileDropdown();
    closeChangePasswordModal();
    closeChangeUsernameModal();
    closeMobileSidebar();
    state.isAuthenticated = false;
    state.currentUser = null;
    if (dom.loginContainer) dom.loginContainer.style.display = 'flex';
    if (dom.appLayout) dom.appLayout.style.display = 'none';
    if (dom.loginErrorAlert && dom.loginErrorMessage) {
      if (errorMessage) {
        dom.loginErrorMessage.textContent = errorMessage;
        dom.loginErrorAlert.style.display = 'flex';
      } else {
        dom.loginErrorAlert.style.display = 'none';
      }
    }
    if (dom.loginPassword) dom.loginPassword.value = '';
    if (dom.loginUsername) {
      setTimeout(() => dom.loginUsername.focus(), 50);
    }
  }

  function showAppView(user) {
    state.isAuthenticated = true;
    state.currentUser = user || { username: 'admin', role: 'Administrator' };
    if (dom.loginContainer) dom.loginContainer.style.display = 'none';
    if (dom.appLayout) dom.appLayout.style.display = 'flex';
    const username = state.currentUser.username || 'admin';
    const role = state.currentUser.role || 'Administrator';
    if (dom.headerAdminName) {
      dom.headerAdminName.textContent = username;
    }
    if (dom.dropdownUsername) {
      dom.dropdownUsername.textContent = username;
    }
    if (dom.dropdownRole) {
      dom.dropdownRole.textContent = role;
    }
    if (dom.displayCurrentUsername) {
      dom.displayCurrentUsername.value = username;
    }
  }

  function togglePasswordVisibility(targetInputId, triggerBtn) {
    let input = null;
    let icon = null;

    if (typeof targetInputId === 'string') {
      input = document.getElementById(targetInputId);
      icon = triggerBtn ? triggerBtn.querySelector('svg') : null;
    } else if (triggerBtn && triggerBtn.dataset && triggerBtn.dataset.target) {
      input = document.getElementById(triggerBtn.dataset.target);
      icon = triggerBtn.querySelector('svg');
    } else {
      input = dom.loginPassword;
      icon = dom.eyeIcon;
    }

    if (!input) return;
    const isPassword = input.getAttribute('type') === 'password';
    input.setAttribute('type', isPassword ? 'text' : 'password');

    if (icon) {
      if (isPassword) {
        icon.innerHTML = `
          <path d="M9.88 9.88a3 3 0 1 0 4.24 4.24"/>
          <path d="M10.73 5.08A10.43 10.43 0 0 1 12 5c7 0 10 7 10 7a13.16 13.16 0 0 1-1.67 2.68"/>
          <path d="M6.61 6.61A13.526 13.526 0 0 0 2 12s3 7 10 7a9.74 9.74 0 0 0 5.39-1.61"/>
          <line x1="2" x2="22" y1="2" y2="22"/>
        `;
      } else {
        icon.innerHTML = `
          <path d="M2 12s3-7 10-7 10 7 10 7-3 7-10 7-10-7-10-7Z"/>
          <circle cx="12" cy="12" r="3"/>
        `;
      }
    }
  }

  // --- Profile Dropdown Controls ---
  function toggleProfileDropdown(e) {
    if (e) e.stopPropagation();
    if (!dom.profileDropdown) return;
    const isVisible = dom.profileDropdown.style.display === 'block';
    if (isVisible) {
      closeProfileDropdown();
    } else {
      openProfileDropdown();
    }
  }

  function openProfileDropdown() {
    if (!dom.profileDropdown) return;
    dom.profileDropdown.style.display = 'block';
    if (dom.profileAvatarBtn) {
      dom.profileAvatarBtn.setAttribute('aria-expanded', 'true');
    }
  }

  function closeProfileDropdown() {
    if (!dom.profileDropdown) return;
    dom.profileDropdown.style.display = 'none';
    if (dom.profileAvatarBtn) {
      dom.profileAvatarBtn.setAttribute('aria-expanded', 'false');
    }
  }

  // --- Change Password Modal Controls ---
  function openChangePasswordModal() {
    closeProfileDropdown();
    if (!dom.changePasswordModal) return;
    if (dom.changePasswordForm) dom.changePasswordForm.reset();
    if (dom.changePasswordErrorAlert) dom.changePasswordErrorAlert.style.display = 'none';
    dom.changePasswordModal.classList.add('active');
    setTimeout(() => {
      if (dom.currentPasswordInput) dom.currentPasswordInput.focus();
    }, 100);
  }

  function closeChangePasswordModal() {
    if (!dom.changePasswordModal) return;
    dom.changePasswordModal.classList.remove('active');
    if (dom.changePasswordForm) dom.changePasswordForm.reset();
    if (dom.changePasswordErrorAlert) dom.changePasswordErrorAlert.style.display = 'none';
  }

  async function handleChangePasswordSubmit(e) {
    e.preventDefault();
    const currentPassword = dom.currentPasswordInput ? dom.currentPasswordInput.value : '';
    const newPassword = dom.newPasswordInput ? dom.newPasswordInput.value : '';
    const confirmPassword = dom.confirmPasswordInput ? dom.confirmPasswordInput.value : '';

    if (!currentPassword || !newPassword || !confirmPassword) {
      showChangePasswordError('All password fields are required.');
      return;
    }

    if (newPassword !== confirmPassword) {
      showChangePasswordError('New password and confirmation do not match.');
      return;
    }

    if (newPassword.length < 8) {
      showChangePasswordError('New password must be at least 8 characters long.');
      return;
    }

    const hasLetter = /[a-zA-Z]/.test(newPassword);
    const hasDigitOrSymbol = /[0-9!@#$%^&*()_+\-=\[\]{};':"\\|,.<>\/?]/.test(newPassword);
    if (!hasLetter || !hasDigitOrSymbol) {
      showChangePasswordError('New password must contain at least one letter and one number or special character.');
      return;
    }

    if (newPassword === currentPassword) {
      showChangePasswordError('New password must be different from current password.');
      return;
    }

    if (dom.saveChangePasswordBtn) {
      dom.saveChangePasswordBtn.disabled = true;
      dom.saveChangePasswordBtn.innerHTML = '<span>Saving Changes...</span>';
    }
    if (dom.changePasswordErrorAlert) dom.changePasswordErrorAlert.style.display = 'none';

    try {
      await API.changePassword(currentPassword, newPassword, confirmPassword);
      closeChangePasswordModal();
      showToast('Password changed successfully. Please sign in with your new password.', 'success');
      showLoginView('Password changed successfully. Please sign in with your new password.');
    } catch (err) {
      showChangePasswordError(err.message || 'Failed to change password.');
      if (dom.currentPasswordInput) dom.currentPasswordInput.focus();
    } finally {
      if (dom.saveChangePasswordBtn) {
        dom.saveChangePasswordBtn.disabled = false;
        dom.saveChangePasswordBtn.innerHTML = '<span>Save Changes</span>';
      }
    }
  }

  function showChangePasswordError(msg) {
    if (dom.changePasswordErrorAlert && dom.changePasswordErrorMessage) {
      dom.changePasswordErrorMessage.textContent = msg;
      dom.changePasswordErrorAlert.style.display = 'flex';
    } else {
      alert(msg);
    }
  }

  // --- Change Username Modal Controls ---
  function openChangeUsernameModal() {
    closeProfileDropdown();
    if (!dom.changeUsernameModal) return;
    if (dom.changeUsernameForm) dom.changeUsernameForm.reset();
    if (dom.changeUsernameErrorAlert) dom.changeUsernameErrorAlert.style.display = 'none';
    if (dom.displayCurrentUsername) {
      dom.displayCurrentUsername.value = state.currentUser ? (state.currentUser.username || 'admin') : 'admin';
    }
    dom.changeUsernameModal.classList.add('active');
    setTimeout(() => {
      if (dom.newUsernameInput) dom.newUsernameInput.focus();
    }, 100);
  }

  function closeChangeUsernameModal() {
    if (!dom.changeUsernameModal) return;
    dom.changeUsernameModal.classList.remove('active');
    if (dom.changeUsernameForm) dom.changeUsernameForm.reset();
    if (dom.changeUsernameErrorAlert) dom.changeUsernameErrorAlert.style.display = 'none';
  }

  function showChangeUsernameError(msg) {
    if (dom.changeUsernameErrorAlert && dom.changeUsernameErrorMessage) {
      dom.changeUsernameErrorMessage.textContent = msg;
      dom.changeUsernameErrorAlert.style.display = 'flex';
    } else {
      alert(msg);
    }
  }

  async function handleChangeUsernameSubmit(e) {
    e.preventDefault();
    const currentUsername = state.currentUser ? (state.currentUser.username || 'admin') : 'admin';
    const newUsername = dom.newUsernameInput ? dom.newUsernameInput.value.trim() : '';
    const currentPassword = dom.authPasswordForUsernameInput ? dom.authPasswordForUsernameInput.value : '';

    if (!newUsername || !currentPassword) {
      showChangeUsernameError('Both new username and current password are required.');
      return;
    }

    if (newUsername.length < 3 || newUsername.length > 32) {
      showChangeUsernameError('Username must be between 3 and 32 characters long.');
      return;
    }

    if (!/^[a-zA-Z0-9_.\-]+$/.test(newUsername)) {
      showChangeUsernameError('Username can only contain alphanumeric characters, underscores, hyphens, and dots.');
      return;
    }

    if (newUsername.toLowerCase() === currentUsername.toLowerCase()) {
      showChangeUsernameError('New username must be different from current username.');
      return;
    }

    if (dom.saveChangeUsernameBtn) {
      dom.saveChangeUsernameBtn.disabled = true;
      dom.saveChangeUsernameBtn.innerHTML = '<span>Saving Changes...</span>';
    }
    if (dom.changeUsernameErrorAlert) dom.changeUsernameErrorAlert.style.display = 'none';

    try {
      const res = await API.changeUsername(currentPassword, newUsername);
      const updatedUsername = (res.user && res.user.username) ? res.user.username : newUsername;
      if (state.currentUser) {
        state.currentUser.username = updatedUsername;
      }
      if (dom.headerAdminName) dom.headerAdminName.textContent = updatedUsername;
      if (dom.dropdownUsername) dom.dropdownUsername.textContent = updatedUsername;
      if (dom.displayCurrentUsername) dom.displayCurrentUsername.value = updatedUsername;

      closeChangeUsernameModal();
      showToast(`Username successfully updated to "${updatedUsername}".`, 'success');
    } catch (err) {
      showChangeUsernameError(err.message || 'Failed to update username.');
      if (dom.authPasswordForUsernameInput) {
        dom.authPasswordForUsernameInput.value = '';
        dom.authPasswordForUsernameInput.focus();
      }
    } finally {
      if (dom.saveChangeUsernameBtn) {
        dom.saveChangeUsernameBtn.disabled = false;
        dom.saveChangeUsernameBtn.innerHTML = '<span>Save Changes</span>';
      }
    }
  }

  // --- Sidebar Collapse & Drawer Controls ---
  function initSidebar() {
    const isMobile = window.innerWidth <= 768;
    if (!isMobile) {
      const savedCollapsed = localStorage.getItem('stockflow_sidebar_collapsed');
      if (savedCollapsed === 'true' && dom.appLayout) {
        dom.appLayout.classList.add('sidebar-collapsed');
      }
    }
  }

  function toggleSidebar() {
    if (!dom.appLayout) return;
    const isMobile = window.innerWidth <= 768;
    if (isMobile) {
      const isOpen = dom.appLayout.classList.contains('sidebar-open');
      if (isOpen) {
        dom.appLayout.classList.remove('sidebar-open');
      } else {
        dom.appLayout.classList.add('sidebar-open');
      }
    } else {
      const isCollapsed = dom.appLayout.classList.contains('sidebar-collapsed');
      if (isCollapsed) {
        dom.appLayout.classList.remove('sidebar-collapsed');
        localStorage.setItem('stockflow_sidebar_collapsed', 'false');
      } else {
        dom.appLayout.classList.add('sidebar-collapsed');
        localStorage.setItem('stockflow_sidebar_collapsed', 'true');
      }
    }
  }

  function closeMobileSidebar() {
    if (dom.appLayout && dom.appLayout.classList.contains('sidebar-open')) {
      dom.appLayout.classList.remove('sidebar-open');
    }
  }

  // --- Sales CSV Export ---
  function handleExportSalesCsv() {
    if (!state.isAuthenticated) {
      showToast('You must be signed in to export sales records.', 'error');
      return;
    }
    showToast('Exporting sales records to CSV...', 'info');
    window.location.href = '/api/sales/export-csv';
  }

  async function handleLoginFormSubmit(e) {
    e.preventDefault();
    const username = dom.loginUsername ? dom.loginUsername.value.trim() : '';
    const password = dom.loginPassword ? dom.loginPassword.value : '';

    if (!username || !password) {
      showLoginError('Please enter both username and password.');
      return;
    }

    if (dom.loginSubmitBtn) {
      dom.loginSubmitBtn.disabled = true;
      dom.loginSubmitBtn.innerHTML = '<span>Signing In...</span>';
    }
    if (dom.loginErrorAlert) dom.loginErrorAlert.style.display = 'none';

    try {
      const res = await API.login(username, password);
      showAppView(res.user);
      showToast(`Welcome back, ${res.user.username}!`, 'success');

      // Refresh products and sales data
      await refreshData();
      const initialPage = window.location.hash.replace('#', '') || 'dashboard';
      navigateTo(initialPage);
    } catch (err) {
      showLoginError(err.message || 'Invalid username or password.');
      if (dom.loginPassword) {
        dom.loginPassword.value = '';
        dom.loginPassword.focus();
      }
    } finally {
      if (dom.loginSubmitBtn) {
        dom.loginSubmitBtn.disabled = false;
        dom.loginSubmitBtn.innerHTML = '<span>Sign In</span>';
      }
    }
  }

  function showLoginError(msg) {
    if (dom.loginErrorAlert && dom.loginErrorMessage) {
      dom.loginErrorMessage.textContent = msg;
      dom.loginErrorAlert.style.display = 'flex';
    } else {
      alert(msg);
    }
  }

  async function handleSignOut(e) {
    if (e) e.preventDefault();
    try {
      await API.logout();
    } catch (err) {
      console.error('Logout error:', err);
    }
    state.cart = [];
    state.products = [];
    state.sales = [];
    showLoginView();
    showToast('Signed out of Stock Flow session.', 'info');
  }

  // --- Router / Page Switcher ---
  function navigateTo(pageId) {
    if (!state.isAuthenticated) {
      showLoginView();
      return;
    }
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

    // 4. Low stock count (1 to 10 units)
    const lowStockCount = state.products.filter(p => Number(p.stock) > 0 && Number(p.stock) <= 10).length;
    if (dom.dashLowStockCount) dom.dashLowStockCount.textContent = lowStockCount;

    // 5. Out of stock count (0 units)
    const outOfStockCount = state.products.filter(p => Number(p.stock) <= 0).length;
    if (dom.dashOutOfStockCount) dom.dashOutOfStockCount.textContent = outOfStockCount;

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
        const isLowStock = !isOutOfStock && product.stock <= 10;
        const card = document.createElement('div');
        card.className = `product-card ${isOutOfStock ? 'out-of-stock' : ''}`;
        card.innerHTML = `
          <div>
            <div class="product-card-title" title="${product.name}">${product.name}</div>
            <div class="product-card-meta">
              <span class="product-card-stock">
                Stock: ${product.stock}
                ${isLowStock ? '<span class="badge badge-low-stock" style="padding: 1px 6px; font-size: 11px; margin-left: 4px;">Low</span>' : ''}
              </span>
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
      dom.posCartTotal.textContent = '₹0.00';
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
          <td colspan="6" class="text-center" style="padding: 28px; color: var(--text-muted);">
            No products found matching "${keyword}".
          </td>
        </tr>`;
      return;
    }

    filtered.forEach(product => {
      const tr = document.createElement('tr');
      const isOut = product.stock <= 0;
      let statusBadge = '';
      if (isOut) {
        statusBadge = '<span class="badge badge-out-of-stock">Out of Stock</span>';
      } else if (product.stock <= 10) {
        statusBadge = `<span class="badge badge-low-stock">Low Stock (${product.stock})</span>`;
      } else {
        statusBadge = '<span class="badge badge-in-stock">In Stock</span>';
      }

      tr.innerHTML = `
        <td class="font-medium">${formatProductId(product.id)}</td>
        <td class="font-bold">${product.name}</td>
        <td>${formatCurrency(product.price)}</td>
        <td>${product.stock}</td>
        <td>${statusBadge}</td>
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
          <td colspan="7" class="text-center" style="padding: 32px; color: var(--text-muted);">
            No sales recorded yet. Process new sales from the POS tab.
          </td>
        </tr>`;
      return;
    }

    sorted.forEach(sale => {
      const tr = document.createElement('tr');
      const itemCount = (sale.items && sale.items.length) ? sale.items.length : 0;
      const customerName = (sale.customerName && sale.customerName.trim()) 
        ? sale.customerName.trim() 
        : 'Walk-in Customer';
      tr.innerHTML = `
        <td class="font-medium">${escapeHtml(sale.billId || 'INV-0000')}</td>
        <td style="color: var(--text-muted);">${escapeHtml(sale.timestamp || 'N/A')}</td>
        <td>${escapeHtml(customerName)}</td>
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

  // Helper to resolve customer name from POS input with sensible fallback
  function getPosCustomerName() {
    const raw = dom.posCustomerNameInput ? dom.posCustomerNameInput.value.trim() : '';
    if (raw) return raw;
    const selected = dom.posCustomerSelect ? dom.posCustomerSelect.value.trim() : '';
    return selected || 'Walk-in Customer';
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

    dom.modalCustomerName.textContent = getPosCustomerName();
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

    const customerName = getPosCustomerName();
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

      // Clear cart and reset customer input so it is never accidentally reused
      state.cart = [];
      if (dom.posCustomerNameInput) {
        dom.posCustomerNameInput.value = '';
      }
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
    // Strictly preserve original transaction timestamp; fallback to 'N/A' without inventing historical dates
    dom.receiptDate.textContent = bill.timestamp || 'N/A';
    dom.receiptCustomer.textContent = (bill.customerName && bill.customerName.trim()) 
      ? bill.customerName.trim() 
      : 'Walk-in Customer';
    dom.receiptMethod.textContent = bill.paymentMethod || 'Cash';

    dom.receiptItemsBody.innerHTML = '';
    if (bill.items && bill.items.length) {
      bill.items.forEach(item => {
        const tr = document.createElement('tr');
        tr.innerHTML = `
          <td>${escapeHtml(item.productName || 'Item')}</td>
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
          closeMobileSidebar();
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

    // Admin Login Form & Password Toggle
    if (dom.loginForm) {
      dom.loginForm.addEventListener('submit', handleLoginFormSubmit);
    }
    if (dom.togglePasswordBtn) {
      dom.togglePasswordBtn.addEventListener('click', togglePasswordVisibility);
    }

    // Sign Out button (sidebar)
    if (dom.signOutBtn) {
      dom.signOutBtn.addEventListener('click', handleSignOut);
    }

    // Sidebar Collapse / Expand Toggle
    if (dom.sidebarToggleBtn) {
      dom.sidebarToggleBtn.addEventListener('click', toggleSidebar);
    }
    if (dom.sidebarBackdrop) {
      dom.sidebarBackdrop.addEventListener('click', closeMobileSidebar);
    }

    // Profile Menu Trigger & Dropdown Actions
    if (dom.profileAvatarBtn) {
      dom.profileAvatarBtn.addEventListener('click', toggleProfileDropdown);
    }
    if (dom.dropdownChangeUsernameBtn) {
      dom.dropdownChangeUsernameBtn.addEventListener('click', (e) => {
        e.preventDefault();
        openChangeUsernameModal();
      });
    }
    if (dom.dropdownChangePasswordBtn) {
      dom.dropdownChangePasswordBtn.addEventListener('click', (e) => {
        e.preventDefault();
        openChangePasswordModal();
      });
    }
    if (dom.dropdownSignOutBtn) {
      dom.dropdownSignOutBtn.addEventListener('click', (e) => {
        e.preventDefault();
        handleSignOut(e);
      });
    }

    // Close Profile Dropdown when clicking outside
    document.addEventListener('click', (e) => {
      if (dom.profileMenuContainer && !dom.profileMenuContainer.contains(e.target)) {
        closeProfileDropdown();
      }
    });

    // Close Dropdown & Modals on Escape key
    document.addEventListener('keydown', (e) => {
      if (e.key === 'Escape' || e.key === 'Esc') {
        closeProfileDropdown();
        closeChangePasswordModal();
        closeChangeUsernameModal();
        closeMobileSidebar();
      }
    });

    // Password visibility toggles across login and modals
    document.addEventListener('click', (e) => {
      const toggleBtn = e.target.closest('.btn-toggle-password');
      if (toggleBtn) {
        e.preventDefault();
        const targetId = toggleBtn.dataset.target || (toggleBtn.id === 'togglePasswordBtn' ? 'loginPassword' : null);
        if (targetId) {
          togglePasswordVisibility(targetId, toggleBtn);
        }
      }
    });

    // Change Username Modal events
    if (dom.closeChangeUsernameModalBtn) {
      dom.closeChangeUsernameModalBtn.addEventListener('click', closeChangeUsernameModal);
    }
    if (dom.cancelChangeUsernameBtn) {
      dom.cancelChangeUsernameBtn.addEventListener('click', closeChangeUsernameModal);
    }
    if (dom.changeUsernameModal) {
      dom.changeUsernameModal.addEventListener('click', (e) => {
        if (e.target === dom.changeUsernameModal) {
          closeChangeUsernameModal();
        }
      });
    }
    if (dom.changeUsernameForm) {
      dom.changeUsernameForm.addEventListener('submit', handleChangeUsernameSubmit);
    }

    // Change Password Modal events
    if (dom.closeChangePasswordModalBtn) {
      dom.closeChangePasswordModalBtn.addEventListener('click', closeChangePasswordModal);
    }
    if (dom.cancelChangePasswordBtn) {
      dom.cancelChangePasswordBtn.addEventListener('click', closeChangePasswordModal);
    }
    if (dom.changePasswordModal) {
      dom.changePasswordModal.addEventListener('click', (e) => {
        if (e.target === dom.changePasswordModal) {
          closeChangePasswordModal();
        }
      });
    }
    if (dom.changePasswordForm) {
      dom.changePasswordForm.addEventListener('submit', handleChangePasswordSubmit);
    }

    // Sales CSV Export button
    if (dom.exportSalesCsvBtn) {
      dom.exportSalesCsvBtn.addEventListener('click', handleExportSalesCsv);
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
    initSidebar();
    bindEvents();

    try {
      const authStatus = await API.checkAuth();
      if (authStatus && authStatus.authenticated) {
        showAppView(authStatus.user);
        const initialPage = window.location.hash.replace('#', '') || 'dashboard';
        navigateTo(initialPage);
        await refreshData();
      } else {
        showLoginView();
      }
    } catch (err) {
      console.warn('Initial session check failed:', err);
      showLoginView();
    }
  }

  // Start app on DOM ready
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }

})();
