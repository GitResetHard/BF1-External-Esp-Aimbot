#pragma once
#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <thread>
#include <type_traits>

class Memory
{
private:
    HWND TargetHwnd = nullptr;
public:
    DWORD PID = 0;
    uint64_t BaseAddress = 0;
    HANDLE pHandle = nullptr;

    struct ReadReport
    {
        unsigned attempts = 0;
        unsigned failures = 0;
        DWORD firstError = ERROR_SUCCESS;
        uintptr_t firstAddress = 0;
        const char* firstStage = nullptr;
    };
    // These diagnostics are owned by the rendering thread, not shared workers.
    mutable ReadReport report{};
    mutable const char* readStage = "unspecified";
    mutable bool lastReadSucceeded = false;

    bool Init();
    HWND FindTarget();

    void ResetReadReport() const noexcept
    {
        report = {};
        readStage = "unspecified";
        lastReadSucceeded = false;
    }

    bool ReadBuffer(uintptr_t address, void* output, SIZE_T size) const noexcept
    {
        ++report.attempts;
        SIZE_T transferred = 0;
        const BOOL result = ReadProcessMemory(pHandle,
            reinterpret_cast<const void*>(address), output, size, &transferred);
        const DWORD error = result ? ERROR_PARTIAL_COPY : GetLastError();
        lastReadSucceeded = result != FALSE && transferred == size;
        if (!lastReadSucceeded)
        {
            ++report.failures;
            if (report.firstStage == nullptr)
            {
                report.firstStage = readStage;
                report.firstAddress = address;
                report.firstError = error != ERROR_SUCCESS ? error : ERROR_READ_FAULT;
            }
            std::memset(output, 0, size);
        }
        return lastReadSucceeded;
    }

    template <typename T>
    T Read(const uintptr_t& address) const noexcept
    {
        static_assert(std::is_trivially_copyable<T>::value,
                      "Remote reads require a trivially copyable value");
        T value{};
        if (!ReadBuffer(address, &value, sizeof(value)))
            return T{};
        return value;
    }

    template <typename T>
    constexpr void Write(const uintptr_t& address, const T& value) const noexcept
    {
        WriteProcessMemory(pHandle, reinterpret_cast<void*>(address), &value, sizeof(T), NULL);
    }
};

extern Memory m;
