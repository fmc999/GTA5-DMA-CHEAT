# GTA5 DMA Control Console

基于 C++、MemProcFS、Dear ImGui 和 DirectX 11 构建的 GTA5 DMA 控制工具，支持 `GTA5.exe` 与 `GTA5_Enhanced.exe`。

> bilibili：一只小微凉鸭
>
> 本项目免费发布，请勿贩卖。

## 重要说明

- 本项目仅用于技术研究、软件开发和 DMA 读写学习。
- 使用者应自行确认并遵守所在地法律、游戏平台规则与游戏服务条款。
- 游戏更新后，内存结构和偏移可能失效。错误偏移可能导致功能异常或目标进程崩溃。
- 作者不对账号、硬件、数据或其他直接及间接损失负责。

## 当前功能

### 人物控制

- 玩家无敌与载具无敌
- 永不通缉与自动生命恢复
- 隐身与无碰撞
- 人物速度控制
- 生命值与防弹衣锁定

### 载具编辑

- 载具基础属性读取与修改
- 引擎、车身、油箱和载具生命值管理
- 附加能力、降落伞、喷气和跳跃参数
- 导弹参数与操控数据编辑

### 武器功能

- 当前武器属性读取
- 伤害、射速、射程、后坐力和冲击力修改
- 无限弹药与无需装弹
- 其他武器实验功能

### 位置传送

- 自定义坐标和预设位置传送
- `F5` 传送到地图标记点
- `F6` 传送到任务点（Enhanced）
- 人物与载具状态自动处理

### 界面与运行

- 紧凑型 Dear ImGui 控制台
- DMA 读写线程与界面线程分离
- 自动识别 GTA5 原版和 Enhanced 进程
- 主机及目标机热键检测

时间控制、任务分红和追战局功能目前已从界面与 DMA 主循环停用，相关源码仍保留，便于后续维护。

## 环境要求

### 硬件

- 可用的 DMA/FPGA 设备
- 运行 GTA5 的目标 Windows 主机
- 运行本工具的 Windows 控制主机

### 开发环境

- Visual Studio 2022 或更高版本
- Desktop development with C++ 工作负载
- Windows SDK
- x64 编译环境

仓库已经包含项目使用的 Dear ImGui 和 MemProcFS 头文件及库文件。

## 构建

1. 打开 `GTA5_DMA/GTA5_DMA.sln`。
2. 将解决方案配置设为 `Release`。
3. 将平台设为 `x64`。
4. 执行“生成解决方案”。
5. 输出文件位于 `GTA5_DMA/x64/Release/GTA5_DMA.exe`。

命令行构建示例：

```powershell
MSBuild.exe GTA5_DMA\GTA5_DMA.sln /t:Build /p:Configuration=Release /p:Platform=x64
```

## 使用

1. 确认 DMA 设备和 MemProcFS 环境工作正常。
2. 在目标主机启动 GTA5 或 GTA5 Enhanced。
3. 在控制主机运行 `GTA5_DMA.exe`。
4. 等待顶部状态显示 DMA 和游戏进程已连接。
5. 通过左侧导航进入人物、载具、武器或传送页面。

### 快捷键

| 按键 | 功能 |
| --- | --- |
| `Insert` | 显示或隐藏控制台 |
| `End` | 退出程序 |
| `F5` | 传送到地图标记点 |
| `F6` | 传送到任务点（Enhanced） |

## 项目结构

```text
GTA5_DMA/
├── GTA5_DMA.sln          # Visual Studio 解决方案
├── GTA5_DMA/             # 主程序源码和项目文件
│   ├── DMA.*             # DMA 初始化与核心地址更新
│   ├── Offsets.h         # GTA5 与 Enhanced 偏移
│   ├── ConsoleShell.*    # 控制台整体布局
│   ├── ConsoleTheme.*    # 控制台主题和通用控件
│   ├── Teleport.*        # 传送功能
│   ├── VehicleEditor.*   # 载具功能
│   └── WeaponInspector.* # 武器功能
├── ImGui/                # Dear ImGui 源码
└── MemProcFS/            # MemProcFS 接口与依赖
```

## 偏移维护

游戏版本更新后，优先检查：

- `WorldPtr`
- `GlobalPtr`
- `BlipPtr`
- `PlayerMgrPtr`
- 人物、载具和武器结构字段

修改 `Offsets.h` 或结构定义后，应先验证只读数据，再启用写入功能。不要在未验证地址的情况下持续写入内存。

## 发布说明

- 不提交 `.vs`、`x64`、PDB、OBJ、TLOG 等本地生成文件。
- 仓库中的 CT 文件用于偏移研究和版本对照。
- 发布版本应先完成 x64 Release 构建和基础功能检查。

## 致谢

- [MemProcFS](https://github.com/ufrisk/MemProcFS)
- [Dear ImGui](https://github.com/ocornut/imgui)

## 许可与声明

具体许可条款见仓库中的 `LICENSE` 文件。

**免费发布，请勿贩卖。**
