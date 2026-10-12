# SBUS 遥控

SBUS 和 DBUS 共用 USART6（RX：PA5），根据空闲中断接收到的包长度自动区分：18 字节按 DBUS 解析，25 字节按 SBUS 解析，其他长度丢弃。接收器信号线接 PA5，并与控制板共地。默认使用标准反相串口：100000 baud、8 位数据、偶校验、2 位停止位；芯片串口使用 9BITS 配合偶校验实现 8 位有效数据。接收器供电和信号电平应符合板卡要求。

## 初始化

启动代码包含 `remote.h`，在 `bsp_init()` 后初始化遥控接收：

```c
if (remote_init() != 0) panic(FAULT_INIT);
```

CMake 和 Keil 均已加入遥控源文件。`remote_init()` 统一初始化 DBUS/SBUS 接收，`remote.c` 持有 UART 回调并按包长度分发；`sbus_init()` 和 `dbus_init()` 为内部实现，不向应用公开。应用可分别使用 `sbus_get()` 和 `dbus_get()` 读取对应协议的快照，无需配置协议宏。两种快照分别维护有效性及超时状态。

应用选择 SBUS 后应包含 `sbus.h`，使用 `sbus_t` 和 `sbus_get()` 替换 DBUS 读取，并移除 `dbus_useUartInstead()` 调用，避免改变 SBUS 的串口配置。当前电机示例每 10 ms 读取 SBUS 快照到 `remote`，未把通道绑定到电机。

## 读取输入

```c
sbus_t remote;
if (sbus_get(&remote) == 0 && remote.valid && remote.online) {
    float command = remote.channel[1]; /* CH1，范围 [-1, 1] */
    /* 在此将 command 映射到目标速度或其他业务指令。 */
    (void)command;
} else {
    /* 在此停止输出或进入业务安全状态。 */
}
```

- `raw[1..18]`：CH1..CH16 的 11 位原始值（0..2047），CH17..CH18 的数字值（0 或 1）。`raw[0]` 和 `channel[0]` 留空，始终为零；两个数组均有 19 个元素。
- `channel[1..16]`：按常用的 172..1811 行程映射到 [-1,1]，越界钳位；中心 ±5% 内输出为零，死区外保持原映射。可在 `sbus.h` 调整 `SBUS_DEADZONE`（归一化宽度，设为 `0.0f` 禁用）。死区应用于全部 16 路，`raw` 不受影响。实际接收器行程不同可用 `raw` 自行标定。
- `channel[17]`、`channel[18]`：CH17、CH18 数字通道，值为 `0.0f` 或 `1.0f`，不应用模拟通道死区。全部通道统一使用 `channel[1..18]`，数组下标等于通道号；遍历从 1 开始，条件为 `i <= SBUS_CHANNEL_COUNT`（18），模拟通道数量为 `SBUS_ANALOG_CHANNEL_COUNT`（16）。
- `frame_lost`、`failsafe`：接收器报告的丢帧和失控标志。
- `valid`：曾收到格式正确的数据；不代表当前可用于控制。
- `online`：数据未超过 100 ms 且未报告丢帧或失控。
- `timestamp_ms`、`sequence`：最新帧接收时间和发布序号。

`sbus_get(NULL)` 或组件未初始化时返回 -1，否则返回快照并返回 0。失联时保留最后数据供诊断，业务必须检查 `valid && online`。只有可用输入才会刷新已启用的 daemon；自动停机仍取决于 daemon 配置和业务逻辑。

`sbus_waitData()` 等待调用后发布的新帧，返回等待毫秒数；包含带失控标志的完整帧。仅可在任务中调用，断线时持续等待；不可用时返回 `UINT32_MAX`。

接收沿用 UART DMA 和空闲中断，要求每个空闲分隔的包为 18 或 25 字节；SBUS 支持标准结尾 0x00 和低四位为 0x04 的结尾。串口错误、非法帧不会发布；不跨空闲边界拼接残包。暂不支持 200000 baud Fast SBUS。

协议格式参考：[Bolder Flight SBUS](https://github.com/bolderflight/sbus/blob/main/README.md)。
