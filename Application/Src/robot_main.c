/* 遥控麦克纳姆轮底盘示例（SBUS：FlySky T8FB 发射机 + R8EF 接收机）
 *
 * 遥控：SBUS 模式。R8EF 的 S.BUS 信号 → H1.7(RX6/PA5)，正极 → H2(+5V)，负极 → H3(GND)。
 *       启动代码已 remote_init() 统一初始化（按包长自动区分 18B DBUS / 25B SBUS），
 *       这里直接读 SBUS 快照，不要自行初始化串口。
 *       ⚠ R8EF 必须处于 SBUS 输出（蓝灯亮 / 红蓝同亮），否则收到的是 PWM 不是 SBUS。
 * 电机：板载 6 路 TT 驱动中的 4 路，默认 CN9/CN5/CN6/CN7 = MOTOR_TT_1..4。
 *       TT 电机无编码器，只能开环给占空比（无速度反馈、无 PID）。
 *
 * T8FB 通道（Mode 2，见战队文档图）：
 *   CH1 = 右摇杆 左右      CH2 = 右摇杆 上下
 *   CH3 = 左摇杆 上下      CH4 = 左摇杆 左右      CH5..CH8 = 拨杆/辅助
 *
 * 映射：CH3 左摇杆上下 → 前进/后退 vx；CH4 左摇杆左右 → 左/右平移 vy；
 *       CH1 右摇杆左右 → 原地旋转 wz；CH5 拨杆向上 → 解锁（否则停车）。
 *
 * 麦轮反解（vx 前、vy 右、wz 逆时针为正）：
 *   FL = vx + vy - wz ; FR = vx - vy + wz ; RL = vx - vy - wz ; RR = vx + vy + wz
 * 前后方向与装轮无关；平移/旋转方向依赖实车装轮。反了改 SIGN_* 或 wheel_mix 符号。
 *
 * 全局 g_remote / g_duty 供 SWD 在线读取（调通道、查遥控是否收到帧）。
 */
#include "os.h"
#include "led.h"
#include "sbus.h"
#include "motor_tt.h"

#include <math.h>
#include <stdbool.h>

/* ---------------- 可调参数 ---------------- */
#define CTRL_PERIOD_MS 10U   /* 控制周期 → 100 Hz */
#define MAX_DUTY       0.50f /* 单轮最大占空比（0~1） */
#define SLEW_PER_CYCLE 0.02f /* 每周期最大变化量 → 0~50% 约 0.25 s 软启动 */

#define CH_FORWARD 3U /* 左摇杆上下 → 前后 */
#define CH_STRAFE  4U /* 左摇杆左右 → 平移 */
#define CH_TURN    1U /* 右摇杆左右 → 旋转 */
#define CH_ARM     5U /* 解锁拨杆：> ARM_ON 解锁（拨向上）；不是 CH5 就改这里 */

#define ARM_ON 0.5f

/* 分量符号：实车验证后按需取反 */
#define SIGN_FORWARD 1.0f
#define SIGN_STRAFE  1.0f
#define SIGN_TURN    1.0f

/* 轮位 → TT 通道，顺序 [左前 FL, 右前 FR, 左后 RL, 右后 RR]，按实车接线调整 */
static const motor_tt_id_t wheel_ch[4] = {
    MOTOR_TT_1, MOTOR_TT_2, MOTOR_TT_3, MOTOR_TT_4
};

/* 麦轮混合矩阵：wheel[i] = mix[i][0]*vx + mix[i][1]*vy + mix[i][2]*wz */
static const float wheel_mix[4][3] = {
    { 1.0f,  1.0f, -1.0f }, /* FL */
    { 1.0f, -1.0f,  1.0f }, /* FR */
    { 1.0f, -1.0f, -1.0f }, /* RL */
    { 1.0f,  1.0f,  1.0f }  /* RR */
};

/* ---- 在线调试用全局（OpenOCD `mdw` / `p` 可读）---- */
sbus_t g_remote;
float g_duty[4];

static float slew_limit(float current, float target, float step)
{
    float delta = target - current;

    if (delta > step) {
        delta = step;
    } else if (delta < -step) {
        delta = -step;
    }
    return current + delta;
}

void robot_main(void *args)
{
    bool armed = false;

    (void)args;

    os_delay(1000U); /* 上电缓冲：留出复位/接线时间，期间电机保持禁用 */

    for (unsigned int i = 0U; i < 4U; ++i) {
        motor_tt_enable(wheel_ch[i]);
        motor_tt_setDuty(wheel_ch[i], 0.0f);
    }

    for (;;) {
        float target[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        float peak = 1.0f;
        float scale;
        bool link_ok;
        bool arm_now;

        link_ok = (sbus_get(&g_remote) == 0) && g_remote.valid && g_remote.online;
        arm_now = link_ok && (g_remote.channel[CH_ARM] > ARM_ON);

        /* 只在新解锁（拨杆由下向上）时进入解锁，避免上电即冲 */
        if (!arm_now) {
            armed = false;
        } else if (!armed) {
            armed = true;
        }

        if (armed) {
            float vx = g_remote.channel[CH_FORWARD] * SIGN_FORWARD;
            float vy = g_remote.channel[CH_STRAFE] * SIGN_STRAFE;
            float wz = g_remote.channel[CH_TURN] * SIGN_TURN;

            for (unsigned int i = 0U; i < 4U; ++i) {
                float w = wheel_mix[i][0] * vx + wheel_mix[i][1] * vy +
                          wheel_mix[i][2] * wz;
                target[i] = w;
                if (fabsf(w) > peak) {
                    peak = fabsf(w);
                }
            }
        }

        /* 归一化后整体缩放到 MAX_DUTY：保持各轮比例 → 运动方向不变 */
        scale = MAX_DUTY / peak;
        for (unsigned int i = 0U; i < 4U; ++i) {
            target[i] *= scale;
            g_duty[i] = slew_limit(g_duty[i], target[i], SLEW_PER_CYCLE);
            motor_tt_setDuty(wheel_ch[i], g_duty[i]);
        }

        led_set(LED_1, armed); /* 解锁且在线时点亮 */

        os_delay(CTRL_PERIOD_MS);
    }
}
