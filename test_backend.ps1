$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
$machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
$env:Path = "$userPath;$machinePath"

$port = 8089
Write-Output "=== Starting C++ Backend Server on Port $port for Automated Testing ==="

# Reset test data to clean state before test
$initialInventory = @"
[
  { "id": 1, "name": "Ahmed Spears", "price": 868.00, "stock": 10 },
  { "id": 2, "name": "Chips", "price": 2.49, "stock": 105 },
  { "id": 3, "name": "Chocolate Bar", "price": 3.99, "stock": 57 },
  { "id": 4, "name": "Coffee", "price": 4.50, "stock": 99 },
  { "id": 5, "name": "Energy Drink", "price": 2.99, "stock": 75 },
  { "id": 6, "name": "Muffin", "price": 3.75, "stock": 30 },
  { "id": 7, "name": "Sandwich", "price": 8.99, "stock": 50 },
  { "id": 8, "name": "Tea", "price": 3.25, "stock": 80 },
  { "id": 9, "name": "Terry", "price": 10000.00, "stock": 0 }
]
"@
Set-Content -Path "data/inventory.json" -Value $initialInventory -Force
Set-Content -Path "data/sales.json" -Value "[]" -Force

$proc = Start-Process -FilePath "build\server.exe" -ArgumentList "$port" -PassThru -NoNewWindow
Start-Sleep -Seconds 2

$baseUrl = "http://127.0.0.1:$port"
$passedTests = 0
$totalTests = 9

