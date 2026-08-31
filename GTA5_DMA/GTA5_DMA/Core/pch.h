#pragma once
#define NOMINMAX // 防止Windows.h的min/max宏与std::min/std::max冲突
#include <iostream>
#include <Windows.h>
#include <cstdint>
#include <vector>
#include <array>
#include <map>
#include <string>
#include <format>
#include <bitset>
#include <print>
#include <memory>

#include <vmmdll.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>

#include "Vector.h"

#include "Reclass.h"

#include "DMA.h"
#include "VehicleEditor.h"