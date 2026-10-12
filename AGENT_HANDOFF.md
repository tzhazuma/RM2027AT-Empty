# RM2027AT macOS 环境 — Agent 交接说明

> **用途**：把 2026-09-30 ~ 10-08 会话的全部上下文交接给其他 agent / 其他机器，可直接继续任务。
> **生成**：2026-10-08 ｜ **目标机器**：azuma 的 MacBook（Apple M3 Pro，macOS，arm64）

---

## 0. 任务目标与当前状态

原始目标：按压缩包 `RM2027AT_Keil环境安装包` 与指南，在 **macOS** 上配好 **VSCode 版 Keil（Keil Studio）+ OpenOCD** 等环境、尝试编译、调研 **ST-Link 在 macOS 的驱动**，**等板子到了尝试连接烧录**。

| 项 | 状态 |
|---|---|
| macOS 全套工具链（GCC/AC6/CLANG/CMSIS-Toolbox/OpenOCD/pyOCD/许可） | ✅ 完成并实测 |
| 三条构建路线编译（CMake+GCC、Keil Studio+pyOCD、CMSIS csolution 三编译器） | ✅ 全部通过（占用与 Windows 参考一致） |
| ST-Link macOS 驱动调研 | ✅ 结论：**无需驱动**（板载实为 CMSIS-DAP，OpenOCD/pyOCD 走 libusb；Windows 的 .inf 包在 mac 无用） |
| 无板仿真（Arm FVP / QEMU） | ✅ 实测可用（QEMU 原生；FVP 经 OrbStack） |
| **实机烧录验证** | ✅ **2026-10-09 完成**：xPack OpenOCD 烧录成功，`Device ID = 0x700a3249 (AT32F423CCT7)` / Flash 256 KiB / **Verified OK** / 复位运行（与 Windows 实测一致）；四路 TT 匀速固件另做寄存器级验证 |
| 四路 TT 电机匀速示例（2026-10-09 追加需求） | ✅ 曾写入 `robot_main.c` 并烧录、寄存器级验证（备份 `~/Robomaster/robot_main_tt_uniform.c.bak`） |
| **麦轮遥控底盘（2026-10-11 追加需求）** | ✅ 遥控改用 FlySky **T8FB + R8EF 的 SBUS**，框架同步官方最新版（新增 `remote.c/sbus.c`）；编译 FLASH 44576 B / RAM 20976 B，**烧录成功**（`Device ID 0x700a3249` / Verified OK）；**SBUS 收帧实测通过**：`g_remote.sequence` 持续增长、`valid/online=true`，帧长 100% 为 25B；CH5 上位=解锁 |

> 原始目标全部完成。后续如需继续：见 §7 待办（文档/打包同步、DJI demo 还原、位置环示例入库等）。

---

## 1. 环境全貌（已装好，位于 `~/toolchains`）

| 工具 | 版本 | 路径 |
|---|---|---|
| Arm GNU Toolchain（GCC 15.3.1 + gdb） | 15.3.rel1 | `~/toolchains/arm-gnu-toolchain-15.3.rel1-darwin-arm64-arm-none-eabi/bin` |
| ATfE（LLVM/clang） | 23.1.0 | `~/toolchains/ATfE-23.1.0-Darwin-universal/bin` |
| Arm Compiler for Embedded（AC6/armclang，x86_64 走 Rosetta） | 6.24 | `~/toolchains/armclang-6.24/bin` |
| CMSIS-Toolbox（cbuild/csolution/cpackget） | 2.15.0 | `~/toolchains/cmsis-toolbox/cmsis-toolbox-darwin-arm64/bin` |
| xPack OpenOCD（**必须 xPack 版**，含 Artery 脚本） | 0.12.0-7 | `~/toolchains/xpack-openocd/xpack-openocd-0.12.0-7/bin` |
| pyOCD（venv） | 0.45.1 | `~/toolchains/pyocd-venv/bin` |
| Arm Debugger（armdbg，可选） | 6.11.0 | `~/toolchains/armdbg-6.11.0/ArmDebugger/bin` |
| AT32F423 器件包 | 2.1.4 | `~/Arm/Packs/ArteryTek/AT32F423_DFP/2.1.4` |
| 旧版 GCC（参照用，可选删） | 14.2.rel1 | `~/toolchains/arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi` |