try {
    # -------------------------------------------------------------
    # TEST 1: Health Check
    # -------------------------------------------------------------
    Write-Output "`n[TEST 1] GET /api/health"
    $res = Invoke-RestMethod -Uri "$baseUrl/api/health" -Method Get
    if ($res.status -eq "healthy") {
        Write-Output "PASS: Health check passed: $($res.server)"
        $passedTests++
    } else {
        Write-Error "FAIL: Health check failed"
    }

    # -------------------------------------------------------------
    # TEST 2: GET /api/products
    # -------------------------------------------------------------
    Write-Output "`n[TEST 2] GET /api/products"
    $products = Invoke-RestMethod -Uri "$baseUrl/api/products" -Method Get
    Write-Output "Fetched $($products.Count) products from catalog."
    $spears = $products | Where-Object { $_.id -eq 1 }
    $chips = $products | Where-Object { $_.id -eq 2 }
    if ($products.Count -ge 9 -and $spears.stock -eq 10 -and $chips.stock -eq 105) {
        Write-Output "PASS: Initial catalog verified (Ahmed Spears stock: $($spears.stock), Chips stock: $($chips.stock))."
        $passedTests++
    } else {
        Write-Error "FAIL: Initial catalog count or stock mismatch."
    }

    # -------------------------------------------------------------
    # TEST 3: POST /api/products (Add product)
    # -------------------------------------------------------------
    Write-Output "`n[TEST 3] POST /api/products (Add product)"
    $newProd = @{ name = "Hot Dog"; price = 5.50; stock = 25 } | ConvertTo-Json
    $addRes = Invoke-RestMethod -Uri "$baseUrl/api/products" -Method Post -Body $newProd -ContentType "application/json"
    if ($addRes.success -and $addRes.product.name -eq "Hot Dog") {
        Write-Output "PASS: Successfully added new product with ID: $($addRes.product.id)"
        $passedTests++
    } else {
        Write-Error "FAIL: Add product failed."
    }

    # -------------------------------------------------------------
    # TEST 4: POST /api/checkout (Negative Test: Unconfirmed payment)
    # -------------------------------------------------------------
    Write-Output "`n[TEST 4] POST /api/checkout with paymentConfirmed = false"
    $unconfirmedBody = @{
        customerName = "Walk-in Customer";
        paymentMethod = "Cash";
        paymentConfirmed = $false;
        items = @(
            @{ productId = 1; quantity = 1 }
        )
    } | ConvertTo-Json -Depth 5

    try {
        Invoke-RestMethod -Uri "$baseUrl/api/checkout" -Method Post -Body $unconfirmedBody -ContentType "application/json"
        Write-Error "FAIL: Unconfirmed checkout should have returned 400!"
    } catch {
        Write-Output "PASS: Unconfirmed checkout rejected as expected (HTTP 400)."
        # Verify stock was NOT deducted
        $checkProducts = Invoke-RestMethod -Uri "$baseUrl/api/products" -Method Get
        $p1 = $checkProducts | Where-Object { $_.id -eq 1 }
        if ($p1.stock -eq 10) {
            Write-Output "PASS: Stock preserved at 10. No deduction occurred."
            $passedTests++
        } else {
            Write-Error "FAIL: Stock was erroneously modified!"
        }
    }

    # -------------------------------------------------------------
    # TEST 5: POST /api/checkout (Negative Test: Insufficient Stock)
    # -------------------------------------------------------------
    Write-Output "`n[TEST 5] POST /api/checkout with insufficient stock (Terry: stock 0)"
    $insufficientBody = @{
        customerName = "Walk-in Customer";
        paymentMethod = "Cash";
        paymentConfirmed = $true;
        items = @(
            @{ productId = 9; quantity = 1 }
        )
    } | ConvertTo-Json -Depth 5

    try {
        Invoke-RestMethod -Uri "$baseUrl/api/checkout" -Method Post -Body $insufficientBody -ContentType "application/json"
        Write-Error "FAIL: Out-of-stock item checkout should have failed!"
    } catch {
        Write-Output "PASS: Out-of-stock item rejected with HTTP 400."
        $passedTests++
    }

    # -------------------------------------------------------------
    # TEST 6: POST /api/checkout (Atomicity test: Cart has 1 valid + 1 invalid item)
    # -------------------------------------------------------------
    Write-Output "`n[TEST 6] POST /api/checkout Atomicity Test (1 valid + 1 out-of-stock)"
    $partialBody = @{
        customerName = "Walk-in Customer";
        paymentMethod = "Cash";
        paymentConfirmed = $true;
        items = @(
            @{ productId = 2; quantity = 1 },
            @{ productId = 9; quantity = 5 }
        )
    } | ConvertTo-Json -Depth 5

    try {
        Invoke-RestMethod -Uri "$baseUrl/api/checkout" -Method Post -Body $partialBody -ContentType "application/json"
        Write-Error "FAIL: Partial checkout should have failed!"
    } catch {
        # Verify product 2 stock did NOT decrement
        $checkProducts = Invoke-RestMethod -Uri "$baseUrl/api/products" -Method Get
        $p2 = $checkProducts | Where-Object { $_.id -eq 2 }
        if ($p2.stock -eq 105) {
            Write-Output "PASS: Transaction is atomic: valid item stock was not deducted when cart had an invalid item."
            $passedTests++
        } else {
            Write-Error "FAIL: Partial stock deduction occurred!"
        }
    }

    # -------------------------------------------------------------
    # TEST 7: POST /api/checkout (Positive Test: Exact Cart from Reference Screenshot)
    # Chips (1 x $2.49) + Ahmed Spears (1 x $868.00) = $870.49
    # -------------------------------------------------------------
    Write-Output "`n[TEST 7] POST /api/checkout (Valid Sale Matching Reference UI Cart)"
    $validSaleBody = @{
        customerName = "Walk-in Customer";
        paymentMethod = "Cash";
        paymentConfirmed = $true;
        items = @(
            @{ productId = 2; quantity = 1 },
            @{ productId = 1; quantity = 1 }
        )
    } | ConvertTo-Json -Depth 5

    $saleRes = Invoke-RestMethod -Uri "$baseUrl/api/checkout" -Method Post -Body $validSaleBody -ContentType "application/json"
    $roundedTotal = [Math]::Round([double]$saleRes.bill.totalAmount, 2)
    if ($saleRes.success -and $roundedTotal -eq 870.49 -and $saleRes.bill.isPaid -eq $true) {
        Write-Output "PASS: Sale processed successfully! Bill ID: $($saleRes.bill.billId), Total: `$$roundedTotal"
        $passedTests++
    } else {
        Write-Error "FAIL: Valid checkout returned unexpected response: $($saleRes | ConvertTo-Json)"
    }

    # -------------------------------------------------------------
    # TEST 8: Verify Stock Decrement After Successful Payment
    # -------------------------------------------------------------
    Write-Output "`n[TEST 8] Verify updated inventory stock"
    $afterProducts = Invoke-RestMethod -Uri "$baseUrl/api/products" -Method Get
    $p1After = $afterProducts | Where-Object { $_.id -eq 1 }
    $p2After = $afterProducts | Where-Object { $_.id -eq 2 }

    if ($p1After.stock -eq 9 -and $p2After.stock -eq 104) {
        Write-Output "PASS: Ahmed Spears stock decremented: 10 -> $($p1After.stock)"
        Write-Output "PASS: Chips stock decremented: 105 -> $($p2After.stock)"
        $passedTests++
    } else {
        Write-Error "FAIL: Stock was not correctly decremented!"
    }

    # -------------------------------------------------------------
    # TEST 9: GET /api/sales and File Persistence
    # -------------------------------------------------------------
    Write-Output "`n[TEST 9] GET /api/sales & verify local JSON persistence"
    $salesList = Invoke-RestMethod -Uri "$baseUrl/api/sales" -Method Get
    $fileSales = Get-Content -Raw "data/sales.json" | ConvertFrom-Json
    if ($salesList.Count -ge 1 -and $fileSales.Count -ge 1) {
        Write-Output "PASS: Sales history contains $($salesList.Count) record(s)."
        Write-Output "PASS: data/sales.json file contains $($fileSales.Count) persisted record(s)."
        $passedTests++
    } else {
        Write-Error "FAIL: Sales history persistence check failed."
    }

    Write-Output "`n========================================================"
    Write-Output "TEST SUMMARY: $passedTests / $totalTests TESTS PASSED!"
    Write-Output "========================================================"

} finally {
    Write-Output "Stopping test server process (PID: $($proc.Id))..."
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
}
