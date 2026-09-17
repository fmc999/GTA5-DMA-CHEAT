<div align="center">

# GTA5 DMA 控制台

**用 FPGA + DMA 从外部读写 GTA5（Enhanced）内存的控制台**

不注入 · 不改游戏文件 · 不挂 hook · **游戏更新后改文本即可，不用重编译**

[![Platform](https://img.shields.io/badge/platform-Windows_x64-blue)](https://github.com/fmc999/GTA5-DMA-CHEAT)
[![Language](https://img.shields.io/badge/language-C%2B%2B23-00599C)](https://isocpp.org/)
[![UI](https://img.shields.io/badge/UI-Dear%20ImGui%20%2B%20DX11-e05361)](https://github.com/ocornut/imgui)
[![DMA](https://img.shields.io/badge/DMA-MemProcFS%20%2F%20FPGA-8A2BE2)](https://github.com/ufrisk/MemProcFS)
[![Build](https://github.com/fmc999/GTA5-DMA-CHEAT/actions/workflows/msbuild.yml/badge.svg)](https://github.com/fmc999/GTA5-DMA-CHEAT/actions/workflows/msbuild.yml)
[![Release](https://img.shields.io/badge/release-latest-2ea44f)](https://github.com/fmc999/GTA5-DMA-CHEAT/releases/latest)
[![License: Custom](https://img.shields.io/badge/license-Custom%20Non--Commercial-orange)](LICENSE)

**中文** · [English](README.en.md)

**免费发布 · 请勿贩卖 · 仅供技术研究与 DMA 读写学习**

</div>

---

## 这是什么

一台**辅机**（装了 FPGA DMA 卡）通过 PCIe 直接读写目标机的物理内存，游戏侧完全看不到工具的存在：

| 原则 | 说明 |
|---|---|
| **不注入** | 不往游戏进程加载任何模块 |
| **不改文件** | 不碰游戏目录、不碰反作弊文件 |
| **不挂 hook** | 不修改游戏任何代码段 |
| **不绑版本** | 关键地址启动时用**特征码现场扫描**，扫不到可用外部文本兜底 |

代价是**画面类功能天然做不到**（不注入就没有绘制入口、也调不了游戏内部函数），详见 [已知限制](#已知限制)。

---

## 快速开始

1. 从 [Releases](https://github.com/fmc999/GTA5-DMA-CHEAT/releases/latest) 下载 `GTA5-DMA-vX.Y-win-x64.zip`
2. 解压到任意目录 —— **不要只拿 exe**，随包的几个 dll 必须与 exe 同目录
3. 目标机进游戏 → 辅机运行 `GTA5_DMA.exe`
4. 第一次用先跑一次自检：

```powershell
GTA5_DMA.exe --health
```

> `--health` 逐项**实测**关键能力（DMA 连接 / 玩家池 / 载具池与模型链 / 脚本全局 / Tunables / BE 查询……），
> 哪项坏了直接告诉你**该改哪个文件**；结果同时写入 `GTA5_DMA_health.txt`。

---

## 界面预览

![控制台](docs/screenshot-console.png)

悬浮玻璃面板：页眉状态胶囊（DMA / 进程 / 热键 / FPS）、左侧导航、右侧分组卡片、底部状态栏（PID / 基址 / 模型哈希）。
面板可拖动缩放，几何与外观记在 `%LOCALAPPDATA%\GTA5_DMA\window.ini`；**面板以外刻意保持纯黑**（融合器拼接用，不是渲染失败）。

![战局载具](docs/screenshot-vehicle-list.png)

---

## 功能一览

### 战局玩家

| 能力 | 说明 |
|---|---|
| 玩家列表 | **9 列**：序号 / 名称 / RID / 血量 / 护甲 / 距离 / **载具** / **BE** / 状态；数字列右对齐 |
| 加密池扫描 | 按 YimMenuV2 的 `PoolEncryption`（rotl64）解密后遍历 `fwBasePool` |
| **BE 封禁自动查询** | 进战局后自动为每个玩家查 BattlEye 状态并标在「BE」列（`封` / `正常` / `排队`），结果落盘 `be_bans_cache.json`，24 小时内复用 |
| BE 查询机制 | 走 **BattlEye 官方服务端库**（`BEServer_x64.dll`）——纯本地、无浏览器、与游戏版本无关；并行 10 路，实测 20 人约 70 秒查完，封禁理由与第三方站点逐字一致 |
| 操作 | 传送到玩家（我→他）/ 拉到我这里（他→我）/ 击杀；传送错开 2 米防卡模，页面显示落点与读回校验 |
| 其它 | 名字搜索过滤；玩家加入 / 离开通知（默认关闭，设置页可开） |

### 战局载具

| 能力 | 说明 |
|---|---|
| 载具列表 | 载具池实时扫描 + 50 米半径过滤，显示车名 / 血量 / 距离 |
| 车名识别 | 内置 **921 条官方全量表** + `vehicle_names_extra.txt` 运行时补充（新 DLC 车写一行即可，**不用重编译**）；汉字字形完整内嵌 |
| 操作 | 一键传送到该载具（把玩家送到车旁，错开 2 米防卡模） |

### 人物控制

玩家无敌 / 载具无敌（持续状态保护） · 永不通缉 · 自动生命恢复 · 自动防弹衣刷新 · 隐身 · 无碰撞 · 速度控制（含野兽模式） · 生命值与防弹衣锁定

### 载具编辑

| 能力 | 说明 |
|---|---|
| 健康值 | 载具 / 引擎 / 车身 / 油箱四段读取与修改 |
| 一键修复 | 写满四段健康值（各 1000）并**逐字段读回校验**；可只修当前载具，或一键修 50 米内全部战局载具；血量低于 80% 时自动修复（1.5 秒节流） |
| 操控与附加 | 操控数据（加速 / 刹车 / 牵引 / 悬浮）实时编辑 · 附加能力 · 降落伞 · 喷气 · 跳跃恢复 · 导弹锁定范围 · 安全带 |

### 经济与自动化

| 能力 | 说明 |
|---|---|
| 产业保险箱 | 7 个产业（夜总会 / 游戏厅 / 事务所 / 车场 / 保释所 / 服装厂 / 洗车行）一键领取，支持周期自动领取（默认 30 秒） |
| 抢劫分账 | 公寓 / 末日 / 赌场三张图的四人分账写入；**二次确认** + 「写入记录」——只在按按钮时写，绝不后台持续写 |
| 金额类 | 佩里克岛主目标价值、跳伞挑战奖励「原值 × 倍率（1~10×）」写入，关闭即还原 |
| 静音来电 | 读「来电 / 通话中 / 电话状态」三个脚本格，条件成立写成 6（静音） |
| GTA+ 解锁 | 写 `GTA_PLUS_ENABLED` / 权益位 + 引擎侧标志，关开关或退出时还原 |
| Tunable（进度与解锁） | RP 倍率（`XP_MULTIPLIER`）· 改外貌免冷却 / 免费 · 防挂机踢出（`IDLEKICK_*` / `ConstrainedKick_*`）；页面右侧常显 tunable 实读值，便于核对定位 |
| 只读自检 | 脚本线程枚举（名字 / hash / 栈基址），供脚本 locals 通道使用 |

### 武器 / 传送 / 界面

- **武器**：当前武器属性读取（伤害 / 射速 / 射程 / 后坐力 / 精度）· 无限弹药 · 无需装弹 · 冲击力 · 子弹速度
- **传送**：自定义坐标 + **55 个预设点位**（8 组：通用 / 赌场系列 / 佩里克岛系列）；坐标 8 米内的重复点位自动合并；`F5` 地图标记点 · `F6` 任务点
- **界面**：悬浮玻璃面板 · 壁纸与中文字形完整内嵌 · 明暗模式与强调色 · Toast 反馈 · 双端热键

### 写入安全（所有写入类功能共有）

- **合法值区间体检**：读到区间外的值即判「该条目未定位」，**绝不写入**
- **写后读回校验**：写进去再读出来比对，失败会明确报错
- **脉冲动作自动还原**：一次性动作 1.5 秒后自动还原
- **一键关停**：`--all-off` 或界面「系统设置 → 安全 → 全部关停」→ 关停所有持续写入开关**并清零游戏内存里的对应位**（读回校验）

---

## 长期可用：游戏更新后不用重编译 ⭐

这是本项目的核心设计目标 —— **一次搞好，长期能用**。

### 四类数据，四种活法

| 类别 | 例子 | 游戏更新后 |
|---|---|---|
| **寻址方式** | `GlobalPtr` / `LocalScriptsPtr` / `GTAPlusPtr` 等指针 | **自动重解析**：每条内置 4 个备选特征码，逐条扫描 + 落点校验 |
| **运行时数据** | tunable 索引、脚本全局分块、线程名单与栈地址 | **每次启动重算**：靠名字的 `joaat` 哈希（哈希不随更新变化）查 `tunables.bin`；再兜底「值序列自发现」 |
| **结构体字段与脚本索引** | `PedVehiclePtr` / `VehicleModelInfo` / `ModelInfoHash` / `VehicleHealth` / 电话脚本索引… | **改文本即可**：见 `GTA5_DMA_offsets.txt` |
| **功能逻辑** | 新功能、新页面 | 才需要重新编译 |

### 四条外部通道（都在 exe 同目录，改文本、不动代码）

| 文件 | 用途 |
|---|---|
| `GTA5_DMA_offsets.txt` | **结构体字段偏移 / 脚本索引**；不存在时自动生成一份**带中文注释的当前实测值**，机械式改数值即可 |
| `GTA5_DMA_patterns.txt` | 追加特征码（`GlobalPtr = 48 8B 0D ? ? ? ? \| disp=3 insn=7`）——内置候选全失配时用 |
| `GTA5_DMA_tunables.txt` | 手填「名字 = 索引」覆盖；也可写 `bin = <路径>` 指向别的 `tunables.bin` |
| `GTA5_DMA_diag.txt` | 每次启动自动写出的诊断报告：解析到了什么、哪些条目没定位、为什么 |

### 一条命令看健康度

```
GTA5_DMA.exe --health
```

11 项**功能性实测**（不是"看代码里有没有值"）：DMA 连接 / 世界与本地玩家 / 脚本全局 / Tunables / 玩家池 /
载具池与模型链 / 脚本线程表 / 静音来电 / BE 查询 / 偏移覆盖表 / 外部覆盖文件 —— 每项坏在哪里、该改哪个文件都直接写出来。

配套文档：**[《更新自救说明.txt》](更新自救说明.txt)**（游戏更新后的自助流程，一页）。

### 值序列自发现（兜底机制）

一组已知值（如佩里克六个主目标价值 `400000/560000/616000/910000/1100000/1900000`）在 tunable 块里**连续且唯一**地出现——
即使索引整体挪走，也能反推出正确位置。实测：把索引故意写错，程序体检拒绝后靠唯一性自动找回。

---

## 命令行

| 命令 | 作用 |
|---|---|
| `--health` | 长寿命体检（11 项实测 + 逐项修法），同时写 `GTA5_DMA_health.txt` |
| `--all-off` / `--god-off` | **一键关停所有写入类开关**并清零游戏内存对应位（读回校验） |
| `--gate-drill` | 写入闸门演练：制造一次真漂移，验证「自动重新解析 + 重建基线」生效 |
| `--ban-check <RID>` | 查单个 RID 的 BattlEye 封禁状态 |
| `--diag [路径]` | 导出诊断报告（偏移解析 / tunable 定位 / 全局动作格 / 脚本线程） |
| `--vehicles-probe` | 载具池与车名解析自检 |
| `--phone-probe [--write-test]` | 静音来电的脚本全局解析（可加写入往返测试） |

> 工具**常驻运行**时 FPGA/FTDI 设备被独占，外部程序初始化不了设备 —— 所以诊断由运行中的实例自己落盘，
> 这正是 `--diag` 与 `GTA5_DMA_diag.txt` 存在的理由。

### 快捷键

| 按键 | 功能 |
|---|---|
| `Insert` | 显示 / 隐藏控制台 |
| `F5` | 传送到地图标记点 |
| `F6` | 传送到任务点（Enhanced） |
| `End` | 退出程序 |

---

## 环境要求

**硬件**：FPGA DMA 卡（实测 75T 系列）+ 两台机器（辅机跑工具，目标机跑游戏）

**运行（辅机）**：
- Windows x64
- 随包运行库：`leechcore.dll` / `vmm.dll` / `FTD3XX.dll` / `FTD3XXWU.dll` —— **必须与 exe 同目录**
- BE 封禁查询额外需要 `BEServer_x64.dll` + `BEServer_x64.cfg`（随包提供）

**开发（想自己编译才需要）**：Visual Studio 2022 或更高（含 C++23 工作负载）+ Windows SDK 10.0+；
Dear ImGui 源码与 MemProcFS 头文件 / 导入库已内置在仓库中。

---

## 构建与测试

```powershell
# 构建
MSBuild.exe GTA5_DMA\GTA5_DMA.sln /t:Build /p:Configuration=Release /p:Platform=x64

# 契约测试（UI 标签与排版、偏移解析确定性、各功能生命周期、tunable / 脚本全局 / 脚本线程 / 诊断）
Get-ChildItem tests/*.ps1 | ForEach-Object { powershell -File $_.FullName }   # 当前 19/19 通过

# 基础设施测试（PatternScanner / MemoryBackend / OffsetResolver 纯逻辑断言）
tests\x64\Release\DmaInfrastructureTests.exe
```

推送 `main` 会自动触发 [MSBuild workflow](.github/workflows/msbuild.yml)（CI 同时跑上面两套测试）。

<details>
<summary><b>项目结构与架构</b></summary>

```text
GTA5_DMA/
├── GTA5_DMA/
│   ├── Core/          # DMA 生命周期、内存后端(VMMDLL+Scatter)、特征码扫描与偏移解析、
│   │                  # 运行时表、诊断落盘、Reclass.h 结构定义、输入管理
│   ├── Features/      # 功能模块（各实现 OnDMAFrame()）：玩家/载具列表、无敌、修复、Tunables、
│   │                  # 脚本全局安全写层、经济与自动化、静音来电、BE 查询、传送、武器、载具编辑…
│   ├── UI/            # DX11+Win32 平台层、玻璃面板外壳、主题、壁纸、字体与字形范围、Toast、页面内容
│   ├── assets/  tools/  Attic/   # 壁纸与字体、生成脚本、已停用功能源码存档
├── ImGui/             # Dear ImGui 源码
├── MemProcFS/         # VMMDLL 头文件与导入库
tests/                 # 契约测试（PowerShell）+ 基础设施测试（C++）
docs/                  # 设计文档与截图
```

```text
main.cpp
 ├─ UI 线程 ─── MyImGui::OnFrame ─── ConsoleShell::Render ─── 各功能页面
 └─ DMA 线程 ── DMA::DMAThreadEntry
                ├─ ResolveRuntimeOffsets()   # 启动时特征码解析全部关键指针
                ├─ UpdateEssentials()        # Scatter 批量刷新指针链
                └─ 各 Feature::OnDMAFrame()  # 功能轮询读写
```

UI 与 DMA 线程完全分离、通过原子变量通信（无锁）；所有内存访问统一走 `MemoryBackend`，热路径用 Scatter 批量读写降低 PCIe 带宽占用。

</details>

---

## 常见问题

**游戏更新了，还能用吗？**
先跑 `--health`。大概率**直接能用**（地址是现场扫的）；若某项 ✗，按它给的提示改对应文本文件即可，不必等新版本。
完整流程见 [《更新自救说明.txt》](更新自救说明.txt)。

**为什么诊断文件要程序自己写，不能手动导出？**
工具常驻时 DMA 设备被独占，别的进程初始化不了设备 —— 所以由运行中的实例自己写 `GTA5_DMA_diag.txt`。

**界面里个别车名显示成问号？**
车名是运行时动态文本，字形需要事先压进字体图集。本项目已把 921 条官方车名 + 补充名表的全部汉字纳入字形范围；
新 DLC 再加车时，把车名写进 `vehicle_names_extra.txt` 并重新生成字形范围即可（`tools/gen_glyph_ranges.py`）。

**怎么确认某个开关没有偷偷写内存？**
所有写入都有读回校验提示；一次性写入（如抢劫分账）带**二次确认**与「写入记录」，不会后台持续写。
任何持续写入的开关都能被 `--all-off` 或界面「全部关停」一键关停并清零。

**某个功能没反应？**
先 `--health`；再确认游戏处于**线上战局**（玩家 / 载具池类功能在单人模式或加载中读不到）；仍不行就看 `GTA5_DMA_diag.txt`。

---

## 已知限制

**纯 DMA 的硬边界**（不是没做，是做不到）：

- **画面类**：ESP 透视、方框绘制 —— 不注入就拿不到绘制入口
- **调用游戏原生函数**：凭空刷车、叫技工、个人载具请求 —— 全部走 `CREATE_VEHICLE` 等内部原生调用
- **hook 类**：任何需要改写游戏代码或替换函数的行为

**其它**：

- 模型信息表（`CBaseModelInfo`）是**按哈希查找的容器里单个分配**，不是连续数组 —— 所以"把任意型号套到我的车上"只能复用战局里已有的型号
- 网络时间 / 游戏时钟不在脚本全局区与模块数据段（在堆上），因此依赖它们的「雷达隐身 / 时间控制」一直**未定位即拒写**
- 依赖战局的功能（玩家池 / 载具池）在单人模式或加载中读不到
- 「传送到载具」若目标车正在移动，落点按读取瞬间坐标计算，可能落在车后
- 面板以外刻意保持纯黑（融合器叠加用）
- 已停用功能（时间控制 / 任务分红 / 追战局）的源码保留在 `Attic/`，未编译进程序
- **无 BattlEye 主动绕过**；在线行为不作任何保证

---

## 免责声明

本项目仅供**技术研究、软件开发与 DMA 读写学习**使用，请在**单人模式 / 自建房间**中测试。
使用者应自行确认并遵守所在地区法律、游戏平台规则与服务条款。
游戏更新后内存结构可能变化，错误偏移可能导致功能异常或目标进程崩溃。
作者不对账号、硬件、数据或其他直接及间接损失负责。
**免费发布，请勿贩卖。**

## 致谢

- [MemProcFS](https://github.com/ufrisk/MemProcFS) / [LeechCore](https://github.com/ufrisk/LeechCore) — DMA 内存访问底座
- [Dear ImGui](https://github.com/ocornut/imgui) — 立即模式 GUI
- [ReClass.NET](https://github.com/ReClassNET/ReClass.NET) — 内存结构逆向
- [YimMenuV2](https://github.com/YimMenu/YimMenuV2) — 加密池扫描与结构思路参考（仅思路，未使用其代码）
- [AmIBattlEyeBanned](https://github.com/calamity-inc/AmIBattlEyeBanned) — BE 官方服务端库直查封禁的思路来源

## 许可

版权所有 © 2026 fmc999。免费发布，**禁止商业使用与未经授权的修改分发**，
详见 [LICENSE](LICENSE) / [LICENSE-zh-CN](LICENSE-zh-CN)。