**环境变量**：写在 `~/.zshrc` 与 `~/.bash_profile` 的 `RM2027AT toolchain (macOS)` 块（PATH、`CMSIS_PACK_ROOT`、`GCC_TOOLCHAIN_15_3_1`、`CLANG_TOOLCHAIN_23_1_0`、`AC6_TOOLCHAIN_6_24_0`）。**新终端生效**。

**许可**：MDK-Community（非商用免费）。`armlm` 在 `~/.local/bin/armlm`（软链到 VSCode 扩展内）。
- 有效至 2033-05-31；**本地缓存 7 天**——当前缓存 **2026-10-14 到期**，续期：`armlm reactivate -product KEMDK-COM0`
- `armlm inspect` 查看状态

**VSCode**：用户装的是 **Insiders**（命令 `code-insiders`）。已装扩展：`arm.keil-studio-pack`、`marus25.cortex-debug`、`llvm-vs-code-extensions.vscode-clangd`、`ms-vscode.cpptools-extension-pack`、`ms-vscode.cmake-tools`、`llvm-vs-code-extensions.lldb-dap`、`vadimcn.vscode-lldb`。
Insiders 用户设置已加：`arm-debugger.debuggerPath`、`lldb-dap.executable-path`（`~/Library/Application Support/Code - Insiders/User/settings.json`，有 .bak 备份）。

`qemu-system-arm` 11.1.2（brew）已装；`orb`（OrbStack 2.2.3）已装，内含 arm64 Ubuntu（机器名 `ubuntu`）与 amd64 `ubuntu-x64`。

---

## 2. 工程状态

**目录**：`~/Robomaster/RM2027AT_Keil环境安装包/project/RM2027AT-Empty`
**基线**：`tzhazuma/RM2027AT-Empty @ ccdb577`（2026-09-30，官方 `cxielumo/RM2027AT-Empty` 的 fork）。
**2026-10-11 已同步官方 `cxielumo/RM2027AT-Empty` 最新版**（新增 SBUS：`Components/{Inc,Src}/sbus.*`、`remote.*`、`project/{src,inc}/wk_i2c.*`；`main()` 改用 `remote_init()`）。同步用 `codeload.github.com` 下 tarball（直连 github.com 走代理会超时，codeload 可用）。

**团队适配（每次同步上游新代码后都要重新套用）**：
1. `ThirdParty/FreeRTOS/MDK-ARM/port.c`、`portmacro.h`：加 `#elif defined(__GNUC__)` 分支（指向 `../Source/portable/GCC/ARM_CM4F/...`）——给 cbuild 的 GCC/CLANG 用
2. `project/src/sysmem.c`：补 `#include <stddef.h>`（GCC15/LLVM 头文件差异）
3. `project/MDK_V5/`：`RM2027AT.csolution.yml`、`RM2027AT.cproject.yml`、`RM2027AT_RM2027AT.sct`、`RTE/`、`artifacts/`、`vcpkg-configuration.json`（cproject 的 Components 组必须包含新驱动 `daemon.c/motor.c/motor_dji.c`）
4. 工程根：`pyocd.yaml` + `packs/ArteryTek.AT32F423_DFP.2.1.4.pack`、`vcpkg-configuration.json`

**上游同步流程**（下次框架更新时照做）：
```bash
cd /tmp && rm -rf new && git clone --depth 20 https://github.com/tzhazuma/RM2027AT-Empty.git new
# 备份当前工程，然后用 rsync 覆盖（保留 .git 之外全部）
rsync -a --exclude '.git/' /tmp/new/ "<工程目录>/"
# 重新套用上面 4 项适配，然后按 §3 全部重编译
```

**.vscode**：本机工作副本已被 `setup_macos.sh --patch-only` 改成 mac 绝对路径（serverpath/gdbPath/烧录命令），有 `.bak`；**分发的 zip 内是团队原版 Windows 路径**（mac 用户跑脚本自动改）。

