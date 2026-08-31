#include "MemoryBackend.h"

#include <limits>
#include <utility>

namespace
{
    bool IsValidTransfer(std::uintptr_t address, const void* buffer, std::size_t size)
    {
        return address != 0 && buffer != nullptr && size != 0 &&
               size <= (std::numeric_limits<DWORD>::max)();
    }
}

ScatterBatch::ScatterBatch(VMMDLL_SCATTER_HANDLE handle) noexcept
    : handle_(handle)
{
}

ScatterBatch::~ScatterBatch()
{
    Close();
}

ScatterBatch::ScatterBatch(ScatterBatch&& other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)),
      reads_(std::move(other.reads_)),
      writes_(std::move(other.writes_)),
      hasRequests_(std::exchange(other.hasRequests_, false))
{
}

ScatterBatch& ScatterBatch::operator=(ScatterBatch&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    Close();
    handle_ = std::exchange(other.handle_, nullptr);
    reads_ = std::move(other.reads_);
    writes_ = std::move(other.writes_);
    hasRequests_ = std::exchange(other.hasRequests_, false);
    return *this;
}

bool ScatterBatch::IsValid() const noexcept
{
    return handle_ != nullptr;
}

bool ScatterBatch::PrepareRead(
    std::uintptr_t address,
    void* destination,
    std::size_t size)
{
    if (!IsValid() || !IsValidTransfer(address, destination, size))
    {
        return false;
    }

    reads_.push_back({static_cast<DWORD>(size), 0});
    ReadRequest& request = reads_.back();
    if (!VMMDLL_Scatter_PrepareEx(
            handle_,
            static_cast<QWORD>(address),
            request.expectedBytes,
            static_cast<PBYTE>(destination),
            &request.readBytes))
    {
        reads_.pop_back();
        return false;
    }

    hasRequests_ = true;
    return true;
}

bool ScatterBatch::PrepareWrite(
    std::uintptr_t address,
    const void* source,
    std::size_t size)
{
    if (!IsValid() || !IsValidTransfer(address, source, size))
    {
        return false;
    }

    const auto* firstByte = static_cast<const std::uint8_t*>(source);
    writes_.emplace_back(firstByte, firstByte + size);
    std::vector<std::uint8_t>& ownedPayload = writes_.back();
    if (!VMMDLL_Scatter_PrepareWrite(
            handle_,
            static_cast<QWORD>(address),
            ownedPayload.data(),
            static_cast<DWORD>(ownedPayload.size())))
    {
        writes_.pop_back();
        return false;
    }

    hasRequests_ = true;
    return true;
}

bool ScatterBatch::Execute()
{
    if (!IsValid() || !hasRequests_ || !VMMDLL_Scatter_Execute(handle_))
    {
        return false;
    }

    for (const ReadRequest& request : reads_)
    {
        if (request.readBytes != request.expectedBytes)
        {
            return false;
        }
    }
    return true;
}

void ScatterBatch::Close() noexcept
{
    if (handle_ != nullptr)
    {
        VMMDLL_Scatter_CloseHandle(handle_);
        handle_ = nullptr;
    }
}

void MemoryBackend::Attach(VMM_HANDLE handle, DWORD processId) noexcept
{
    handle_ = handle;
    processId_ = processId;
}

void MemoryBackend::Reset() noexcept
{
    handle_ = nullptr;
    processId_ = 0;
}

bool MemoryBackend::IsAttached() const noexcept
{
    return handle_ != nullptr && processId_ != 0;
}

bool MemoryBackend::Read(
    std::uintptr_t address,
    void* destination,
    std::size_t size) const
{
    if (!IsAttached() || !IsValidTransfer(address, destination, size))
    {
        return false;
    }

    DWORD bytesRead = 0;
    const BOOL success = VMMDLL_MemReadEx(
        handle_,
        processId_,
        static_cast<ULONG64>(address),
        static_cast<PBYTE>(destination),
        static_cast<DWORD>(size),
        &bytesRead,
        VMMDLL_FLAG_NOCACHE);
    return success != FALSE && bytesRead == size;
}

bool MemoryBackend::Write(
    std::uintptr_t address,
    const void* source,
    std::size_t size) const
{
    if (!IsAttached() || !IsValidTransfer(address, source, size))
    {
        return false;
    }

    return VMMDLL_MemWrite(
               handle_,
               processId_,
               static_cast<ULONG64>(address),
               const_cast<PBYTE>(static_cast<const BYTE*>(source)),
               static_cast<DWORD>(size)) != FALSE;
}

ScatterBatch MemoryBackend::BeginScatter() const
{
    if (!IsAttached())
    {
        return {};
    }

    return ScatterBatch(VMMDLL_Scatter_Initialize(
        handle_, processId_, static_cast<DWORD>(VMMDLL_FLAG_NOCACHE)));
}
