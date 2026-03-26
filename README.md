# Edge Network Guard

Windows C++ tray application that silently checks connectivity on an interval and refreshes or opens a target page in Microsoft Edge when the network is unavailable.

## Build

```powershell
cmake -S . -B build
cmake --build build
```

## Run

- `EdgeNetworkGuard.exe` starts the tray app.
- `EdgeNetworkGuard.exe --run-once` executes one check and exits.
- `EdgeNetworkGuard.exe --install` registers a highest-privilege scheduled task.
- `EdgeNetworkGuard.exe --uninstall` removes the scheduled task.

## Config

- `test_hosts`: preferred probe target list. If any one is reachable, the network is treated as healthy.
- `test_host`: legacy single-target fallback for older configs.
- `wifi_ssid`: optional Wi-Fi SSID to reconnect before opening or refreshing the target page.
- `wifi_reconnect_wait_seconds`: wait time after sending the Wi-Fi reconnect request before probing again.
- `target_url`: page to open or refresh when all probe targets fail.
- `check_interval_seconds`: interval between checks in seconds.

The app reads `check_network_config.json` from the same executable directory.