**最近一次构建数值**（`ccdb577` 基线）：

| 路线 | 结果 |
|---|---|
| CMake+GCC 15.3.1 Debug / Release | FLASH 44284 B / 25140 B |
| cbuild AC6 6.24 | text/data/bss = 26276 / 104 / 22672 |
| cbuild GCC 15.3.1 | 59736 / 3292 / 25072 |
| cbuild CLANG 23.1.0 | 60400 / 3236 / 22708 |

---

## 3. 三条路线怎么跑（复制即用）

```bash
# ── 路线一：CMake + GCC（推荐，F5 调试）────────────────────────
cd ~/Robomaster/RM2027AT_Keil环境安装包/project/RM2027AT-Empty
cmake --preset Debug && cmake --build build/Debug -- -j8     # 产物 build/Debug/RM2027AT.elf

# ── 路线三：CMSIS csolution 三编译器 ──────────────────────────
cd project/MDK_V5 && rm -rf tmp out
cbuild RM2027AT.csolution.yml --context "RM2027AT+RM2027AT"                       # AC6
cbuild RM2027AT.csolution.yml --context "RM2027AT+RM2027AT" --toolchain "GCC@15.3.1"
cbuild RM2027AT.csolution.yml --context "RM2027AT+RM2027AT" --toolchain "CLANG@23.1.0"
# ⚠ 若 AC6 报 L3900U（-L/opt/homebrew/opt/bison/lib）→ 当前 shell 有 LDFLAGS 泄漏，用 env -u LDFLAGS 或注释 ~/.zshrc 那行

# ── 烧录（板子到了就跑这两条）────────────────────────────────
openocd -f openocd.cfg -c "program build/Debug/RM2027AT.elf verify reset exit"
pyocd flash build/Debug/RM2027AT.elf        # 或 VSCode F5（cortex-debug 配置已就绪）

# ── 无板仿真 ──────────────────────────────────────────────────
cd ~/Robomaster/RM2027AT_Keil环境安装包/examples/fvp-qemu-hello
bash run.sh          # QEMU 原生（mps2-an386，需 brew install qemu）
bash run.sh --fvp    # OrbStack 里的 Arm FVP（需先在 VM 内装 FVP，见附录 A.6）
bash run.sh --gdb    # QEMU + GDB 断点调试
```

---

## 4. 已完成验证 / 待验证

**已验证**（证据见主文档 §10、附录 A.4）：
- 三种编译器全部编译通过，占用与 Windows 参考逐项一致
- gdb 断点可解析（`robot_main.c` 精确到行）；`pxCurrentTCB`/`uxTopUsedPriority` 符号齐全（FreeRTOS 线程视图可用）
- pyOCD 解析本地 Pack（`_at32f423cct7` 等 22 个 AT32F423 目标）；OpenOCD 配置链路正确（无板时报 "unable to find a matching CMSIS-DAP device" 属正常）
- QEMU 原生跑通最小固件 + GDB 断点命中；Arm FVP 在 OrbStack 里跑通（含许可）
- **实机烧录（2026-10-09）**：`openocd -f openocd.cfg -c "program build/Debug/RM2027AT.elf verify reset exit"`
  → `SWD DPIDR 0x2ba01477` / `Device ID = 0x700a3249 (AT32F423CCT7)` / Flash 256 KiB / **Verified OK** / 复位运行
- **TT 匀速固件寄存器级验证**：TMR4/TMR3/TMR1 `tmren=1`；比较值 2250/2250/6000/2250 = 30% 占空比；IN2 通道为 0（正向）
- **麦轮遥控固件实机烧录（2026-10-11）**：同一命令 → `Device ID 0x700a3249` / **Verified OK**；在线读回 PC=`0x08009214`（在固件区）、四路 PWM 比较值全 0（无遥控输入时安全静止）。探针卡死用 **`tools/reset_probe.py`（pyusb USB 复位）**恢复，无需拔线（见下）
- **SBUS 遥控收帧（2026-10-11）**：框架同步官方最新版（`remote.c/sbus.c`）后，`g_remote.sequence` 持续增长、`valid/online=true`、`frame_lost=failsafe=false`，帧长统计 100% 为 25 字节。**中途踩坑**：`rsync -a` 保留旧 mtime → Ninja 漏编 `main.c`/`dbus.c`，固件走旧 DBUS 路径（见 §5.10）；全量重建后解决

