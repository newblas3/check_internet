#include "task_scheduler_installer.h"

namespace {

struct ScopedBstr {
    explicit ScopedBstr(const std::wstring& value) : bstr(SysAllocString(value.c_str())) {
    }
    ~ScopedBstr() {
        if (bstr) {
            SysFreeString(bstr);
        }
    }
    BSTR bstr = nullptr;
};

VARIANT EmptyVariant() {
    VARIANT value;
    VariantInit(&value);
    return value;
}

} // namespace

TaskSchedulerInstaller::TaskSchedulerInstaller(std::wstring taskName, Logger& logger)
    : taskName_(std::move(taskName)), logger_(logger) {
}

bool TaskSchedulerInstaller::EnsureElevatedAndInstall(const std::wstring& executablePath) {
    if (IsRunningAsAdmin()) {
        return true;
    }
    return RelaunchElevated(executablePath, L"--install");
}

bool TaskSchedulerInstaller::EnsureElevatedAndUninstall(const std::wstring& executablePath) {
    if (IsRunningAsAdmin()) {
        return true;
    }
    return RelaunchElevated(executablePath, L"--uninstall");
}

bool TaskSchedulerInstaller::Install(const std::wstring& executablePath, const AppConfig& config, bool runAtBoot) {
    return RegisterTask(executablePath, config, runAtBoot);
}

bool TaskSchedulerInstaller::Uninstall() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool coInitialized = SUCCEEDED(hr);
    if (hr == RPC_E_CHANGED_MODE) {
        hr = S_OK;
    }
    if (FAILED(hr)) {
        logger_.Error(L"CoInitializeEx failed in Uninstall.");
        return false;
    }

    ITaskService* service = nullptr;
    hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, reinterpret_cast<void**>(&service));
    if (FAILED(hr)) {
        if (coInitialized) {
            CoUninitialize();
        }
        logger_.Error(L"CoCreateInstance failed for Task Scheduler.");
        return false;
    }

    VARIANT empty = EmptyVariant();
    hr = service->Connect(empty, empty, empty, empty);
    if (FAILED(hr)) {
        service->Release();
        if (coInitialized) {
            CoUninitialize();
        }
        logger_.Error(L"Task Scheduler connect failed.");
        return false;
    }

    ITaskFolder* root = nullptr;
    ScopedBstr rootPath(L"\\");
    hr = service->GetFolder(rootPath.bstr, &root);
    if (SUCCEEDED(hr)) {
        ScopedBstr taskName(taskName_);
        hr = root->DeleteTask(taskName.bstr, 0);
        root->Release();
    }
    service->Release();
    if (coInitialized) {
        CoUninitialize();
    }

    if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
        logger_.Error(L"Failed to delete scheduled task.");
        return false;
    }
    logger_.Info(L"Scheduled task removed.");
    return true;
}

