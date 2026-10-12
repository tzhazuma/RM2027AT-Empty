# RM2027AT 电控环境与工程 —— 打开先看我 / Agent 交接说明

> 面向两类读者：**① 拿到这个包的队友**（想跑起来）；**② 接手调试的 AI Agent**（想直接干活）。
> 最后更新：2026-10-12 ｜ 目标机型：macOS（Apple Silicon），Windows 亦可

---

## 0. 这个包里有什么

| 路径 | 说明 |
|---|---|
| `README_快速开始与Agent交接.md` | **本文件**，先读这个 |
| `电控环境配置_VSCode版.md` | 主文档：工具链、三条构建路线、烧录/调试、**附录 A = macOS 全流程实测**、附录 B = 鸿蒙 |
| `AGENT_HANDOFF.md` | **Agent 交接书**：环境全貌、工程状态、踩坑清单、大疆电机知识、待办、**当前未决问题** |
| `setup_macos.sh` | macOS 一键装工具链（或用 `--patch-only` 只改 `.vscode` 路径） |
| `setup_windows.ps1` / `setup_wsl.sh` | Windows / WSL 配置脚本 |
| `project/RM2027AT-Empty/` | **主工程**（框架 + 示例固件） |
| `tools/flash_macos.sh` | **macOS 一键烧录**（先给探针做 USB 复位，再 OpenOCD 烧写） |
| `tools/reset_probe.py` | 探针卡死时的 USB 软复位（比拔线快） |
| `examples/fvp-qemu-hello/` | 无板仿真（QEMU 原生 / Arm FVP 经 OrbStack） |
| `packs/`、`pyocd.yaml` | pyOCD 本地器件包方案 |
| `docs/电控环境配置_原版参考.pdf` | 原始课件（Windows 版参考） |

---

## 1. 给队友：最快跑起来（macOS）

```bash
# ① 一键装工具链（GCC15.3 / AC6 / CLANG / CMSIS-Toolbox / OpenOCD / pyOCD，约十几分钟）
bash setup_macos.sh

# ② 开一个新终端（让 PATH 生效），编译
cd project/RM2027AT-Empty
cmake --preset Debug && cmake --build build/Debug -- -j8

# ③ 插上板子/探针（直插 Mac 机身 USB 口！），烧录
bash tools/flash_macos.sh

# ④ 调试：VSCode 打开 project/RM2027AT-Empty → Run and Debug → "Debug RM2027AT with OpenOCD" → F5
```

**接线速查**

| 用途 | 接法 |
|---|---|
| 遥控接收机（SBUS/DBUS 都适用） | 信号 → **H1.7 (RX6/PA5)**，电源 → **H2 (+5V)**，地 → **H3 (GND)** |
| SWD 调试 | CN11：SWDIO / SWCLK / GND / 3V3 |
| 板载 TT 电机 | CN9=1 / CN5=2 / CN6=3 / CN7=4 / CN8=5 / CN10=6 |
| 大疆电机 | CN13-15 = CAN1，CN17-19 = CAN2 |

**⚠️ 三条必读的坑**

1. **同步/覆盖框架后必须全量重建**：`rm -rf build/Debug` 再编。本包同步过的工程已经全量重建过，但你若再覆盖源码，`rsync -a` 会保留旧 mtime 导致 Ninja **漏编**，产出"新旧混编"固件（编译零报错、烧录 Verified OK，但功能静默失效）。
2. **探针必须直插 Mac 机身 USB 口**，不要走外接扩展坞。探针卡死（`CMSIS-DAP command CMD_INFO failed`）时先跑 `bash tools/flash_macos.sh`（内含软复位），不行再物理拔插。
3. **不要用 brew 的 `arm-none-eabi-gcc` 和 `openocd`**（缺目标端 libc / 缺 Artery 脚本），用 `setup_macos.sh` 装的那套（GCC 15.3 + xPack OpenOCD）。

---

## 2. 给 Agent：如何接手（推荐流程）

1. **先读 `AGENT_HANDOFF.md`** —— 10 分钟掌握全部上下文（环境、工程、踩坑、待办、未决问题）。
2. **再读 `电控环境配置_VSCode版.md` 的附录 A** —— macOS 全流程、烧录/调试实测方法。
3. **工程内文档**：`project/RM2027AT-Empty/docs/` 下有 `功能引脚对应.md`、`接口使用说明.md`、`大疆电机驱动.md`、`SBUS遥控.md`、`框架设计方案.md`（权威接口与引脚说明）。
4. **在线调试手法**（本项目验证过的读全局变量方式）：
   ```bash
   # 终端 A：起 gdb server（工程根目录，pyocd.yaml 提供本地 Pack）
   openocd -f openocd.cfg
   # 终端 B：读运行中的全局变量（示例固件暴露了 g_remote / g_duty）
   arm-none-eabi-gdb -batch -q -ex "target extended-remote :3333" \
     -ex "p g_remote.sequence" -ex "p g_remote.channel[1]@8" -ex "p g_duty" \
     -ex "detach" build/Debug/RM2027AT.elf
   ```
   > 想看更多中间量，可临时在 `Bsp/Src/uart.c`、`Components/Src/remote.c` 加 `volatile` 计数器（用 SWD 读），**诊断完务必还原**。