**踩坑（2026-10-09/10-11 实遇，务必记住）**：探针枚举正常（CDC/HID 都在）但 CMSIS-DAP 命令全部超时
（OpenOCD 报 `CMD_INFO failed`、pyOCD 报 `already open`/`Timeout reading from probe`）→
**首选软件复位** `python3 tools/reset_probe.py`（pyusb 发 USB reset，秒级恢复，**不必拔线**；卡死期间反复连它只会加重占用，出现 `unable to open ... 0xc251:0xf001`）；仍不行再拔插 USB 断电重启。
另外 `adb`/Android 模拟器会抢占 USB 设备，调试前先 `adb kill-server` 并退出模拟器。
该探针是 CDC+HID 复合设备（Keil VID `0xC251:0xF001`），**没有 v2 bulk 接口**，不要强制 `cmsis-dap backend usb_bulk`。

---

## 5. 踩坑清单（按重要性）

1. **不要用 brew 的 `arm-none-eabi-gcc`**（16.2 是 Homebrew 自编上游 GCC，**缺目标端 libc 头文件**，本工程编不过）→ 用 Arm 官方 15.3
2. **不要用 brew 的 `openocd`**（缺 `target/artery/at32f4x.cfg`）→ 用 xPack
3. `~/.zshrc` 里 Homebrew 遗留的 `export LDFLAGS="-L/opt/homebrew/opt/bison/lib"` 会泄漏进 armclang → **已注释**（注释说明保留在文件里）；如复现，`env -u LDFLAGS cbuild ...`
4. **armdbg 内置目标库不含 ArteryTek** → Keil Studio 调试请用 **pyOCD 后端**（或 cortex-debug+OpenOCD）
5. **IAR（cxarm）无 macOS 版**；**Arm FVP 无 macOS 原生版**（闭源 Linux 二进制，无法移植）→ FVP 用 OrbStack；日常用 **QEMU 原生**（mps2-an386 与 FVP_MPS2_Cortex-M4 是同一块参考板）
6. **CAN 波特率**：框架实测为 **1 Mbps**（APB1 75MHz ÷ 5×(1+11+3)），正合大疆电机要求；`docs/板级验证记录.md` 里"500kbps"是**过时描述**，勿被误导
7. `setup_macos.sh` 的 `--patch-only` 只改 `.vscode` 路径、不装工具（工具未装会 die 并提示）
8. 打包/解压中文路径用 `ditto`（`unzip` 会报 Illegal byte sequence）
9. **DAPLink 探针卡死**（OpenOCD `CMD_INFO failed`、pyOCD `already open`）：先 **`python3 tools/reset_probe.py`**（pyusb 发 USB 复位，秒级恢复，无需拔线），不行再拔插断电；卡死期间反复连它只会加重占用
10. **同步框架后必须全量重建**（2026-10-11 实遇，极隐蔽）：`rsync -a` 保留上游旧 mtime → Ninja 认为源码"比 .obj 旧" → **漏编**。表现：编译零报错、烧录 `Verified OK`，但功能静默失效（本次 `main.c` 仍是旧的 `dbus_init()`，SBUS 25B 帧全被丢）。判据：`nm <obj> | grep remote_init` 应有 `U remote_init`；FLASH 占用应随源码变化。做法：`rm -rf build/Debug && cmake --preset Debug && cmake --build build/Debug`
11. **探针必须直插 Mac 机身 USB 口**：挂在外接扩展坞（`USB2.0 Hub`）后面时，坞本身会周期性闪断，探针几乎必卡死；直插时明显稳定

---

## 6. 大疆电机（M3508/M2006/GM6020）知识与代码框架

