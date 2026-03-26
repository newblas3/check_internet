# =============================
# 配置
# =============================
$testHost = "8.8.8.8"
$targetUrl = "http://192.168.2.135"
$edgePath = "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
$debugPort = 9222

# =============================
# 检查网络是否连通
# =============================
$ping = Test-Connection -ComputerName $testHost -Count 2 -Quiet

if ($ping) {
    exit
}

# =============================
# 网络不通时执行
# =============================

# 检查 Edge 是否已开启远程调试模式
$edgeDebug = netstat -ano | findstr ":$debugPort"

if (-not $edgeDebug) {
    # 启动 Edge 调试模式
    Start-Process $edgePath "--remote-debugging-port=$debugPort"
    Start-Sleep -Seconds 3
}

# 获取当前标签页
try {
    $tabs = Invoke-RestMethod "http://localhost:$debugPort/json"
} catch {
    # 获取失败则直接打开页面
    Start-Process $edgePath $targetUrl
    exit
}

$found = $false

foreach ($tab in $tabs) {
    if ($tab.url -like "*192.168.2.135*") {
        $found = $true
        $refreshUrl = $tab.webSocketDebuggerUrl -replace "ws://", "http://"
        Invoke-RestMethod -Method Post "$refreshUrl/reload"
        break
    }
}

if (-not $found) {
    Start-Process $edgePath $targetUrl
}