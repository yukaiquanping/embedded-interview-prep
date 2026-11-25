# AC791N 实时语音助手：项目面试追问（16 题）

适用方向：嵌入式 C、联网音频、SDK 二次开发、任务与回调协作。依据截至 2026-11-25 的公开源码与复盘整理。所有时间换算都标明配置含义；没有新增烧板验收或实测性能结论。

## 01. 这是端侧 AI 项目还是云端语音终端？你实现了什么？

**参考回答：** 这是基于杰理 AC791N / WL82 SDK 的联网语音终端。开发板完成音频采集、VAD 分轮、网络上传、回复播放和 LCD 交互；ASR、LLM、TTS 在云端执行。项目在 Hello 示例上新增应用，并适配 OneSDK、WebSocket 事件、音频缓冲和 UI。个人工作应按实际参与说明，厂商音频驱动、底层算法和云端模型不能算成自己从零实现。[项目首页](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/README.md)

**追问：** 为什么选择这个平台？可以结合无线、录放音、屏幕、SDK 可复用能力和开发成本解释；没有完成多平台功耗与性能比较时，不应声称自己通过实测选出了最优平台。

## 02. 从上电到听到 AI 回复，中间经过哪些阶段？

**参考回答：** 初始化后等待 WiFi，完成设备注册及配置获取，再建立带鉴权的 WebSocket 会话；初始化录放音，业务循环读取 PCM 并组织分轮，网络任务完成消息发送。云端返回音频和文字事件后，分别进入播放缓冲与 UI 数据更新。讲流程时，应把“联网”“WebSocket 连接”“业务会话”“音频播放”作为不同阶段检查。[项目流程复盘](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/docs/AC791N项目流程与技术面试复盘.md)、[hello_realtime_voice.c](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/overlay/apps/demo/demo_hello/hello_realtime_voice.c)

**追问：** HTTP 101 成功能证明对话正常吗？它只能表明协议升级成功，后续会话配置、音频格式、发送队列和云端业务接入仍可能出错。

## 03. 3200 字节音频块代表什么？Base64 后多大？

**参考回答：** 格式为 16 kHz、16 bit、单声道，所以每秒 PCM 为 `16000 × 2 × 1 = 32000` 字节。`MIC_PACKET_BYTES=3200` 对应 100 ms。Base64 长度为 `4 × ceil(n/3)`，3200 字节编码后为 4268 个字符；C 字符串若需要结尾 NUL，还要另外预留一字节。上行还包括 JSON 与网络封装。[分包及编码代码](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/overlay/apps/demo/demo_hello/hello_realtime_voice.c)

**误区：** 100 ms 是块的音频时长，不能回答成端到端响应时间。Base64 是可逆编码，不是压缩或加密。

## 04. 为什么拆成语音任务和 WebSocket 任务？这是多核并行吗？

**参考回答：** 语音任务推进联网、会话和分轮逻辑，`hello_rt_ws` 任务持续服务网络事件循环。两者通过队列与状态协作，使音频产生与网络可发送时间解耦。`thread_fork`、信号量、互斥锁等由 SDK 提供；存在多个任务并不自动说明使用了多个 CPU 核。具体优先级语义、任务栈参数单位和调度 tick 要以该 SDK 为准。[任务创建与业务循环](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/overlay/apps/demo/demo_hello/hello_realtime_voice.c)

**追问：** 如何证明任务划分合理？记录采集、入队、回调和播放的时间与队列水位，检查阻塞、栈峰值和调度竞争，而不是只展示“创建了两个线程”。

## 05. 音频已经入队，为什么仍可能发不出去？

**参考回答：** 入队只改变应用内存，不代表网络事件循环已经安排发送。本项目曾出现数据积在 ring 的问题：修复路径是在服务循环中检查待发队列、请求可写通知，并在入队后唤醒服务循环。实际 `lws_write` 放在 `LWS_CALLBACK_CLIENT_WRITEABLE` 中。空的可写回调也可能发生，所以回调仍需检查队列。[业务侧循环](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/overlay/apps/demo/demo_hello/hello_realtime_voice.c)、[发送队列与可写回调](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/overlay/apps/common/LLM/onesdk/src/infer_realtime_ws.c)

