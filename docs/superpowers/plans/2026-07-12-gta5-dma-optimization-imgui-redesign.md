# GTA5 DMA Optimization and ImGui Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在保持现有 DMA 读写、功能状态和快捷键行为不变的前提下，完成线程与访问保护，并将 UI 更新为可缩放的中性蓝灰紧凑双栏 ImGui 控制台。

**Architecture:** 先为现有工程建立双配置构建与人工回归基线，再独立修复运行生命周期和受检 DMA 接口。新 UI 由单独的外壳和主题组件承载，继续调用现有 `MenuManager::Render*PageContent()`，逐页迁移而不触碰功能模块内部读写。

**Tech Stack:** C++20, Visual Studio MSBuild, Win32, DirectX 11, Dear ImGui, MemProcFS

---

## 文件结构

- 新建 `GTA5_DMA/GTA5_DMA/AppRuntime.h/.cpp`：集中维护原子运行状态和停止请求。
- 修改 `GTA5_DMA/GTA5_DMA/main.cpp`、`ModeSelector.cpp`、`DMA.cpp`：统一后台线程生命周期，避免 detached 或重复 DMA 循环。
- 修改 `GTA5_DMA/GTA5_DMA/DMA.h`：补齐受检读写和多级指针边界。
- 新建 `GTA5_DMA/GTA5_DMA/ConsoleTheme.h/.cpp`：中性蓝灰 ImGui 样式。
- 新建 `GTA5_DMA/GTA5_DMA/ConsoleShell.h/.cpp`：900x620 可缩放双栏外壳、导航、顶部栏和底部状态栏。
- 修改 `GTA5_DMA/GTA5_DMA/MenuManager.h/.cpp`：保留页面内容函数，接入新外壳并逐页复用原控件绑定。
- 修改 `GTA5_DMA/GTA5_DMA/MyMenu.cpp`：以单一入口切换新 UI，旧渲染路径在迁移期可回退。
- 修改 `GTA5_DMA/GTA5_DMA/GTA5_DMA.vcxproj`：登记新增源文件。
- 新建 `docs/testing/gta5-dma-regression-checklist.md`：实机回归记录。

### Task 1: 构建与回归基线

- [ ] 构建 `Debug|x64`：`MSBuild GTA5_DMA/GTA5_DMA.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m /v:minimal`，预期成功并记录项目自身警告。
- [ ] 构建 `Release|x64`：同上改为 `Release`，预期成功；第三方 C4200 单独记录。
- [ ] 新建实机回归清单，列出初始化、版本识别、人物、载具、武器、传送、任务、上下车、重生、切换战局、游戏重启和 END 退出。
- [ ] 不提交仓库；用 `git diff --check` 确认文档格式。

### Task 2: 统一运行生命周期

- [ ] 新建 `AppRuntime`，提供 `IsRunning()`、`RequestStop()` 和测试用 `Reset()`，内部使用 `std::atomic_bool`。
- [ ] 将 `main.cpp` 的普通 `bAlive` 替换为 `AppRuntime`，保留 END 键语义。
- [ ] 清点并移除 `ModeSelector.cpp` 的 detached DMA 线程启动路径，确保只有 `main.cpp` 拥有并 join 后台线程。
- [ ] 让 DMA 循环使用统一运行状态；初始化失败时请求停止并输出错误。
- [ ] 分别构建 Debug 和 Release；实机验证启动、初始化失败及 END 退出无卡死。

### Task 3: 加固 DMA 访问边界

- [ ] 在 `DMA.h` 增加句柄、PID 和地址有效性检查辅助函数。
- [ ] 让 `ReadMultiLevelPointer` 与 `WriteMultiLevelPointer` 在 offsets 为空时返回 false，并保持逐级字节数与空地址检查。
- [ ] 检查 `VMMDLL_MemWrite` 的返回值，禁止底层写入失败却返回成功。
- [ ] 保持所有调用签名兼容，不修改偏移和功能数值。
- [ ] 修复 `PlayerChaser.cpp` 的 `size_t` 到 `DWORD` 窄化转换，先验证范围再转换。
- [ ] 构建双配置；实机回归人物、载具、武器与传送读写。

### Task 4: 建立新主题与可缩放 UI 外壳

- [ ] 新建 `ConsoleTheme`，设置中性蓝灰颜色、3-5px 圆角、稳定间距和成功/警告/错误色。
- [ ] 新建 `ConsoleShell`，使用 `SetNextWindowSize(..., ImGuiCond_FirstUseEver)` 设置 900x620，使用窗口尺寸约束保持最小 760x520。
- [ ] 实现顶部状态栏、固定宽度左栏、可扩展右侧内容区和底部状态栏；各区域使用 `BeginChild`，参数对齐使用 `BeginTable`。
- [ ] 左侧快速控制只绑定现有状态变量，不直接调用新的 DMA 读写。
- [ ] 在 vcxproj 中登记新增文件，构建双配置。
- [ ] 启动程序并截取 900x620、760x520 和更宽窗口截图，检查文字、滚动和控件无重叠。

### Task 5: 迁移页面并保持功能绑定

- [ ] 将人物页接入右侧内容区，复用现有 `RenderPlayerPageContent()`；实机验证全部人物功能。
- [ ] 迁移载具页，复用 `RenderVehiclePageContent()` 与 `VehicleEditor` 现有接口；验证上下车与载具切换。
- [ ] 迁移武器页和传送页；验证参数修改、传送列表与选择状态。
- [ ] 迁移时间、抢劫和设置页；验证默认值和开关语义。
- [ ] 删除页面各自创建顶层窗口的重复代码，仅由 `ConsoleShell` 创建主窗口；保留内容函数和旧路径直至全部回归通过。
- [ ] 每迁移一页就构建 Release 并执行对应实机清单，不将多个失败页面混在同一批修复。

### Task 6: 减少可证明的无效 DMA 访问

- [ ] 统计 `DMA::DMAThreadEntry()` 中每轮调用；仅对关闭功能仍执行的访问和相同目标值重复写入做优化。
- [ ] 在单次 DMA 更新周期内复用已解析的本地玩家、导航、载具和武器地址，不跨角色重生或战局切换长期缓存。
- [ ] 当关键指针变化或读取失败时使相关快照立即失效。
- [ ] 不改变关键功能原有响应频率；若实机感知延迟增加，恢复该模块原策略。
- [ ] 构建双配置并完成完整实机回归。

### Task 7: 拆分大型 UI 文件

- [ ] 将 `MenuManager.cpp` 的页面内容按人物、载具、武器、传送和任务拆到独立实现文件，保持 `MenuManager` 公共签名。
- [ ] 将 `VehicleEditor.cpp` 逐步拆为状态读取、属性写入、预设和 UI 渲染实现，保持 `VehicleEditor.h` 的公共接口。
- [ ] 每次只迁移一个职责并构建，禁止同时修改功能数值或 DMA 地址逻辑。
- [ ] 运行完整实机回归，确认旧菜单回退不再需要后再移除旧调用。

### Task 8: 最终验证

- [ ] 运行 `git diff --check`，确认无补丁格式问题且未误改历史目录、ImGui、MemProcFS 或 Offsets。
- [ ] 清理构建后重新构建 Debug 和 Release，记录成功输出与剩余第三方警告。
- [ ] 完成原版与 Enhanced 的完整实机回归清单。
- [ ] 检查 900x620、760x520 和宽屏尺寸的 UI，无重叠、裁切和失效控件。
- [ ] 保持所有修改为本地未提交状态，不执行 add、commit 或 push。
