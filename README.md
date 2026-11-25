# 嵌入式 / Linux 面试与笔试题库

结合个人项目整理的中文复习资料，适用于嵌入式软件、Linux 应用与初级驱动开发岗位。共 **88 道面试问答、两套共 28 题的模拟笔试、6 道 C 编程练习**，均附参考答案或实现。

## 学习入口

| 模块 | 内容 | 建议用法 |
| --- | --- | --- |
| [C 与嵌入式基础](interview/c-and-embedded.md) | 指针、内存、位操作、中断、RTOS、总线 | 先独立回答，再核对边界条件 |
| [Linux 与网络](interview/linux-and-network.md) | 进程线程、同步、IO、TCP/UDP、排障 | 用自己的项目举例 |
| [Linux 驱动项目](interview/linux-drivers.md) | STM32MP157 / Linux 5.4：设备树、Input、I2C、SPI、PWM、V4L2、ALSA | 对照原项目代码复盘 |
| [RK3566 可视对讲](interview/rk3566-intercom.md) | 呼叫状态机、GStreamer、媒体传输、音视频故障 | 练习从架构讲到一次具体故障 |
| [AC791N 语音助手](interview/ac791n-voice.md) | PCM、VAD、插话、异步任务、WebSocket、UI、重连 | 解释数据从 MIC 到喇叭的完整路径 |
| [笔试 A：基础卷](written/paper-a.md) / [答案](written/answers-a.md) | 60 分钟 / 100 分：C、操作系统、总线、计算与代码审查 | 限时作答，先不要打开答案 |
| [笔试 B：项目与排障卷](written/paper-b.md) / [答案](written/answers-b.md) | 90 分钟 / 100 分：协议、驱动、音视频、状态机 | 用证据和实验步骤回答 |
| [C 编程题](written/coding.md) / [参考代码](solutions/c/) | 6 道独立编程练习及边界测试 | 自己实现后运行测试 |
| [项目表达模板](practice/project-story.md) | 自我介绍、贡献说明、故障复盘、反问 | 用真实经历填空，不背虚构履历 |
| [14 天复习安排](practice/study-plan.md) | 每天约 60–90 分钟 | 记录不会的题并复测 |

## 对应的个人项目

- [linux-advanced-drivers](https://github.com/yukaiquanping/linux-advanced-drivers)：STM32MP157、Linux 5.4.31 的驱动学习与复习。原仓库 `reference/` 保留存在已知问题的学习示例。
- [rk3566-linux-intercom](https://github.com/yukaiquanping/rk3566-linux-intercom)：Python / PyQt5 / GStreamer / MySQL 的局域网可视对讲原型。
- [ac791n-realtime-voice-assistant](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant)：基于杰理 SDK / OneSDK 的云端实时语音终端。

项目题以整理时仓库内容为依据；后续源码变化时，应回到原文件核对。Linux 驱动题默认使用 5.4 基线，移植到其他内核需要核对接口和绑定。

## 如何回答一道面试题

用“结论 → 原理 → 项目里的例子 → 适用边界”组织答案。例如：先说为什么需要缓冲，再描述读写双方和满空判定，最后解释过载时如何处理、为什么 `volatile` 不能替代同步。

参考答案是答题要点，不是唯一措辞。设计题允许其他方案，只要约束、失败路径和验证方式自洽。不会的地方可以说明如何查文档和设计实验。

## 练习代码

```sh
cmake -S solutions/c -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

详细构建方式以 [C 练习说明](written/coding.md) 为准；这些练习是独立教学实现，不是开发板驱动或固件替代品。

## 资料说明

本题库为原创整理的学习问答与模拟题，不标称任何公司的真题，不收录未公开的招聘试卷、课程原文或账号配置。项目回答区分已有实现、历史联调记录和后续改进；不把配置计算当成延迟或稳定性实测。

官方技术依据见 [参考资料](REFERENCES.md)。整理日期：2026-11-25。