**追问：** 现有代码是否就完全线程安全？不能这样推断。源码在业务侧仍调用可写请求；按 libwebsockets 官方线程模型，更稳妥的收敛方向是生产者安全入队后使用允许跨线程的唤醒方式，让服务线程请求可写并拥有连接生命周期。[libwebsockets 官方编码说明](https://libwebsockets.org/lws-api-doc-master/html/md_READMEs_README_coding.html)

## 06. 发送队列满了该怎么办？丢音频块就彻底解决了吗？

**参考回答：** 当前 ring 有限长，插入失败会释放本次分配并向上返回错误，业务侧记录丢弃；这样避免无限堆积，但会损失语音内容。应区分连续 PCM 和 `commit`、取消等关键控制消息，为控制消息保留发送机会，并明确一轮失败后如何复位。源码中的提交失败日志表明这仍需进一步设计，不能说控制消息已经保证送达。[入队逻辑](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/overlay/apps/common/LLM/onesdk/src/infer_realtime_ws.c)、[业务错误处理](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/overlay/apps/demo/demo_hello/hello_realtime_voice.c)

**追问：** 怎么决定队列容量？结合最大消息大小、允许积压时长与可用内存预算，并记录高水位、丢弃率和恢复时间，不能只增加队列项数。

## 07. VAD、音量门限和 AEC 分别解决什么？

**参考回答：** VAD 帮助判断是否处于说话阶段；项目再结合峰值、均值和持续音频量条件，减少底噪或瞬时噪声触发。AEC 针对扬声器回声，不会因为有 VAD 或幅度门限就自动具备。门限需要在距离、噪声和播放音量变化的场景下验收，公开代码中的常量不是通用最优参数。[run_dialog 与 packet_peak](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/overlay/apps/demo/demo_hello/hello_realtime_voice.c)

**追问：** 为什么播放时打断更难？麦克风也可能采到本机喇叭，需要区分真实用户与回声；提高门限会减少误触发，也可能漏掉轻声或远处说话。

## 08. 插话打断为什么要“取消云端回复”和“清空本地播放”两步？

**参考回答：** 云端取消用于停止旧回复继续生成或发送；本地缓冲中可能已经有旧音频，若不清空仍会继续播放。当前达到打断条件后按需取消旧回复、清空待播数据、更新状态并恢复前置音频，再进入新一轮。`BARGE_IN_CONFIRM_BYTES=5120` 在该格式下对应 160 ms 的合格 PCM 量，不是测得的打断响应时延。[打断流程](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/overlay/apps/demo/demo_hello/hello_realtime_voice.c)、[播放缓冲接口](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/overlay/apps/common/LLM/audio/audio_input.c)

**追问：** 取消后迟到的旧音频怎么办？可进一步按轮次或响应 ID 过滤、协调取消确认。当前的清空操作不能证明网络里所有旧消息都被撤回。

## 09. 前置缓存为什么能补句首？内存代价是多少？

**参考回答：** 检测与持续确认需要时间，等确认后才上传会漏掉已经采集的句首。当前等待回复期间保留最近 PCM，确认插话后先按包补发，再处理后续输入。`PRE_ROLL_BYTES=20480` 为 20 KiB，按 32000 字节/秒换算约 0.64 秒。源码用数组、`memmove` 与长度记录维护滑动缓存，并非一个无拷贝环形缓冲。[pre_roll_push](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/overlay/apps/demo/demo_hello/hello_realtime_voice.c)

**追问：** 可以怎样优化？用有界环形缓冲减少搬移，明确写入、补发和清空的所有权；还需测试是否出现重复片段、跨轮残留和队列满时句首丢失。

## 10. 语音结束时为什么要读剩余 PCM，再提交？

**参考回答：** VAD 从说话变为静音，不代表采集缓冲已空。若立即结束一轮，仍在缓冲中的话尾可能被丢弃。收尾逻辑会尝试读取剩余 PCM；部分尾包路径直接携带提交标志，仍无数据的收尾分支会补一块静音并提交，之后进入等待回复状态。提交时间与播放完成时间是两件事，应分别记录与处理。[分轮尾部处理](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/overlay/apps/demo/demo_hello/hello_realtime_voice.c)、[audio_stream_read_available](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/overlay/apps/common/LLM/audio/audio_input.c)

**追问：** 增加静音有什么代价？会增加少量数据与等待；是否必要取决于云端协议和断句行为，需要对比完整性与交互等待，不能只凭听感给出固定收益。

## 11. 云端说 response.done 后，就能马上开始下一轮吗？

**参考回答：** 云端生成结束不等于本地扬声器已经播完。当前逻辑同时检查待播缓冲是否排空，以及回复完成标志；也对已收到音频但较长时间没有新音频的情况提供结束判断。还有等待回复的超时路径，避免永久停在等待。应分别解释云端事件、接收结束与播放结束。[reply_done 与 pending_playback 判断](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/overlay/apps/demo/demo_hello/hello_realtime_voice.c)

**追问：** “音频一段时间没到”会不会误判？弱网可能造成长间隔，所以这是有取舍的恢复策略，需要结合协议事件、轮次和播放状态验收，不能视为严格的云端完成证明。

## 12. 音频回调能不能一直等到播放缓冲腾出空间？

**参考回答：** 当前 `receive_audio` 解码 Base64 后分段写播放缓冲；写不进去时会检查退出条件并短暂等待。优点是没有直接丢掉整段回复，但如果它位于网络事件循环回调中，长时间等待会拖住其他网络事件。这是值得继续改进的点：用有界接收队列把播放写入交给专门任务，制定超时、溢出和取消策略。[receive_audio](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/overlay/apps/demo/demo_hello/hello_realtime_voice.c)

**追问：** 异步交给另一个任务时保存哪个指针？必须明确传入缓冲的有效期，复制数据或转移所有权；不能回调返回后继续使用框架可能复用的内存。

## 13. UI 为什么要把数据更新和重绘分开？

**参考回答：** 网络或业务回调更新文本、状态并置刷新标记，UI 的定时刷新再检查当前页面和防重入状态，生成有限长度的显示内容。这样可以缩短网络回调中的 UI 工作，并避免错误上下文里复杂重绘。当前 `ui_main.c` 有两种条件编译布局：旧模式保存最近消息，`CHAT_LAYOUT` 模式保存问题和回答并分页；实际展示必须以构建宏和 UI 资源为准。[ui_main.c](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/overlay/apps/common/example/Devkitboard_test/ui_main.c)

**追问：** 堆还有空间为什么仍会重启？还应检查任务栈、回调递归或重入、控件长度断言、资源 ID 和页面生命周期，不能只看堆剩余量。

## 14. volatile 能保证任务间同步吗？共享文本和标志如何管理？

**参考回答：** `volatile` 不提供互斥，也不自动让一组共享状态形成一致快照。源码中一些状态标志用了 `volatile`，发送 ring 有互斥锁，问答布局的文本更新也使用锁；这些仍需要逐条核对读写上下文。更清晰的设计是用消息队列传递事件、指定状态拥有者，必要时用平台支持的同步原语保护跨任务数据。[业务状态](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/overlay/apps/demo/demo_hello/hello_realtime_voice.c)、[UI 文本锁](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/overlay/apps/common/example/Devkitboard_test/ui_main.c)

**追问：** 为什么不要持锁做整个绘制？绘制可能耗时或触发回调，扩大锁占用与死锁风险；可以在短锁内复制快照，再在 UI 合法上下文中渲染。

## 15. 断线恢复为什么不能直接释放上下文再重新连接？

**参考回答：** WebSocket 任务可能仍在运行事件循环，音频回调也可能持有上下文。当前退出路径停止音频流、设置停止标志、唤醒服务循环并通过信号量等待 WebSocket 任务退出，然后再反初始化会话；外层复位会话标志后重试。这样体现了先结束使用者、再释放资源的顺序。[voice_run_once 与 websocket_exited](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/overlay/apps/demo/demo_hello/hello_realtime_voice.c)

**追问：** 有重连循环就算稳定了吗？还需分别验证 WiFi 断开、DNS/TLS 失败、鉴权失败、任务创建失败和连续重连资源增长。当前实现不等于已经完成长期运行测试；退避、失败分类和退出等待上限也是后续检查点。

## 16. 遇到 401、500 或“连接成功但没声音”，你如何排查？

**参考回答：** 先按阶段定位：注册失败检查参与签名的字段与鉴权头；握手失败检查网关地址、路径、智能体参数及服务端响应；101 后无数据检查会话配置、MIC PCM、发送队列、可写回调、下行事件与播放路径。日志应使用脱敏信息及必要的长度、状态、错误码，不把凭据作为复盘材料发布。历史项目分别处理过接入参数、失效推理接入点和事件循环唤醒等问题。[项目复盘](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/docs/AC791N项目流程与技术面试复盘.md)、[项目首页](https://github.com/yukaiquanping/ac791n-realtime-voice-assistant/blob/main/README.md)

**追问：** 怎样证明修复？为当前失败阶段建立最小复现，固定配置，只改变待验证因素；保存脱敏日志并重复同一场景。实机演示能证明视频中出现的交互，不能推出固定首包延迟、识别准确率或全部异常恢复场景均已通过。

## 口述练习

先画出“PCM → 分轮 → 编码 → 发送队列 → 网络回调 → 云端 → 接收回调 → 播放缓冲 → DAC”的链路。再练习把“源码已有行为”“历史故障修复”“仍需改进的设计”分开说明，遇到任务上下文和生命周期追问时打开对应函数验证。
