# test_auth_suite.ps1
# Comprehensive automated test suite for Authentication and Protected Endpoints (PowerShell 5.1 compatible)

$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
$machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
$env:Path = "$userPath;$machinePath;C:\Windows\System32;C:\Windows\System32\WindowsPowerShell\v1.0"

$serverProc = $null

# Ensure any previous server instance is terminated
Get-Process -Name server -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500

# Backup user credentials if present, and prepare clean test credentials
$authBackup = $null
if (Test-Path "data/auth.json") {
    $authBackup = [System.IO.File]::ReadAllText("data/auth.json")
}
$testAuthJson = '{"passwordHash":"f0f08cd62488c652730b81a562ad67760f8e8700b3ab46d7a05f5d9eb0bf6fe3","salt":"f97f276c4f54c7d61a55eb23e9016bac","username":"admin"}'
[System.IO.File]::WriteAllText("data/auth.json", $testAuthJson)

try {
    Write-Host "==========================================================" -ForegroundColor Cyan
    Write-Host " STARTING INVENTORY & BILLING SYSTEM AUTHENTICATION SUITE" -ForegroundColor Cyan
    Write-Host "==========================================================" -ForegroundColor Cyan

    # Start server
    Write-Host "`n[1] Starting server.exe in background..." -ForegroundColor Yellow
    $serverProc = Start-Process -FilePath ".\build\server.exe" -ArgumentList "8089" -PassThru -NoNewWindow
    Start-Sleep -Seconds 3

    $baseUrl = "http://127.0.0.1:8089"
    $passed = 0
    $failed = 0

    function Assert-Test($name, $condition, $details = "") {
        if ($condition) {
            Write-Host "  [PASS] $name" -ForegroundColor Green
            $script:passed++
        } else {
            Write-Host "  [FAIL] $name - $details" -ForegroundColor Red
            $script:failed++
        }
    }

    function Invoke-Http {
        param(
            [string]$Uri,
            [string]$Method = "GET",
            [string]$Body = $null,
            [hashtable]$Headers = @{},
            [Microsoft.PowerShell.Commands.WebRequestSession]$WebSession = $null
        )

        try {
            $params = @{
                Uri = $Uri
                Method = $Method
                UseBasicParsing = $true
            }
            if ($Body) {
                $params["Body"] = $Body
                $params["ContentType"] = "application/json"
            }
            if ($Headers.Count -gt 0) {
                $params["Headers"] = $Headers
            }
            if ($WebSession) {
                $params["WebSession"] = $WebSession
            }

            $res = Invoke-WebRequest @params
            return @{
                StatusCode = [int]$res.StatusCode
                Content = $res.Content
                Headers = $res.Headers
            }
        }
        catch [System.Net.WebException] {
            $resp = $_.Exception.Response
            $content = ""
            if ($_.ErrorDetails -and $_.ErrorDetails.Message) {
                $content = $_.ErrorDetails.Message
            } elseif ($resp) {
                try {
                    $stream = $resp.GetResponseStream()
                    if ($stream) {
                        $reader = New-Object System.IO.StreamReader($stream)
                        $content = $reader.ReadToEnd()
                        $reader.Close()
                    }
                } catch {}
            }
            if ($resp) {
                return @{
                    StatusCode = [int]$resp.StatusCode
                    Content = $content
                    Headers = $resp.Headers
                }
            }
            throw $_
        }
    }

    # -----------------------------------------------------------
    # Test 1: Unauthenticated access to /api/products -> 401
    # -----------------------------------------------------------
    Write-Host "`n[Test Group: Access Control]" -ForegroundColor Cyan
    $res1 = Invoke-Http -Uri "$baseUrl/api/products" -Method "GET"
    Assert-Test "Unauthenticated GET /api/products returns 401 Unauthorized" `
        ($res1.StatusCode -eq 401) "Expected 401, got $($res1.StatusCode)"

    # -----------------------------------------------------------
    # Test 2: Unauthenticated checkout -> 401
    # -----------------------------------------------------------
    $checkoutPayload = '{"customerName":"Test","paymentMethod":"Cash","items":[{"productId":1,"quantity":1}]}'
    $res2 = Invoke-Http -Uri "$baseUrl/api/checkout" -Method "POST" -Body $checkoutPayload
    Assert-Test "Unauthenticated POST /api/checkout returns 401 Unauthorized" `
        ($res2.StatusCode -eq 401) "Expected 401, got $($res2.StatusCode)"

    # -----------------------------------------------------------
    # Test 3: Unauthenticated sales history -> 401
    # -----------------------------------------------------------
    $res3 = Invoke-Http -Uri "$baseUrl/api/sales" -Method "GET"
    Assert-Test "Unauthenticated GET /api/sales returns 401 Unauthorized" `
        ($res3.StatusCode -eq 401) "Expected 401, got $($res3.StatusCode)"

    # -----------------------------------------------------------
    # Test 4: GET /api/auth/me without session -> authenticated: false
    # -----------------------------------------------------------
    $res4 = Invoke-Http -Uri "$baseUrl/api/auth/me" -Method "GET"
    $body4 = $res4.Content | ConvertFrom-Json
    Assert-Test "GET /api/auth/me returns 200 with authenticated: false" `
        ($res4.StatusCode -eq 200 -and $body4.authenticated -eq $false) "Body: $($res4.Content)"

    # -----------------------------------------------------------
    # Test 5: Invalid Login Credentials -> 401
    # -----------------------------------------------------------
    Write-Host "`n[Test Group: Credentials & Login]" -ForegroundColor Cyan
    $badLoginPayload = '{"username":"admin","password":"wrongpassword123"}'
    $res5 = Invoke-Http -Uri "$baseUrl/api/auth/login" -Method "POST" -Body $badLoginPayload
    $body5 = try { $res5.Content | ConvertFrom-Json } catch { $null }
    Assert-Test "Invalid password rejected with 401" `
        ($res5.StatusCode -eq 401) "Status: $($res5.StatusCode), Content: $($res5.Content)"

    # -----------------------------------------------------------
    # Test 6: Valid Login Credentials -> 200 + Session Cookie
    # -----------------------------------------------------------
    $webSession = New-Object Microsoft.PowerShell.Commands.WebRequestSession
    $validLoginPayload = '{"username":"admin","password":"admin123"}'
    $res6 = Invoke-Http -Uri "$baseUrl/api/auth/login" -Method "POST" -Body $validLoginPayload -WebSession $webSession
    $body6 = try { $res6.Content | ConvertFrom-Json } catch { $null }

    $cookies = $webSession.Cookies.GetCookies([System.Uri]$baseUrl)
    $sessionCookie = $cookies["session_id"]

    Assert-Test "Valid login returns 200 and user info" `
        ($res6.StatusCode -eq 200 -and $body6.success -eq $true -and $body6.user.username -eq "admin") "Body: $($res6.Content)"
    
    $sessionId = if ($sessionCookie) { $sessionCookie.Value } elseif ($body6.token) { $body6.token } else { "" }
    Assert-Test "Valid login sets session token" `
        ($sessionId.Length -ge 32) "Session token length: $($sessionId.Length)"

    # -----------------------------------------------------------
    # Test 7: GET /api/auth/me with session cookie -> authenticated: true
    # -----------------------------------------------------------
    $res7 = Invoke-Http -Uri "$baseUrl/api/auth/me" -Method "GET" -WebSession $webSession
    $body7 = try { $res7.Content | ConvertFrom-Json } catch { $null }
    Assert-Test "GET /api/auth/me with session cookie returns authenticated: true and role: Administrator" `
        ($res7.StatusCode -eq 200 -and $body7.authenticated -eq $true -and $body7.user.username -eq "admin" -and $body7.user.role -eq "Administrator") "Body: $($res7.Content)"

    # -----------------------------------------------------------
    # Test 8: Bearer token header support
    # -----------------------------------------------------------
    $bearerHeaders = @{ "Authorization" = "Bearer $sessionId" }
    $res8 = Invoke-Http -Uri "$baseUrl/api/auth/me" -Method "GET" -Headers $bearerHeaders
    $body8 = try { $res8.Content | ConvertFrom-Json } catch { $null }
    Assert-Test "Authorization: Bearer <token> accepted" `
        ($res8.StatusCode -eq 200 -and $body8.authenticated -eq $true) "Status: $($res8.StatusCode)"

    # -----------------------------------------------------------
    # Test 9: Authenticated GET /api/products
    # -----------------------------------------------------------
    Write-Host "`n[Test Group: Authenticated Business Logic]" -ForegroundColor Cyan
    $res9 = Invoke-Http -Uri "$baseUrl/api/products" -Method "GET" -WebSession $webSession
    $products = $res9.Content | ConvertFrom-Json
    Assert-Test "Authenticated GET /api/products returns product list" `
        ($res9.StatusCode -eq 200 -and $products.Count -gt 0) "Product count: $($products.Count)"

    # Dynamically select first product in catalog
    $targetProd = $products[0]
    $targetId = $targetProd.id
    $targetInitialStock = $targetProd.stock
    Write-Host "    (Target Product: '$($targetProd.name)' (ID: $targetId), Initial Stock: $targetInitialStock)" -ForegroundColor Gray

    # -----------------------------------------------------------
    # Test 10: Authenticated Checkout -> Stock Deducted
    # -----------------------------------------------------------
    $salePayload = @"
{"customerName":"Verified Buyer","paymentMethod":"Cash","paymentConfirmed":true,"items":[{"productId":$targetId,"quantity":2}]}
"@
    $res10 = Invoke-Http -Uri "$baseUrl/api/checkout" -Method "POST" -Body $salePayload -WebSession $webSession
    $checkoutResult = $res10.Content | ConvertFrom-Json

    Assert-Test "Authenticated checkout completes successfully" `
        ($res10.StatusCode -eq 200 -and $checkoutResult.success -eq $true) "Response: $($res10.Content)"

    # Verify stock of target product decreased by exactly 2
    $res11 = Invoke-Http -Uri "$baseUrl/api/products" -Method "GET" -WebSession $webSession
    $updatedProducts = $res11.Content | ConvertFrom-Json
    $targetUpdated = $updatedProducts | Where-Object { $_.id -eq $targetId }
    $targetUpdatedStock = $targetUpdated.stock
    Assert-Test "Inventory stock correctly decremented after confirmed payment" `
        ($targetUpdatedStock -eq ($targetInitialStock - 2)) "Expected $($targetInitialStock - 2), got $targetUpdatedStock"

    # -----------------------------------------------------------
    # Test 11: Authenticated GET /api/sales shows the new bill
    # -----------------------------------------------------------
    $res12 = Invoke-Http -Uri "$baseUrl/api/sales" -Method "GET" -WebSession $webSession
    $sales = $res12.Content | ConvertFrom-Json
    $newSale = $sales | Where-Object { $_.billId -eq $checkoutResult.bill.billId }
    Assert-Test "Sales history contains the newly processed bill" `
        ($newSale -ne $null -and $newSale.customerName -eq "Verified Buyer") "BillId: $($checkoutResult.bill.billId)"

    # -----------------------------------------------------------
    # Test: Customer Name Capture & Validation
    # -----------------------------------------------------------
    # Reject customer name > 100 characters
    $longName = "A" * 105
    $longNamePayload = @"
{"customerName":"$longName","paymentMethod":"Cash","paymentConfirmed":true,"items":[{"productId":$targetId,"quantity":1}]}
"@
    $resLongName = Invoke-Http -Uri "$baseUrl/api/checkout" -Method "POST" -Body $longNamePayload -WebSession $webSession
    Assert-Test "Checkout rejects customer name exceeding 100 characters (returns 400)" `
        ($resLongName.StatusCode -eq 400) "Got status: $($resLongName.StatusCode)"

    # Empty customer name defaults to Walk-in Customer
    $emptyNamePayload = @"
{"customerName":"   ","paymentMethod":"Cash","paymentConfirmed":true,"items":[{"productId":$targetId,"quantity":1}]}
"@
    $resEmptyName = Invoke-Http -Uri "$baseUrl/api/checkout" -Method "POST" -Body $emptyNamePayload -WebSession $webSession
    $emptyNameBill = ($resEmptyName.Content | ConvertFrom-Json).bill
    Assert-Test "Checkout with empty/whitespace customer name defaults to 'Walk-in Customer'" `
        ($resEmptyName.StatusCode -eq 200 -and $emptyNameBill.customerName -eq "Walk-in Customer") `
        "Got: $($emptyNameBill.customerName)"

    # Complex customer name with quotes and commas
    $complexNamePayload = @"
{"customerName":"Gupta, Rajesh \"Gold\"","paymentMethod":"UPI / Online","paymentConfirmed":true,"items":[{"productId":$targetId,"quantity":1}]}
"@
    $resComplexName = Invoke-Http -Uri "$baseUrl/api/checkout" -Method "POST" -Body $complexNamePayload -WebSession $webSession
    $complexNameBill = ($resComplexName.Content | ConvertFrom-Json).bill
    Assert-Test "Checkout captures customer name with commas and quotes" `
        ($resComplexName.StatusCode -eq 200 -and $complexNameBill.customerName -eq 'Gupta, Rajesh "Gold"') `
        "Got: $($complexNameBill.customerName)"

    # Verify complete timestamp format (YYYY-MM-DD HH:MM:SS)
    Assert-Test "Checkout generates complete timestamp with seconds" `
        ($complexNameBill.timestamp -match '^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$') `
        "Got: $($complexNameBill.timestamp)"

    # -----------------------------------------------------------
    # Test 12: Logout invalidates session
    # -----------------------------------------------------------
    Write-Host "`n[Test Group: Session Invalidation & Logout]" -ForegroundColor Cyan
    $res13 = Invoke-Http -Uri "$baseUrl/api/auth/logout" -Method "POST" -WebSession $webSession
    Assert-Test "POST /api/auth/logout returns 200" `
        ($res13.StatusCode -eq 200) "Status: $($res13.StatusCode)"

    # Check /api/auth/me after logout
    $res14 = Invoke-Http -Uri "$baseUrl/api/auth/me" -Method "GET" -WebSession $webSession
    $body14 = $res14.Content | ConvertFrom-Json
    Assert-Test "GET /api/auth/me returns authenticated: false after logout" `
        ($body14.authenticated -eq $false) "Body: $($res14.Content)"

    # Attempting to access protected API with invalidated session returns 401
    $res15 = Invoke-Http -Uri "$baseUrl/api/products" -Method "GET" -WebSession $webSession
    Assert-Test "Subsequent request with old session receives 401 Unauthorized" `
        ($res15.StatusCode -eq 401) "Status: $($res15.StatusCode)"

    # Also test old Bearer token header -> 401
    $res16 = Invoke-Http -Uri "$baseUrl/api/products" -Method "GET" -Headers $bearerHeaders
    Assert-Test "Old Bearer token receives 401 Unauthorized" `
        ($res16.StatusCode -eq 401) "Status: $($res16.StatusCode)"

    # -----------------------------------------------------------
    # Test Group: Change Password Feature
    # -----------------------------------------------------------
    Write-Host "`n[Test Group: Change Password Feature]" -ForegroundColor Cyan
    $cpSession = New-Object Microsoft.PowerShell.Commands.WebRequestSession
    $cpLogin = Invoke-Http -Uri "$baseUrl/api/auth/login" -Method "POST" -Body '{"username":"admin","password":"admin123"}' -WebSession $cpSession
    Assert-Test "Login before password change succeeds" ($cpLogin.StatusCode -eq 200)

    # 1. Unauthenticated change password -> 401
    $resCpUnauth = Invoke-Http -Uri "$baseUrl/api/auth/change-password" -Method "POST" -Body '{"currentPassword":"x","newPassword":"y","confirmPassword":"y"}'
    Assert-Test "Unauthenticated POST /api/auth/change-password returns 401" ($resCpUnauth.StatusCode -eq 401)

    # 2. Wrong current password -> 400
    $resCpWrong = Invoke-Http -Uri "$baseUrl/api/auth/change-password" -Method "POST" -Body '{"currentPassword":"wrongpass","newPassword":"NewSecurePassword123!","confirmPassword":"NewSecurePassword123!"}' -WebSession $cpSession
    Assert-Test "Change password with wrong current password returns 400" ($resCpWrong.StatusCode -eq 400)

    # 3. Mismatched confirmation -> 400
    $resCpMismatch = Invoke-Http -Uri "$baseUrl/api/auth/change-password" -Method "POST" -Body '{"currentPassword":"admin123","newPassword":"NewSecurePassword123!","confirmPassword":"DifferentPassword123!"}' -WebSession $cpSession
    Assert-Test "Change password with mismatched confirmation returns 400" ($resCpMismatch.StatusCode -eq 400)

    # 4. Short password (< 8 chars) -> 400
    $resCpShort = Invoke-Http -Uri "$baseUrl/api/auth/change-password" -Method "POST" -Body '{"currentPassword":"admin123","newPassword":"short","confirmPassword":"short"}' -WebSession $cpSession
    Assert-Test "Change password with short password returns 400" ($resCpShort.StatusCode -eq 400)

    # 5. Letters only (no digits or symbols) -> 400
    $resCpLetters = Invoke-Http -Uri "$baseUrl/api/auth/change-password" -Method "POST" -Body '{"currentPassword":"admin123","newPassword":"onlyletterslong","confirmPassword":"onlyletterslong"}' -WebSession $cpSession
    Assert-Test "Change password without digits or symbols returns 400" ($resCpLetters.StatusCode -eq 400)

    # 6. Same as current password -> 400
    $resCpSame = Invoke-Http -Uri "$baseUrl/api/auth/change-password" -Method "POST" -Body '{"currentPassword":"admin123","newPassword":"admin123","confirmPassword":"admin123"}' -WebSession $cpSession
    Assert-Test "Change password with identical password returns 400" ($resCpSame.StatusCode -eq 400)

    # 7. Valid password change -> 200
    $resCpSuccess = Invoke-Http -Uri "$baseUrl/api/auth/change-password" -Method "POST" -Body '{"currentPassword":"admin123","newPassword":"NewSecurePass2026!","confirmPassword":"NewSecurePass2026!"}' -WebSession $cpSession
    Assert-Test "Valid password change returns 200" ($resCpSuccess.StatusCode -eq 200)

    # 8. Session invalidated after password change -> 401
    $resCpOldSession = Invoke-Http -Uri "$baseUrl/api/products" -Method "GET" -WebSession $cpSession
    Assert-Test "Old session is invalidated after password change (returns 401)" ($resCpOldSession.StatusCode -eq 401)

    # 9. Old password rejected for login
    $resOldLogin = Invoke-Http -Uri "$baseUrl/api/auth/login" -Method "POST" -Body '{"username":"admin","password":"admin123"}'
    Assert-Test "Old password rejected after password change" ($resOldLogin.StatusCode -eq 401)

    # 10. New password accepted for login
    $newSession = New-Object Microsoft.PowerShell.Commands.WebRequestSession
    $resNewLogin = Invoke-Http -Uri "$baseUrl/api/auth/login" -Method "POST" -Body '{"username":"admin","password":"NewSecurePass2026!"}' -WebSession $newSession
    Assert-Test "New password accepted for login" ($resNewLogin.StatusCode -eq 200)

    # 11. Revert password back to admin123 so suite remains idempotent
    $resRevert = Invoke-Http -Uri "$baseUrl/api/auth/change-password" -Method "POST" -Body '{"currentPassword":"NewSecurePass2026!","newPassword":"admin123","confirmPassword":"admin123"}' -WebSession $newSession
    Assert-Test "Password reverted back to default for idempotency" ($resRevert.StatusCode -eq 200)

    # -----------------------------------------------------------
    # Test Group: Change Username Feature
    # -----------------------------------------------------------
    Write-Host "`n[Test Group: Change Username Feature]" -ForegroundColor Cyan
    $cuSession = New-Object Microsoft.PowerShell.Commands.WebRequestSession
    $cuLogin = Invoke-Http -Uri "$baseUrl/api/auth/login" -Method "POST" -Body '{"username":"admin","password":"admin123"}' -WebSession $cuSession
    Assert-Test "Login before username change succeeds" ($cuLogin.StatusCode -eq 200)

    # 1. Unauthenticated change username -> 401
    $resCuUnauth = Invoke-Http -Uri "$baseUrl/api/auth/change-username" -Method "POST" -Body '{"currentPassword":"admin123","newUsername":"manager"}'
    Assert-Test "Unauthenticated POST /api/auth/change-username returns 401" ($resCuUnauth.StatusCode -eq 401)

    # 2. Wrong current password -> 400
    $resCuWrongPass = Invoke-Http -Uri "$baseUrl/api/auth/change-username" -Method "POST" -Body '{"currentPassword":"wrongpassword","newUsername":"super_admin"}' -WebSession $cuSession
    Assert-Test "Change username with wrong password returns 400" ($resCuWrongPass.StatusCode -eq 400)

    # 3. Short username (< 3 chars) -> 400
    $resCuShort = Invoke-Http -Uri "$baseUrl/api/auth/change-username" -Method "POST" -Body '{"currentPassword":"admin123","newUsername":"ab"}' -WebSession $cuSession
    Assert-Test "Change username with short username returns 400" ($resCuShort.StatusCode -eq 400)

    # 4. Username with illegal characters -> 400
    $resCuInvalid = Invoke-Http -Uri "$baseUrl/api/auth/change-username" -Method "POST" -Body '{"currentPassword":"admin123","newUsername":"bad username!"}' -WebSession $cuSession
    Assert-Test "Change username with illegal characters returns 400" ($resCuInvalid.StatusCode -eq 400)

    # 5. Identical username -> 400
    $resCuSame = Invoke-Http -Uri "$baseUrl/api/auth/change-username" -Method "POST" -Body '{"currentPassword":"admin123","newUsername":"admin"}' -WebSession $cuSession
    Assert-Test "Change username with identical username returns 400" ($resCuSame.StatusCode -eq 400)

    # 6. Valid username change -> 200
    $resCuSuccess = Invoke-Http -Uri "$baseUrl/api/auth/change-username" -Method "POST" -Body '{"currentPassword":"admin123","newUsername":"stock_master"}' -WebSession $cuSession
    Assert-Test "Valid username change returns 200" ($resCuSuccess.StatusCode -eq 200)

    # 7. Active session reflects new username
    $resCuMe = Invoke-Http -Uri "$baseUrl/api/auth/me" -Method "GET" -WebSession $cuSession
    $bodyCuMe = try { $resCuMe.Content | ConvertFrom-Json } catch { $null }
    Assert-Test "GET /api/auth/me reflects updated username" `
        ($resCuMe.StatusCode -eq 200 -and $bodyCuMe.user.username -eq "stock_master")

    # 8. Old username cannot login
    $resOldUserLogin = Invoke-Http -Uri "$baseUrl/api/auth/login" -Method "POST" -Body '{"username":"admin","password":"admin123"}'
    Assert-Test "Login with old username rejected" ($resOldUserLogin.StatusCode -eq 401)

    # 9. New username logs in with same password
    $newUserSession = New-Object Microsoft.PowerShell.Commands.WebRequestSession
    $resNewUserLogin = Invoke-Http -Uri "$baseUrl/api/auth/login" -Method "POST" -Body '{"username":"stock_master","password":"admin123"}' -WebSession $newUserSession
    Assert-Test "Login with new username and preserved password succeeds" ($resNewUserLogin.StatusCode -eq 200)

    # 10. Revert username back to admin for suite idempotency
    $resRevertUser = Invoke-Http -Uri "$baseUrl/api/auth/change-username" -Method "POST" -Body '{"currentPassword":"admin123","newUsername":"admin"}' -WebSession $newUserSession
    Assert-Test "Username reverted back to admin for idempotency" ($resRevertUser.StatusCode -eq 200)

    # -----------------------------------------------------------
    # Test Group: Sales CSV Export Feature
    # -----------------------------------------------------------
    Write-Host "`n[Test Group: Sales CSV Export Feature]" -ForegroundColor Cyan

    # 1. Unauthenticated CSV export -> 401
    $resCsvUnauth = Invoke-Http -Uri "$baseUrl/api/sales/export-csv" -Method "GET"
    Assert-Test "Unauthenticated GET /api/sales/export-csv returns 401 Unauthorized" ($resCsvUnauth.StatusCode -eq 401)

    # 2. Authenticated CSV export -> 200, Content-Type text/csv, contains valid headers
    $resCsvAuth = Invoke-Http -Uri "$baseUrl/api/sales/export-csv" -Method "GET" -WebSession $newUserSession
    $hasHeader = ($resCsvAuth.Content -match "Bill ID" -and $resCsvAuth.Content -match "Total Amount \(INR\)" -and $resCsvAuth.Content -match "Customer Name")
    Assert-Test "Authenticated GET /api/sales/export-csv returns 200 with valid CSV content" `
        ($resCsvAuth.StatusCode -eq 200 -and $hasHeader) "Length: $($resCsvAuth.Content.Length)"

    # 3. CSV contains complete timestamps (YYYY-MM-DD HH:MM:SS) for transactions
    $csvLines = $resCsvAuth.Content -split "`r?`n"
    $dataLines = $csvLines | Where-Object { $_ -match "^INV-" }
    $allHaveCompleteTimestamps = ($dataLines.Count -gt 0)
    foreach ($line in $dataLines) {
        if (-not ($line -match '\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}')) {
            $allHaveCompleteTimestamps = $false
            Write-Host "    Line without complete timestamp: $line" -ForegroundColor Yellow
            break
        }
    }
    Assert-Test "All transactions in CSV contain complete timestamps (with seconds)" `
        ($allHaveCompleteTimestamps -eq $true) "Checked $($dataLines.Count) transactions"

    # 4. CSV properly escapes complex customer name with quotes and commas
    $hasEscapedCustomer = ($resCsvAuth.Content -match '"Gupta, Rajesh ""Gold"""')
    Assert-Test "CSV properly escapes customer names containing commas and quotes per RFC 4180" `
        ($hasEscapedCustomer -eq $true) "Looking for escaped customer name in CSV"

    # -----------------------------------------------------------
    # Test 13: Brute-Force Rate Limiting Lockout
    # -----------------------------------------------------------
    Write-Host "`n[Test Group: Brute-Force Protection]" -ForegroundColor Cyan
    $lockoutTriggered = $false
    for ($i = 1; $i -le 6; $i++) {
        $resLock = Invoke-Http -Uri "$baseUrl/api/auth/login" -Method "POST" -Body '{"username":"admin","password":"badpassword"}'
        $rawContent = if ($resLock.ContainsKey("Content")) { $resLock["Content"] } else { $resLock.Content }
        $bodyLock = try { $rawContent | ConvertFrom-Json } catch { $null }
        $msg = if ($bodyLock -and $bodyLock.message) { $bodyLock.message } else { $rawContent }
        Write-Host "    Attempt $($i) - Status: $($resLock.StatusCode), Message: '$msg'" -ForegroundColor Gray
        if ($resLock.StatusCode -eq 401 -and ($msg -match "locked|Too many failed")) {
            $lockoutTriggered = $true
            Write-Host "    -> Brute force lockout triggered on attempt $($i)!" -ForegroundColor Yellow
            break
        }
    }
    Assert-Test "Brute-force lockout triggers after consecutive failed attempts" `
        ($lockoutTriggered -eq $true) "Lockout status: $lockoutTriggered"

    # -----------------------------------------------------------
    # SUMMARY
    # -----------------------------------------------------------
    Write-Host "`n==========================================================" -ForegroundColor Cyan
    Write-Host " TEST RESULTS: $passed PASSED, $failed FAILED" -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Red" })
    Write-Host "==========================================================" -ForegroundColor Cyan

    if ($failed -gt 0) {
        exit 1
    }
}
finally {
    if ($serverProc -ne $null -and -not $serverProc.HasExited) {
        Write-Host "`nStopping background server process ($($serverProc.Id))..." -ForegroundColor Gray
        $serverProc.Kill()
        $serverProc.WaitForExit()
    }
    Get-Process -Name server -ErrorAction SilentlyContinue | Stop-Process -Force

    # Restore user's original credentials
    if ($authBackup -ne $null) {
        [System.IO.File]::WriteAllText("data/auth.json", $authBackup)
        Write-Host "Restored original authentication data to data/auth.json." -ForegroundColor Gray
    }
}