> 来源：飞书《电控培训4-pid》（Magician 战队，公开可读）+ 框架 `docs/大疆电机驱动.md` + 源码实测。

### 6.1 与普通 TT 电机的区别

| 维度 | TT 电机（板载 6 路） | 大疆电机 |
|---|---|---|
| 类型 | 有刷直流减速电机 | 无刷直流 + 减速箱（M3508 约 19:1） |
| 驱动 | RZ7889 H 桥，PWM+方向 | CAN → 电调(C610/C620) → 电机 |
| 反馈 | 无（开环） | 编码器：转速/角度/电流/温度 |
| 控制量 | 占空比 | **电流（≈力矩）**，闭环 |
| 框架 API | `motor_tt_enable/setDuty/disable` | `motor_dji_register/setCurrent/stop` + `measure` |

### 6.2 PID 的作用（速度环）
电流 ∝ 力矩 ∝ **加速度**（不是速度）；要匀速必须让电流恰好抵消摩擦/负载，但那是未知且变化的量 → 只能**测转速、按误差自动修电流**。框架示例（`robot_main.c`）：
`pid_step(&speed_pid, target_rpm, motor.measure.speed_rpm, 0.01f, &out)` → `motor_dji_setCurrent(&motor, (int16_t)out)`。
示例参数 kp=2/ki=0/kd=0 是**纯 P**（会有稳态误差，实际需加 I 调 Kp）。

### 6.3 固定角度 = 串级 PID（位置环→速度环→电流环）

⚠️ **该示例只在 /tmp 里编译验证过（FLASH 45148 B），/tmp 已清理，尚未写入工程**。完整代码如下，可直接替换 `Application/Src/robot_main.c` 后重编译：

```c
/* 固定角度（位置保持）：位置环(外) → 速度环(内) → 电流。目标角度相对上电零点。 */
#include <math.h>
#include "os.h"
#include "motor_dji.h"
#include "pid.h"

static motor_dji_t motor;
static pid_t pos_pid;    /* 外环：角度 counts → 目标转速 rpm */
static pid_t speed_pid;  /* 内环：转速 rpm   → 电流指令      */

#define TARGET_COUNTS    8192.0f /* 转子整 1 圈（8192 counts/圈；输出轴约 1/19.2 圈） */
#define DEADBAND_COUNTS  16.0f   /* 到位死区（约 0.7° 转子角），防抖动 */

void robot_main(void *args)
{
    const motor_dji_config_t motor_config = {
        .id = 1, .bus = CAN_BUS_1, .model = MOTOR_DJI_M3508, .invert = false
    };
    const pid_config_t pos_config = {   /* 输出=目标转速 → 限幅即最大转速（安全） */
        .kp = 0.5f, .ki = 0.0f, .kd = 0.05f,
        .output_min = -300.0f, .output_max = 300.0f,
        .integral_min = -100.0f, .integral_max = 100.0f
    };
    const pid_config_t speed_config = { /* 输出=电流指令 → 限幅即电流上限 */
        .kp = 3.0f, .ki = 0.3f, .kd = 0.0f,
        .output_min = -3000.0f, .output_max = 3000.0f,
        .integral_min = -1000.0f, .integral_max = 1000.0f
    };
    float target_rpm = 0.0f, current = 0.0f;

    (void)args;
    (void)motor_dji_register(&motor, &motor_config);
    (void)pid_init(&pos_pid, &pos_config);
    (void)pid_init(&speed_pid, &speed_config);

    for (;;) {
        if (motor.measure.online) {
            float angle = (float)motor.measure.angle_counts;
            if (fabsf(TARGET_COUNTS - angle) < DEADBAND_COUNTS) {
                target_rpm = 0.0f;               /* 到位：让内环把速度拉回 0 */
                (void)pid_reset(&pos_pid, 0.0f); /* 清外环积分，防再启动冲过头 */
            } else {
                (void)pid_step(&pos_pid, TARGET_COUNTS, angle, 0.01f, &target_rpm);
            }
            (void)pid_step(&speed_pid, target_rpm,
                           (float)motor.measure.speed_rpm, 0.01f, &current);
            motor_dji_setCurrent(&motor, (int16_t)current);
        }
        os_delay(10);   /* 双环同频 100Hz；工程上常内环 1kHz、外环分频 */
    }
}
```