bool TaskSchedulerInstaller::RegisterTask(const std::wstring& executablePath, const AppConfig&, bool runAtBoot) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool coInitialized = SUCCEEDED(hr);
    if (hr == RPC_E_CHANGED_MODE) {
        hr = S_OK;
    }
    if (FAILED(hr)) {
        logger_.Error(L"CoInitializeEx failed in Install.");
        return false;
    }

    hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
                              RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE,
                              nullptr, 0, nullptr);
    if (FAILED(hr) && hr != RPC_E_TOO_LATE) {
        if (coInitialized) {
            CoUninitialize();
        }
        logger_.Error(L"CoInitializeSecurity failed.");
        return false;
    }

    ITaskService* service = nullptr;
    hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, reinterpret_cast<void**>(&service));
    if (FAILED(hr)) {
        if (coInitialized) {
            CoUninitialize();
        }
        logger_.Error(L"CoCreateInstance failed for Task Scheduler.");
        return false;
    }

    VARIANT empty = EmptyVariant();
    hr = service->Connect(empty, empty, empty, empty);
    if (FAILED(hr)) {
        service->Release();
        if (coInitialized) {
            CoUninitialize();
        }
        logger_.Error(L"Task Scheduler connect failed.");
        return false;
    }

    ITaskFolder* root = nullptr;
    ScopedBstr rootPath(L"\\");
    hr = service->GetFolder(rootPath.bstr, &root);
    if (FAILED(hr)) {
        service->Release();
        if (coInitialized) {
            CoUninitialize();
        }
        logger_.Error(L"Failed to get Task Scheduler root folder.");
        return false;
    }

    {
        ScopedBstr taskName(taskName_);
        root->DeleteTask(taskName.bstr, 0);
    }

    ITaskDefinition* task = nullptr;
    hr = service->NewTask(0, &task);
    if (FAILED(hr)) {
        root->Release();
        service->Release();
        if (coInitialized) {
            CoUninitialize();
        }
        logger_.Error(L"Failed to create task definition.");
        return false;
    }

    IRegistrationInfo* regInfo = nullptr;
    task->get_RegistrationInfo(&regInfo);
    if (regInfo) {
        ScopedBstr author(L"EdgeNetworkGuard");
        ScopedBstr description(L"Starts Edge Network Guard with highest privileges.");
        regInfo->put_Author(author.bstr);
        regInfo->put_Description(description.bstr);
        regInfo->Release();
    }

    IPrincipal* principal = nullptr;
    task->get_Principal(&principal);
    if (principal) {
        principal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
        principal->put_RunLevel(TASK_RUNLEVEL_HIGHEST);
        principal->Release();
    }

    ITaskSettings* settings = nullptr;
    task->get_Settings(&settings);
    if (settings) {
        ScopedBstr noLimit(L"PT0S");
        settings->put_StartWhenAvailable(VARIANT_TRUE);
        settings->put_MultipleInstances(TASK_INSTANCES_IGNORE_NEW);
        settings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
        settings->put_StopIfGoingOnBatteries(VARIANT_FALSE);
        settings->put_ExecutionTimeLimit(noLimit.bstr);
        settings->Release();
    }

    ITriggerCollection* triggers = nullptr;
    task->get_Triggers(&triggers);
    if (!triggers) {
        task->Release();
        root->Release();
        service->Release();
        if (coInitialized) {
            CoUninitialize();
        }
        logger_.Error(L"Failed to access trigger collection.");
        return false;
    }

    ITrigger* trigger = nullptr;
    hr = triggers->Create(runAtBoot ? TASK_TRIGGER_BOOT : TASK_TRIGGER_LOGON, &trigger);
    if (SUCCEEDED(hr) && trigger) {
        trigger->Release();
    }
    triggers->Release();
    if (FAILED(hr)) {
        task->Release();
        root->Release();
        service->Release();
        if (coInitialized) {
            CoUninitialize();
        }
        logger_.Error(L"Failed to create task trigger.");
        return false;
    }

    IActionCollection* actions = nullptr;
    task->get_Actions(&actions);
    IAction* action = nullptr;
    hr = actions->Create(TASK_ACTION_EXEC, &action);
    IExecAction* execAction = nullptr;
    if (SUCCEEDED(hr)) {
        hr = action->QueryInterface(IID_IExecAction, reinterpret_cast<void**>(&execAction));
    }
    if (SUCCEEDED(hr) && execAction) {
        const std::wstring workingDir = std::filesystem::path(executablePath).parent_path().wstring();
        ScopedBstr exePath(executablePath);
        ScopedBstr workingDirBstr(workingDir);
        execAction->put_Path(exePath.bstr);
        execAction->put_WorkingDirectory(workingDirBstr.bstr);
        execAction->Release();
    }
    if (action) {
        action->Release();
    }
    if (actions) {
        actions->Release();
    }
    if (FAILED(hr)) {
        task->Release();
        root->Release();
        service->Release();
        if (coInitialized) {
            CoUninitialize();
        }
        logger_.Error(L"Failed to create task action.");
        return false;
    }

    IRegisteredTask* registeredTask = nullptr;
    ScopedBstr taskName(taskName_);
    VARIANT emptyUser = EmptyVariant();
    VARIANT emptyPassword = EmptyVariant();
    VARIANT sddl = EmptyVariant();
    hr = root->RegisterTaskDefinition(taskName.bstr,
                                      task,
                                      TASK_CREATE_OR_UPDATE,
                                      emptyUser,
                                      emptyPassword,
                                      TASK_LOGON_INTERACTIVE_TOKEN,
                                      sddl,
                                      &registeredTask);
    if (registeredTask) {
        registeredTask->Release();
    }
    task->Release();
    root->Release();
    service->Release();
    if (coInitialized) {
        CoUninitialize();
    }

    if (FAILED(hr)) {
        logger_.Error(L"RegisterTaskDefinition failed.");
        return false;
    }

    logger_.Info(L"Scheduled task installed successfully.");
    return true;
}

bool TaskSchedulerInstaller::IsRunningAsAdmin() const {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
}

bool TaskSchedulerInstaller::RelaunchElevated(const std::wstring& executablePath, const std::wstring& arguments) const {
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = executablePath.c_str();
    sei.lpParameters = arguments.c_str();
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei) == TRUE;
}
