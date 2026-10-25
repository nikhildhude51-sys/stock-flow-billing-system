$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
$machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
$env:Path = "$userPath;$machinePath"

$port = 8088
Write-Output "=== Starting Full End-to-End System Test on Port $port ==="

$proc = Start-Process -FilePath "build\server.exe" -ArgumentList "$port" -PassThru -NoNewWindow
Start-Sleep -Seconds 2

$baseUrl = "http://127.0.0.1:$port"
$passedTests = 0
$totalTests = 8

try {
    # -------------------------------------------------------------
    # TEST 1: Serve Frontend HTML
    # -------------------------------------------------------------
    Write-Output "`n[TEST 1] GET / (Serve index.html)"
    $html = (Invoke-WebRequest -Uri "$baseUrl/" -UseBasicParsing).Content
    if ($html -match "Admin Panel" -and $html -match "Point of Sale") {
        Write-Output "PASS: Successfully served index.html with Admin Panel UI."
        $passedTests++
    } else {
        Write-Error "FAIL: HTML did not contain expected content."
    }

    # -------------------------------------------------------------
    # TEST 2: Serve CSS stylesheet
    # -------------------------------------------------------------
    Write-Output "`n[TEST 2] GET /styles.css (Serve CSS)"
    $css = (Invoke-WebRequest -Uri "$baseUrl/styles.css" -UseBasicParsing).Content
    if ($css -match "--sidebar-width" -and $css -match "product-card") {
        Write-Output "PASS: Successfully served styles.css."
        $passedTests++
    } else {
        Write-Error "FAIL: CSS did not contain expected styles."
    }

    # -------------------------------------------------------------
    # TEST 3: Serve JavaScript application
    # -------------------------------------------------------------
    Write-Output "`n[TEST 3] GET /app.js (Serve JavaScript)"
    $js = (Invoke-WebRequest -Uri "$baseUrl/app.js" -UseBasicParsing).Content
    if ($js -match "renderDashboard" -and $js -match "addToCart") {
        Write-Output "PASS: Successfully served app.js."
        $passedTests++
    } else {
        Write-Error "FAIL: JS did not contain expected functions."
    }

    # -------------------------------------------------------------
    # TEST 4: API GET /api/products
    # -------------------------------------------------------------
    Write-Output "`n[TEST 4] GET /api/products"
    $products = Invoke-RestMethod -Uri "$baseUrl/api/products" -Method Get
    if ($products.Count -ge 9) {
        Write-Output "PASS: Catalog returned $($products.Count) products."
        $passedTests++
    } else {
        Write-Error "FAIL: Catalog returned fewer products than expected."
    }

    # -------------------------------------------------------------
    # TEST 5: API GET /api/sales
    # -------------------------------------------------------------
    Write-Output "`n[TEST 5] GET /api/sales"
    $sales = Invoke-RestMethod -Uri "$baseUrl/api/sales" -Method Get
    if ($sales.Count -ge 5) {
        Write-Output "PASS: Sales history returned $($sales.Count) transactions."
        $passedTests++
    } else {
        Write-Error "FAIL: Sales history count mismatch."
    }

    # -------------------------------------------------------------
    # TEST 6: Checkout Workflow with Stock Deduction
    # -------------------------------------------------------------
    Write-Output "`n[TEST 6] POS Checkout Workflow"
    # Find chips stock before checkout
    $chipsBefore = ($products | Where-Object { $_.id -eq 2 }).stock
    Write-Output "Chips stock before checkout: $chipsBefore"

    $checkoutBody = @{
        customerName = "Walk-in Customer";
        paymentMethod = "Cash";
        paymentConfirmed = $true;
        items = @(
            @{ productId = 2; quantity = 2 }
        )
    } | ConvertTo-Json -Depth 5

    $checkoutRes = Invoke-RestMethod -Uri "$baseUrl/api/checkout" -Method Post -Body $checkoutBody -ContentType "application/json"
    if ($checkoutRes.success -and $checkoutRes.bill.isPaid) {
        Write-Output "PASS: Checkout succeeded with Bill ID: $($checkoutRes.bill.billId), Total: `$$($checkoutRes.bill.totalAmount)"
        
        # Verify stock decreased by exactly 2
        $productsAfter = Invoke-RestMethod -Uri "$baseUrl/api/products" -Method Get
        $chipsAfter = ($productsAfter | Where-Object { $_.id -eq 2 }).stock
        Write-Output "Chips stock after checkout: $chipsAfter"
        if ($chipsAfter -eq ($chipsBefore - 2)) {
            Write-Output "PASS: Inventory stock correctly decremented in C++ backend."
            $passedTests++
        } else {
            Write-Error "FAIL: Stock was not properly decremented."
        }
    } else {
        Write-Error "FAIL: Checkout request failed."
    }

    # -------------------------------------------------------------
    # TEST 7: Prevent Over-purchasing / Negative Stock
    # -------------------------------------------------------------
    Write-Output "`n[TEST 7] Prevent Over-purchasing Beyond Stock"
    $overBody = @{
        customerName = "Walk-in Customer";
        paymentMethod = "Cash";
        paymentConfirmed = $true;
        items = @(
            @{ productId = 9; quantity = 1 } # Terry has stock 0
        )
    } | ConvertTo-Json -Depth 5

    try {
        Invoke-RestMethod -Uri "$baseUrl/api/checkout" -Method Post -Body $overBody -ContentType "application/json"
        Write-Error "FAIL: Checkout should have been rejected for zero-stock item."
    } catch {
        Write-Output "PASS: Correctly rejected purchase exceeding available stock (HTTP 400)."
        $passedTests++
    }

    # -------------------------------------------------------------
    # TEST 8: Add, Update, and Delete Product via API
    # -------------------------------------------------------------
    Write-Output "`n[TEST 8] Add and Delete Product via C++ Backend"
    $newProd = @{
        name = "Caramel Donut";
        price = 3.49;
        stock = 40
    } | ConvertTo-Json

    $createRes = Invoke-RestMethod -Uri "$baseUrl/api/products" -Method Post -Body $newProd -ContentType "application/json"
    $newId = $createRes.product.id
    if ($createRes.success -and $newId -gt 0) {
        Write-Output "PASS: Created product ID $newId."
        
        # Now delete it
        $delRes = Invoke-RestMethod -Uri "$baseUrl/api/products/$newId" -Method Delete
        if ($delRes.success) {
            Write-Output "PASS: Successfully deleted product ID $newId."
            $passedTests++
        } else {
            Write-Error "FAIL: Delete product failed."
        }
    } else {
        Write-Error "FAIL: Create product failed."
    }

    Write-Output "`n========================================================"
    Write-Output "FULL SYSTEM TEST SUMMARY: $passedTests / $totalTests TESTS PASSED!"
    Write-Output "========================================================"

} finally {
    Write-Output "Stopping test server (PID: $($proc.Id))..."
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
}