前提与注意：`angle_counts` 是**相对角度**（首帧/重连清零，回绕按 8192）；counts 是**转子侧**（输出轴角度 ÷19.2）；要绝对角度需找零流程或改用 **GM6020**（绝对编码器、直驱、精度高）；长时间运行后 counts 很大时 float 精度损失，需重标零位。

### 6.4 框架 API 速查（`Components/Inc/motor_dji.h`）

| API / 字段 | 说明 |
|---|---|
| `motor_dji_register(&motor, &cfg)` | cfg: `id`(C6x0 1..8 / GM6020 1..7)、`bus`、`model`、`invert`；首次注册启动服务任务（1kHz）；对象须 static |
| `motor_dji_setCurrent(&motor, ±16384)` | M3508/C620 与 GM6020 限 ±16384；M2006/C610 限 ±10000；**必须在线才生效**，100ms 无新命令自动清零 |
| `motor.measure.*` | `encoder_raw / speed_rpm / current_raw / temperature_c / angle_counts / online / enabled`（只读；C610 无有效温度） |
| `motor_dji_stop()` / `motor_shutdown()` | 停止单机 / 全部（TT+大疆） |
| `daemon_enable(ms)` + `daemon_reload()` | 看门狗：超时自动切断大疆；DBUS 数据有效时自动喂狗 |

CAN 协议（驱动已实现）：命令帧 `0x200`(ID1-4)/`0x1FF`(ID5-8)，GM6020 `0x1FE`/`0x2FE`，每帧 4 机×int16；反馈帧 `0x200+ID`（GM6020 `0x204+ID`）。同总线 C6x0 的 ID5..8 与 GM6020 的 ID1..4 反馈 ID 冲突，注册会被拒。

### 6.5 当前示例固件：麦克纳姆轮 SBUS 遥控（2026-10-11）

**遥控器换成了 FlySky T8FB + R8EF（SBUS）**，框架已同步到官方 `cxielumo/RM2027AT-Empty` 最新版：

- 新版把遥控统一到 `remote_init()`（`Components/Src/remote.c`），按包长自动分发：18B→DBUS，25B→SBUS；应用直接用 `sbus_get()`（`Components/Inc/sbus.h`）。`main()` 里已改为 `remote_init()`，`dbus_init()/sbus_init()` 不再对外。
- SBUS 与 DBUS 电气层相同（PA5/H1.7、100000、偶校验、反相），**接线不变**。
- T8FB 通道（Mode 2）：**CH1 右横、CH2 右纵、CH3 左纵、CH4 左横、CH5..CH8 拨杆**。
- 示例映射：CH3→前后、CH4→平移、CH1→旋转；**CH5 拨杆向上解锁**（否则停车）。麦轮反解同 §6.5 旧版。
- ✅ **实测（2026-10-11）**：`g_remote.sequence` 持续增长、`valid/online=true`；实物 `raw[1..8]` = 997/983/1032/1025/1800/1800/1800/200（摇杆居中 ≈991，拨杆两端 ≈1800/200）；摇杆居中时 `g_duty` 全 0。
- 全局 `g_remote`（`sbus_t`）与 `g_duty[4]` 供 SWD 在线读，调试通道/确认收帧。
- 本地适配（每次同步后重套）：`sysmem.c` 加 `#include <stddef.h>`；FreeRTOS `MDK-ARM/port.c`+`portmacro.h` 加 `__GNUC__` 分支；`project/MDK_V5/RM2027AT.cproject.yml` 加 `sbus.c/remote.c/wk_i2c.c`。
- 历史备份：`~/Robomaster/robot_main_dji_demo.c.bak`、`robot_main_tt_uniform.c.bak`。

---

## 7. 待办（下一步可做项）

