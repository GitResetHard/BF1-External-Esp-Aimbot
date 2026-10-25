#pragma once
#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdint>
#include <cstring>
#include <limits>
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
        SIZE_T firstRequested = 0;
        SIZE_T firstTransferred = 0;
        const char* firstOperation = nullptr;
    };
    // Rendering-thread only, as in the original implementation.
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

    // Caller must supply a local buffer containing at least size writable bytes.
    bool ReadBuffer(uintptr_t address, void* output, SIZE_T size) const noexcept
    {
        ++report.attempts;
        lastReadSucceeded = false;
        SIZE_T transferred = 0;
        DWORD error = ERROR_SUCCESS;
        const char* operation = "argument validation";

        if (!output || size == 0)
            error = ERROR_INVALID_PARAMETER;
        else if (!pHandle || pHandle == INVALID_HANDLE_VALUE)
            error = ERROR_INVALID_HANDLE;
        else if (!address || size - 1 > (std::numeric_limits<uintptr_t>::max)() - address)
            error = ERROR_INVALID_ADDRESS;
        else
        {
            operation = "ReadProcessMemory";
            SetLastError(ERROR_SUCCESS);
            const BOOL ok = ReadProcessMemory(
                pHandle, reinterpret_cast<const void*>(address),
                output, size, &transferred);
            // Capture immediately, before logging or any other Windows call.
            error = ok ? ERROR_SUCCESS : GetLastError();

            if (ok && transferred == size)
            {
                lastReadSucceeded = true;
                return true;
            }
            if (ok)
                error = ERROR_PARTIAL_COPY;
            else if (error == ERROR_SUCCESS)
                error = ERROR_READ_FAULT;
        }

        ++report.failures;
        if (report.failures == 1)
        {
            report.firstStage = readStage;
            report.firstError = error;
            report.firstAddress = address;
            report.firstRequested = size;
            report.firstTransferred = transferred;
            report.firstOperation = operation;
        }
        if (output && size)
            std::memset(output, 0, size);
        return false;
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

    // This class now opens read-only handles. Accidental writes are compile errors.
    template <typename T>
    void Write(const uintptr_t&, const T&) const = delete;
};

extern Memory m;
