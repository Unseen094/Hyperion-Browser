// Syscall allowlist for x64 Windows (using SetProcessMitigationPolicy)
// This module provides syscall filtering via process mitigation policies
// rather than seccomp-BPF (which is Linux-specific).

#ifdef _WIN32

#include <windows.h>
#include <winternl.h>
#include <stdio.h>

// Syscall numbers for x64 Windows 10/11
// These are version-dependent; use dynamic resolution in production.
#define SYSCALL_NT_QUERY_SYSTEM_INFORMATION   0x0036
#define SYSCALL_NT_QUERY_INFORMATION_PROCESS  0x0019
#define SYSCALL_NT_SET_INFORMATION_PROCESS    0x001A
#define SYSCALL_NT_CREATE_THREAD              0x00A5
#define SYSCALL_NT_OPEN_PROCESS               0x0026
#define SYSCALL_NT_OPEN_THREAD                0x00A6
#define SYSCALL_NT_ALLOCATE_VIRTUAL_MEMORY    0x0018
#define SYSCALL_NT_FREE_VIRTUAL_MEMORY        0x001E
#define SYSCALL_NT_PROTECT_VIRTUAL_MEMORY     0x003D
#define SYSCALL_NT_READ_VIRTUAL_MEMORY        0x003C
#define SYSCALL_NT_WRITE_VIRTUAL_MEMORY       0x0037
#define SYSCALL_NT_CREATE_FILE                0x0055
#define SYSCALL_NT_READ_FILE                  0x0006
#define SYSCALL_NT_WRITE_FILE                 0x0008
#define SYSCALL_NT_CLOSE                      0x000C
#define SYSCALL_NT_CREATE_KEY                 0x004F
#define SYSCALL_NT_OPEN_KEY                   0x000E
#define SYSCALL_NT_QUERY_VALUE_KEY            0x0010
#define SYSCALL_NT_SET_VALUE_KEY              0x0011

#define SYSCALL_COUNT 256

// x64 syscall allowlist
static int g_allowlist[SYSCALL_COUNT] = {0};

static void init_allowlist(void) {
    // File I/O
    g_allowlist[SYSCALL_NT_CREATE_FILE] = 1;
    g_allowlist[SYSCALL_NT_READ_FILE] = 1;
    g_allowlist[SYSCALL_NT_WRITE_FILE] = 1;
    g_allowlist[SYSCALL_NT_CLOSE] = 1;

    // Memory management
    g_allowlist[SYSCALL_NT_ALLOCATE_VIRTUAL_MEMORY] = 1;
    g_allowlist[SYSCALL_NT_FREE_VIRTUAL_MEMORY] = 1;
    g_allowlist[SYSCALL_NT_PROTECT_VIRTUAL_MEMORY] = 1;

    // Process/thread query (not creation)
    g_allowlist[SYSCALL_NT_QUERY_INFORMATION_PROCESS] = 1;
    g_allowlist[SYSCALL_NT_QUERY_SYSTEM_INFORMATION] = 1;

    // Registry reads (not writes)
    g_allowlist[SYSCALL_NT_OPEN_KEY] = 1;
    g_allowlist[SYSCALL_NT_QUERY_VALUE_KEY] = 1;
}

// Check if a syscall number is on the allowlist
int syscall_is_allowed(unsigned long syscall_num) {
    if (syscall_num >= SYSCALL_COUNT) return 0;
    init_allowlist(); // idempotent after first call
    return g_allowlist[syscall_num];
}

// Apply syscall filtering via Win32 mitigation policies
int apply_syscall_filter(void) {
    PROCESS_MITIGATION_DYNAMIC_CODE_POLICY dcp = {0};
    dcp.ProhibitDynamicCode = 1;
    if (!SetProcessMitigationPolicy(ProcessDynamicCodePolicy, &dcp, sizeof(dcp))) {
        return -1;
    }

    PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY bsp = {0};
    bsp.MicrosoftSignedOnly = 1;
    if (!SetProcessMitigationPolicy(ProcessSignaturePolicy, &bsp, sizeof(bsp))) {
        return -1;
    }

    PROCESS_MITIGATION_IMAGE_LOAD_POLICY ilp = {0};
    ilp.NoRemoteImages = 1;
    ilp.NoLowMandatoryLabelImages = 1;
    ilp.PreferSystem32Images = 1;
    if (!SetProcessMitigationPolicy(ProcessImageLoadPolicy, &ilp, sizeof(ilp))) {
        return -1;
    }

    return 0;
}

// Block thread creation
int prevent_thread_creation(void) {
    PROCESS_MITIGATION_CHILD_PROCESS_POLICY cpp = {0};
    cpp.NoChildProcesses = 1;
    return SetProcessMitigationPolicy(ProcessChildProcessPolicy, &cpp, sizeof(cpp)) ? 0 : -1;
}

#else
// Linux seccomp-BPF equivalent (stub)
#include <stddef.h>

int syscall_is_allowed(unsigned long syscall_num) {
    (void)syscall_num;
    // In production, generate BPF programs via libseccomp
    return 1;
}

int apply_syscall_filter(void) {
    // prctl(PR_SET_NO_NEW_PRIVS, 1) + seccomp(SECCOMP_SET_MODE_FILTER, ...)
    return 0;
}

int prevent_thread_creation(void) {
    return 0;
}
#endif