- ❗ **[最高优先] 排查 `robot_main` 冻结**：干净版 SBUS 固件烧入后 `g_remote.sequence` 爬到约 684 后**冻结**、`online` 停在过期 `true`、`g_duty` 冻结在 0 → 推摇杆电机无反应。推断 `robot_main` 已不执行，疑似走到 `panic()`（`__disable_irq()` + `pwm_shutdown()`，正好解释"中断停 + 电机不动"）；可疑点是 `configASSERT`（`uart_receiveFrame` 槽位断言）或任务栈溢出。建议：读 `$pc`/`$primask`/`pxCurrentTCB->pcTaskName`；在 `panic()` 里加 `volatile fault_reason_t g_panic_reason` 记录原因后重烧读取；或先把 `robot_main` 栈从 768 words 调到 1024 试。
- [x] **板子到手后**：按 §3 烧录命令验证 → ✅ 2026-10-09 完成（`Device ID 0x700a3249` / `Verified OK`，见 §4）
- [x] **麦轮遥控固件烧录** → ✅ 2026-10-11 完成（`Device ID 0x700a3249` / `Verified OK`，见 §4）
- [x] **SBUS 收帧确认** → ✅ 2026-10-11 完成。根因是**新旧混编固件**（`rsync -a` 保留旧 mtime → Ninja 漏编 `main.c`/`dbus.c`，固件走旧 DBUS 路径）；`rm -rf build/Debug` 全量重建后 SBUS 全链路打通（见 §5.10、§6.5）
- [ ] （可选）把 §6.3 位置保持示例正式写入 `robot_main.c` 并重编译验证
- [ ] （可选）把无板仿真性能评测数据写进主文档附录 A.6：QEMU 原生紧凑循环 ≈ **2.4 GIPS**（MMIO 117 ns/次）；FVP（OrbStack）计算慢 4~5×、但 **MMIO 约 2× 更快**（58 ns/次）
- [ ] （可选）`armlm reactivate`（**10-14 缓存到期**）
- [ ] （可选）上游框架再次更新时按 §2 流程同步 + 重套适配

---

## 8. 关键文件与产物

| 路径 | 说明 |
|---|---|
| `~/Robomaster/RM2027AT_Keil环境安装包/` | 安装包工作目录（含 `setup_macos.sh`、`电控环境配置_VSCode版.md` 主文档、`examples/fvp-qemu-hello/`） |
| `.../project/RM2027AT-Empty/` | 工程（基线 ccdb577，本机已改 mac 路径） |
| `~/Robomaster/RM2027AT_Keil环境安装包_v5_20261011.zip` | 已打包分发版（14.2 MB；`.vscode` 为团队 Windows 原版） |
| `.../tools/reset_probe.py`、`.../tools/flash_macos.sh` | 探针 USB 复位 / macOS 一键烧录（先复位再 OpenOCD 烧录） |
| `~/toolchains/*` | 全部工具链（见 §1） |
| 主文档 §10 / 附录 A | 验证记录、macOS 全流程、踩坑表、FVP/QEMU 说明 |
| `docs/大疆电机驱动.md`（工程内） | 大疆驱动完整说明 |
| `/tmp/old-project-backup` | 上一版工程备份（**/tmp 可能被清理，不可依赖**） |

**重新打包命令**（供后续 agent 复现）：
```bash
STAGE=/tmp/pkg; rm -rf $STAGE; mkdir -p $STAGE
SRC=~/Robomaster/RM2027AT_Keil环境安装包
rsync -a --exclude 'build/' --exclude 'tmp/' --exclude 'out/' --exclude '*.bak' --exclude 'hello.elf' "$SRC/" "$STAGE/RM2027AT_Keil环境安装包/"
# 分发版 .vscode 用团队原版（从上游仓库取）：
# cp /tmp/new/.vscode/{launch,tasks}.json "$STAGE/RM2027AT_Keil环境安装包/project/RM2027AT-Empty/.vscode/"
cd $STAGE && ditto -c -k --keepParent "RM2027AT_Keil环境安装包" ~/Robomaster/RM2027AT_Keil环境安装包_v5_20261011.zip
# 校验：解压到 /tmp/verify 后 diff -rq 比对
```
