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

- `test_hosts`: optional probe target list used only for diagnostics and fallback when HTTP connectivity checks are disabled.
- `test_host`: legacy single-target fallback for older configs.
- `connectivity_check_url`: URL used to verify that normal web access is actually working.
- `connectivity_expected_status`: expected HTTP status for `connectivity_check_url`. A mismatch is treated as offline or captive-portal state.
- `wifi_ssid`: optional Wi-Fi SSID to reconnect before opening or refreshing the target page.
- `wifi_reconnect_wait_seconds`: wait time after sending the Wi-Fi reconnect request before probing again.
- `target_url`: page to open or refresh when the internet check fails.
- `check_interval_seconds`: interval between checks in seconds.

The app reads `check_network_config.json` from the same executable directory.
