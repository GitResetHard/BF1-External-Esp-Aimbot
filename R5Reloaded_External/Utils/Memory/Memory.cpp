#include "Memory.h"
#include <chrono>
#include <iostream>
#include <thread>

bool Memory::Init()
{
    static_assert(sizeof(uintptr_t) == 8, "Build this project as x64.");

    // Init is called on the main thread before the worker starts.
    if (pHandle && pHandle != INVALID_HANDLE_VALUE)
        CloseHandle(pHandle);
    pHandle = nullptr;
    PID = 0;
    BaseAddress = 0;
    TargetHwnd = nullptr;

    std::cout << "[attach] Waiting for the game window...\n";
    HWND window = nullptr;
    while (!(window = FindTarget()))
        std::this_thread::sleep_for(std::chrono::milliseconds(250));

    DWORD candidatePid = 0;
    if (!GetWindowThreadProcessId(window, &candidatePid))
    {
        const DWORD error = GetLastError();
        std::cerr << "[attach] GetWindowThreadProcessId failed: " << error << '\n';
        return false;
    }
    if (candidatePid == 0 || candidatePid == GetCurrentProcessId())
    {
        std::cerr << "[attach] Refusing invalid/self PID: " << candidatePid << '\n';
        return false;
    }

    // Standard, read-only Windows access. No privilege escalation or driver.
    constexpr DWORD access = PROCESS_VM_READ | PROCESS_QUERY_LIMITED_INFORMATION;
    HANDLE process = OpenProcess(access, FALSE, candidatePid);
    if (!process)
    {
        const DWORD error = GetLastError();
        std::cerr << "[attach] OpenProcess failed: PID=" << candidatePid
                  << " error=" << error << '\n';
        return false;
    }

    auto fail = [process](const char* operation, DWORD error)
    {
        std::cerr << "[attach] " << operation << " failed: " << error << '\n';
        CloseHandle(process);
        return false;
    };

    const DWORD handlePid = GetProcessId(process);
    if (!handlePid)
        return fail("GetProcessId", GetLastError());
    if (handlePid != candidatePid)
        return fail("process identity check", ERROR_INVALID_PARAMETER);

    wchar_t image[32768]{};
    DWORD length = static_cast<DWORD>(sizeof(image) / sizeof(image[0]));
    if (!QueryFullProcessImageNameW(process, 0, image, &length))
        return fail("QueryFullProcessImageNameW", GetLastError());

    DWORD exitCode = 0;
    if (!GetExitCodeProcess(process, &exitCode))
        return fail("GetExitCodeProcess", GetLastError());
    if (exitCode != STILL_ACTIVE)
        return fail("target already exited", ERROR_PROCESS_ABORTED);

    // Recheck the window after opening/querying the process.
    DWORD currentWindowPid = 0;
    if (!GetWindowThreadProcessId(window, &currentWindowPid))
        return fail("window revalidation", GetLastError());
    if (currentWindowPid != candidatePid)
        return fail("window owner changed", ERROR_INVALID_PARAMETER);

    TargetHwnd = window;
    PID = candidatePid;
    pHandle = process;
    ResetReadReport();
    std::cout << "[attach] PID=" << PID << " handle=" << pHandle
              << " pointerBits=" << sizeof(uintptr_t) * 8 << '\n';
    std::wcout << L"[attach] executable=" << image << L'\n';
    return true;
}

HWND Memory::FindTarget()
{
    // Keep the original window-title selection; print the selected executable above.
    return FindWindowW(nullptr, L"Battlefield\u2122 1");
}

Memory m;
