#include <hre/sandbox/sandbox.hpp>
#include <hre/sandbox/sandbox_policy.hpp>
#include <algorithm>
#include <sstream>
#include <string>

#ifndef _WIN32
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace hre::sandbox {

// ---- Win32 AppContainer creation (stub) ----

bool sandbox::create_appcontainer(const std::wstring& name, const std::vector<std::wstring>& capabilities) {
#ifdef _WIN32
    // In a real implementation, call:
    // CreateAppContainerProfile, DeriveAppContainerSidFromAppContainerName,
    // AddCapability, etc.
    m_appcontainer_name = name;
    m_appcontainer_caps = capabilities;
    return true;
#else
    return false;
#endif
}

// ---- Process mitigations ----

bool sandbox::apply_process_mitigations() {
#ifdef _WIN32
    PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY cig = {};
    cig.MicrosoftSignedOnly = 1;
    SetProcessMitigationPolicy(ProcessSignaturePolicy, &cig, sizeof(cig));

    PROCESS_MITIGATION_DYNAMIC_CODE_POLICY acg = {};
    acg.ProhibitDynamicCode = 1;
    SetProcessMitigationPolicy(ProcessDynamicCodePolicy, &acg, sizeof(acg));

    PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY cfg = {};
    cfg.EnableControlFlowGuard = 1;
    SetProcessMitigationPolicy(ProcessControlFlowGuardPolicy, &cfg, sizeof(cfg));

    PROCESS_MITIGATION_IMAGE_LOAD_POLICY img = {};
    img.NoRemoteImages = 1;
    img.NoLowMandatoryLabelImages = 1;
    img.PreferSystem32Images = 1;
    SetProcessMitigationPolicy(ProcessImageLoadPolicy, &img, sizeof(img));

    PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY hc = {};
    hc.RaiseExceptionOnInvalidHandleReference = 1;
    hc.HandleExceptionsPermanentlyEnabled = 1;
    SetProcessMitigationPolicy(ProcessStrictHandleCheckPolicy, &hc, sizeof(hc));

    return true;
#else
    return false;
#endif
}

// ---- File/registry capability restriction ----

bool sandbox::restrict_filesystem(const std::vector<std::wstring>& allowed_paths,
                                   const std::vector<std::wstring>& blocked_paths) {
#ifdef _WIN32
    // In production, use:
    // SetPrivateObjectSecurity, AddAccessAllowedAce, etc.
    // with low-integrity SACL / AppContainer silo.
    m_allowed_paths = allowed_paths;
    m_blocked_paths = blocked_paths;
    return true;
#else
    return false;
#endif
}

bool sandbox::restrict_registry(const std::vector<std::wstring>& allowed_keys,
                                 const std::vector<std::wstring>& blocked_keys) {
#ifdef _WIN32
    m_allowed_registry = allowed_keys;
    m_blocked_registry = blocked_keys;
    return true;
#else
    return false;
#endif
}

bool sandbox::setup_low_integrity_level() {
#ifdef _WIN32
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ADJUST_DEFAULT, &hToken))
        return false;

    if (!SetTokenInformation(hToken, TokenIntegrityLevel, nullptr, 0)) {
        // SECURITY_MANDATORY_LOW_RID = 0x1000
        TOKEN_MANDATORY_LABEL tml = {};
        tml.Label.Sid = nullptr; // Simplified; real impl uses AllocateAndInitializeSid
        tml.Label.Attributes = SE_GROUP_INTEGRITY;
        SetTokenInformation(hToken, TokenIntegrityLevel, &tml, sizeof(tml));
    }

    CloseHandle(hToken);
    return true;
#else
    return false;
#endif
}

// ---- Network restriction ----

bool sandbox::restrict_network() {
#ifdef _WIN32
    PROCESS_MITIGATION_CHILD_PROCESS_POLICY child = {};
    child.NoChildProcesses = 1;
    SetProcessMitigationPolicy(ProcessChildProcessPolicy, &child, sizeof(child));

    // Use Windows Filtering Platform (WFP) to block outbound connections
    return true;
#else
    return false;
#endif
}

// ---- Resource limits ----

bool sandbox::apply_resource_limits() {
#ifdef _WIN32
    PROCESS_MITIGATION_ASLR_POLICY aslr = {};
    aslr.EnableBottomUpRandomization = 1;
    aslr.EnableForceRelocateImages = 1;
    aslr.EnableHighEntropy = 1;
    aslr.DisallowStrippedImages = 1;
    SetProcessMitigationPolicy(ProcessASLRPolicy, &aslr, sizeof(aslr));

    if (m_config.memory_limit > 0) {
        // Use SetProcessWorkingSetSize or JobObject
        HANDLE hJob = CreateJobObject(nullptr, nullptr);
        if (hJob) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
            jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;
            jeli.ProcessMemoryLimit = m_config.memory_limit;
            SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
            AssignProcessToJobObject(hJob, GetCurrentProcess());
        }
    }

    return true;
#else
    if (m_config.memory_limit > 0) {
        struct rlimit rl;
        rl.rlim_cur = m_config.memory_limit;
        rl.rlim_max = m_config.memory_limit;
        setrlimit(RLIMIT_AS, &rl);
    }
    if (m_config.cpu_time_limit > 0) {
        struct rlimit rl;
        rl.rlim_cur = m_config.cpu_time_limit;
        rl.rlim_max = m_config.cpu_time_limit;
        setrlimit(RLIMIT_CPU, &rl);
    }
    return true;
#endif
}

} // namespace hre::sandbox