**禁止 / 注意**
- 不要用 `adb` 或 Android 模拟器占着 USB（调试前 `adb kill-server`）。
- 不要强制 `cmsis-dap backend usb_bulk`（本探针是 CDC+HID 复合设备，无 v2 bulk 接口）。
- `.vscode/` 里是**团队 Windows 原版路径**，macOS 用户跑 `bash setup_macos.sh --patch-only` 自动改成 mac 路径。
- 打包/解压中文路径用 `ditto`（`unzip` 报 Illegal byte sequence）。

---

## 3. 当前状态

### ✅ 已完成并实测

| 项 | 状态 |
|---|---|
| macOS 全套工具链（GCC 15.3 / AC6 6.24 / CLANG 23.1 / CMSIS-Toolbox / xPack OpenOCD / pyOCD） | ✅ |
| 三条构建路线（CMake+GCC、Keil Studio+pyOCD、CMSIS csolution 三编译器） | ✅ 全部通过 |
| ST-Link / CMSIS-DAP 驱动调研 | ✅ 结论：macOS **免驱**（板载实为 CMSIS-DAP，走 libusb） |
| 实机烧录 | ✅ `Device ID 0x700a3249` / `Verified OK` |
| 框架同步官方最新版（新增 SBUS：`remote.c/sbus.c`） | ✅ |
| **SBUS 遥控收帧**（FlySky T8FB + R8EF） | ✅ `g_remote.sequence` 增长、帧长 100% 25B |
| 无板仿真（QEMU 原生 / FVP 经 OrbStack） | ✅ |

### ❗ 未决问题（接手 Agent 请优先看这里）

**现象**：烧入干净版 SBUS 麦轮固件后，`g_remote.sequence` 爬到约 **684 后冻结**，通道值不再变化；
`g_remote.online` 仍为 `true`（**过期值** —— 正常情况下 100 ms 无帧会被 `sbus_get()` 置为 false），
`g_duty` 冻结在 0 → 用户推摇杆**电机无反应**。

**推断**：`robot_main` 任务已不再执行（否则会周期性调用 `sbus_get()` 把 `online` 刷新为 false）。
最可能是走到了 `panic()`：`panic()` 会 `__disable_irq()` + `pwm_shutdown()`，正好解释
"中断停止（不再有新 SBUS 帧）+ PWM 关断（电机不动）"。可疑触发点：`configASSERT`
（例如 `uart_receiveFrame()` 的槽位断言）或任务栈溢出（`robot_main` 768 words / `uart_rx` 384 words，
`configCHECK_FOR_STACK_OVERFLOW=2`）。

**下一步建议**（需探针可用）：
1. 读 `p/x $pc`、`$primask`、`pxCurrentTCB->pcTaskName` —— 判断是否停在 `panic()` 的死循环、
   以及 `PRIMASK` 是否=1；
2. 在 `panic()` 里加一个记录 `reason` 的全局变量（`volatile fault_reason_t g_panic_reason;`），
   烧录后直接读，即可知道是 `FAULT_ASSERT` 还是 `FAULT_STACK_OVERFLOW`；
3. 检查各任务栈高水位：`p uxTaskGetStackHighWaterMark(...)` 或读 `pxCurrentTCB` 的栈填充；
4. 也可先在 `Application/Src/robot_main.c` 里把 768 words 调到 1024 试一下。

> 复现方式：`bash tools/flash_macos.sh` 烧入当前固件，遥控通电后等约 10 秒再读 `g_remote.sequence` 两次。

---

## 4. 工程内关键文件（改造点）

| 文件 | 说明 |
|---|---|
| `Application/Src/robot_main.c` | **示例固件**：麦克纳姆轮 SBUS 遥控（CH3 前后 / CH4 平移 / CH1 旋转 / **CH5 上位解锁**），含离线归零与斜率限制；暴露 `g_remote` / `g_duty` 供在线调试 |
| `Components/Src/sbus.c`、`remote.c` | SBUS 解码 + 统一遥控入口（按包长分发 18B DBUS / 25B SBUS） |
| `Components/Src/dbus.c`、`motor_tt.c`、`motor_dji.c`、`servo.c`、`daemon.c` | 框架组件（DBUS / 板载 TT 电机 / 大疆 CAN 电机 / 舵机 / 看门狗） |
| `packs/` + `pyocd.yaml` | pyOCD 本地器件包（免 `--pack`） |
| `project/MDK_V5/` | CMSIS csolution 工程（AC6/GCC/CLANG 三编译器） |

---

## 5. 一句话交给下一个 Agent

> 「请读 `AGENT_HANDOFF.md` 和 `电控环境配置_VSCode版.md` 附录 A，然后按 §3 的**未决问题**排查
> `robot_main` 冻结；烧录用 `bash tools/flash_macos.sh`，探针卡死先用 `tools/reset_probe.py`。」
