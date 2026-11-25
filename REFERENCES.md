# 项目依据与官方参考

[返回目录](README.md)

项目专属问答逐题链接到原仓库文件；本页集中列出技术文档。问答和模拟题为原创归纳，没有搬运课程原文或企业试卷。查阅与整理日期：2026-11-25。

## 个人项目

- [Linux 高级驱动复习](https://github.com/yukaiquanping/linux-advanced-drivers)：使用 README、notes、参考驱动和移植说明；默认内核为 Linux 5.4.31。
- [RK3566 双端可视对讲](https://github.com/yukaiquanping/rk3566-linux-intercom)：使用 README、数据流程文档、`intercom.py`、`media.py` 等文件。
- [AC791N 实时智能语音助手](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant)：使用 README、项目复盘及 `overlay/` 内音频应用、网络和 UI 代码。

链接使用原仓库默认分支，源码可能继续更新。遇到答案与新代码不一致时，以具体版本源码、厂商文档和实测记录重新确认。

## Linux 5.4

- [Driver Basics](https://www.kernel.org/doc/html/v5.4/driver-api/basics.html)：执行上下文、锁和驱动基础接口。
- [Managed Device Resource](https://www.kernel.org/doc/html/v5.4/driver-api/driver-model/devres.html)：资源管理与 `devm_*` 的范围。
- [GPIO Consumer Interface](https://www.kernel.org/doc/html/v5.4/driver-api/gpio/consumer.html)：GPIO 描述符和可睡眠访问接口。
- [PWM](https://www.kernel.org/doc/html/v5.4/driver-api/pwm.html)：PWM 状态与配置。
- [V4L2 userspace API](https://www.kernel.org/doc/html/v5.4/media/uapi/v4l/v4l2.html)：格式、缓冲与流式采集。
- [ALSA PCM API](https://www.alsa-project.org/alsa-doc/alsa-lib/pcm.html)：用户态 PCM 与 xrun 处理。
- [Why volatile should not be used](https://www.kernel.org/doc/html/v5.4/process/volatile-considered-harmful.html)：为什么不能用 volatile 代替同步。

## 网络与多媒体

- [RFC 9293：TCP](https://www.rfc-editor.org/rfc/rfc9293.html)：可靠有序字节流，应用消息边界需自行定义。
- [RFC 6455：WebSocket](https://www.rfc-editor.org/rfc/rfc6455.html)：升级握手与消息传输；业务会话另由服务协议定义。
- [RFC 3550：RTP/RTCP](https://www.rfc-editor.org/rfc/rfc3550.html)：序号、时间戳和同步相关机制。
- [GStreamer queue](https://gstreamer.freedesktop.org/documentation/coreelements/queue.html)：队列、线程边界和容量限制。
- [GStreamer rtpjitterbuffer](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpjitterbuffer.html)：抖动与乱序缓冲。
- [libwebsockets coding guide](https://libwebsockets.org/lws-api-doc-master/html/md_READMEs_README_coding.html)：可写回调、发送与线程约束。
- [Qt 5：Threads and QObjects](https://doc.qt.io/archives/qt-5.15/threads-qobject.html)：对象线程亲和性、事件循环与 GUI 主线程。

官方网页可能展示其他版本；真正移植时，应以项目固定内核、SDK、GStreamer、Qt 和网络库版本的头文件及文档为准。
