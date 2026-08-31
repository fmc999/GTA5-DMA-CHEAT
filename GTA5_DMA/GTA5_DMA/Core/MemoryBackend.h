#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

#include "vmmdll.h"

class ScatterBatch
{
public:
    ScatterBatch() = default;
    ~ScatterBatch();

    ScatterBatch(const ScatterBatch&) = delete;
    ScatterBatch& operator=(const ScatterBatch&) = delete;
    ScatterBatch(ScatterBatch&& other) noexcept;
    ScatterBatch& operator=(ScatterBatch&& other) noexcept;

    bool IsValid() const noexcept;
    bool PrepareRead(std::uintptr_t address, void* destination, std::size_t size);
    bool PrepareWrite(std::uintptr_t address, const void* source, std::size_t size);
    bool Execute();

private:
    friend class MemoryBackend;

    struct ReadRequest
    {
        DWORD expectedBytes = 0;
        DWORD readBytes = 0;
    };

    explicit ScatterBatch(VMMDLL_SCATTER_HANDLE handle) noexcept;
    void Close() noexcept;

    VMMDLL_SCATTER_HANDLE handle_ = nullptr;
    std::deque<ReadRequest> reads_;
    std::deque<std::vector<std::uint8_t>> writes_;
    bool hasRequests_ = false;
};

class MemoryBackend
{
public:
    void Attach(VMM_HANDLE handle, DWORD processId) noexcept;
    void Reset() noexcept;
    bool IsAttached() const noexcept;

    bool Read(std::uintptr_t address, void* destination, std::size_t size) const;
    bool Write(std::uintptr_t address, const void* source, std::size_t size) const;
    ScatterBatch BeginScatter() const;

private:
    VMM_HANDLE handle_ = nullptr;
    DWORD processId_ = 0;
};
