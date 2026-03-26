#include "wifi_manager.h"

namespace {

std::wstring GuidToString(const GUID& guid) {
    wchar_t buffer[64]{};
    StringFromGUID2(guid, buffer, static_cast<int>(std::size(buffer)));
    return buffer;
}

} // namespace

void WifiManager::HandleCloser::operator()(HANDLE handle) const {
    if (handle) {
        WlanCloseHandle(handle, nullptr);
    }
}

void WifiManager::MemoryFreer::operator()(void* ptr) const {
    if (ptr) {
        WlanFreeMemory(ptr);
    }
}

WifiManager::WifiManager(Logger& logger) : logger_(logger) {
}

std::optional<WifiManager::ClientHandle> WifiManager::OpenClient() const {
    DWORD negotiatedVersion = 0;
    HANDLE rawHandle = nullptr;
    const DWORD result = WlanOpenHandle(2, nullptr, &negotiatedVersion, &rawHandle);
    if (result != ERROR_SUCCESS || !rawHandle) {
        logger_.Error(L"WlanOpenHandle failed.");
        return std::nullopt;
    }
    return ClientHandle(rawHandle);
}

bool WifiManager::EnsureConnected(const std::wstring& targetSsid) {
    if (targetSsid.empty()) {
        logger_.Info(L"wifi_ssid is empty. Skipping Wi-Fi recovery.");
        return false;
    }

    auto clientHandle = OpenClient();
    if (!clientHandle) {
        return false;
    }

    WLAN_INTERFACE_INFO_LIST* rawInterfaces = nullptr;
    const DWORD enumResult = WlanEnumInterfaces(clientHandle->get(), nullptr, &rawInterfaces);
    std::unique_ptr<WLAN_INTERFACE_INFO_LIST, MemoryFreer> interfaces(rawInterfaces);
    if (enumResult != ERROR_SUCCESS || !interfaces) {
        logger_.Error(L"WlanEnumInterfaces failed.");
        return false;
    }

    for (DWORD i = 0; i < interfaces->dwNumberOfItems; ++i) {
        const auto& iface = interfaces->InterfaceInfo[i];
        logger_.Info(L"Checking Wi-Fi interface: " + std::wstring(iface.strInterfaceDescription));

        if (IsTargetConnected(clientHandle->get(), iface.InterfaceGuid, targetSsid)) {
            logger_.Info(L"Target Wi-Fi is already connected: " + targetSsid);
            return true;
        }

        EnableInterface(clientHandle->get(), iface.InterfaceGuid);
        if (ConnectInterface(clientHandle->get(), iface.InterfaceGuid, targetSsid)) {
            logger_.Info(L"Wi-Fi connect request sent for SSID: " + targetSsid);
            return true;
        }
    }

    logger_.Error(L"No Wi-Fi interface could connect to SSID: " + targetSsid);
    return false;
}

bool WifiManager::EnableInterface(HANDLE clientHandle, const GUID& interfaceGuid) const {
    WLAN_PHY_RADIO_STATE radioState{};
    radioState.dwPhyIndex = 0;
    radioState.dot11SoftwareRadioState = dot11_radio_state_on;
    radioState.dot11HardwareRadioState = dot11_radio_state_on;

    const DWORD result = WlanSetInterface(clientHandle,
                                          &interfaceGuid,
                                          wlan_intf_opcode_radio_state,
                                          sizeof(radioState),
                                          &radioState,
                                          nullptr);
    if (result != ERROR_SUCCESS) {
        logger_.Info(L"WlanSetInterface radio_state failed or unsupported for interface " + GuidToString(interfaceGuid));
        return false;
    }

    logger_.Info(L"Requested Wi-Fi radio enable for interface " + GuidToString(interfaceGuid));
    return true;
}

bool WifiManager::ConnectInterface(HANDLE clientHandle, const GUID& interfaceGuid, const std::wstring& targetSsid) const {
    WLAN_AVAILABLE_NETWORK_LIST* rawNetworks = nullptr;
    const DWORD listResult = WlanGetAvailableNetworkList(clientHandle,
                                                         &interfaceGuid,
                                                         0,
                                                         nullptr,
                                                         &rawNetworks);
    std::unique_ptr<WLAN_AVAILABLE_NETWORK_LIST, MemoryFreer> networks(rawNetworks);
    if (listResult != ERROR_SUCCESS || !networks) {
        logger_.Error(L"WlanGetAvailableNetworkList failed.");
        return false;
    }

    for (DWORD i = 0; i < networks->dwNumberOfItems; ++i) {
        const auto& network = networks->Network[i];
        const std::wstring ssid = SsidToString(network.dot11Ssid);
        if (_wcsicmp(ssid.c_str(), targetSsid.c_str()) != 0) {
            continue;
        }

        std::wstring profileName = network.strProfileName;
        if (profileName.empty()) {
            profileName = targetSsid;
        }

        WLAN_CONNECTION_PARAMETERS params{};
        params.wlanConnectionMode = wlan_connection_mode_profile;
        params.strProfile = profileName.c_str();
        params.pDot11Ssid = nullptr;
        params.pDesiredBssidList = nullptr;
        params.dot11BssType = network.dot11BssType;
        params.dwFlags = 0;

        const DWORD connectResult = WlanConnect(clientHandle, &interfaceGuid, &params, nullptr);
        if (connectResult == ERROR_SUCCESS) {
            return true;
        }

        logger_.Error(L"WlanConnect failed for SSID: " + targetSsid);
        return false;
    }

    logger_.Error(L"Configured SSID was not found in available networks: " + targetSsid);
    return false;
}

bool WifiManager::IsTargetConnected(HANDLE clientHandle, const GUID& interfaceGuid, const std::wstring& targetSsid) const {
    WLAN_CONNECTION_ATTRIBUTES* rawAttributes = nullptr;
    DWORD dataSize = 0;
    WLAN_OPCODE_VALUE_TYPE opcode = wlan_opcode_value_type_invalid;
    const DWORD queryResult = WlanQueryInterface(clientHandle,
                                                 &interfaceGuid,
                                                 wlan_intf_opcode_current_connection,
                                                 nullptr,
                                                 &dataSize,
                                                 reinterpret_cast<PVOID*>(&rawAttributes),
                                                 &opcode);
    std::unique_ptr<WLAN_CONNECTION_ATTRIBUTES, MemoryFreer> attributes(rawAttributes);
    if (queryResult != ERROR_SUCCESS || !attributes) {
        return false;
    }

    if (attributes->isState != wlan_interface_state_connected) {
        return false;
    }

    const std::wstring connectedSsid = SsidToString(attributes->wlanAssociationAttributes.dot11Ssid);
    return _wcsicmp(connectedSsid.c_str(), targetSsid.c_str()) == 0;
}

std::wstring WifiManager::SsidToString(const DOT11_SSID& ssid) {
    return ToWide(std::string(reinterpret_cast<const char*>(ssid.ucSSID), ssid.uSSIDLength));
}
