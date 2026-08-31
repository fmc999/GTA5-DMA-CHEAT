#include <cassert>
#include "../GTA5_DMA/GTA5_DMA/Core/AppRuntime.h"

int main()
{
    AppRuntime::Reset();
    assert(AppRuntime::IsRunning());
    AppRuntime::RequestStop();
    assert(!AppRuntime::IsRunning());
    AppRuntime::Reset();
    assert(AppRuntime::IsRunning());
    return 0;
}
