# RK3566 可视对讲：项目面试追问（16 题）

适用方向：嵌入式 Linux 应用、音视频应用、系统联调。依据截至 2026-11-25 的公开项目整理；回答应结合自己真正负责的部分练习。以下是项目实现的复盘与设计分析，没有新增上板测试，也不提供未经测量的性能数字。

## 01. 用一分钟介绍项目，你的工作落在哪里？

**参考回答：** 系统由 RK3566 楼门端、Ubuntu 住户端、Python 目录服务和 MySQL 组成。访客输入门牌，查询住户地址并呼叫；住户接听后接收访客视频、双向语音，也可以主动开启自己的摄像头。PyQt5 组织业务与界面，GStreamer 完成媒体采集、编解码与 RTP/UDP 收发。可以重点说明自己参与的分层整合、设备配置、摄像头开关和实机排障，底层编解码与驱动主要复用已有组件。[项目首页](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/README.md)

**追问：** 为什么不说“从零写了一套音视频协议栈”？因为控制协议是应用自定义，媒体处理使用 GStreamer；项目也基于学习示例开展，个人贡献需要按实际参与交代。

## 02. 为什么分为目录、信令、媒体三条通路？

**参考回答：** 目录解决门牌到地址的映射，信令解决谁与谁通话及状态变化，媒体负责持续音视频数据。目录服务不转发媒体，已经建立的控制与媒体可以直接在两端之间运行。这样便于分别排查“查不到人”“呼叫没接通”“接通后没画面”等问题。目录服务故障会影响新呼叫；当前没有目录缓存回退。[代码与数据流程](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/docs/RK3566可视对讲_代码与数据流程面试讲解.md)

**误区：** 数据库里存在一条住户 IP 记录，只表示最后登记的地址，不能证明住户现在在线。

## 03. 目录为什么用 TCP？一次 recv 能否读出完整请求？

**参考回答：** TCP 便于承载少量注册和查询请求，但它传递的是字节流，需要应用分帧。本项目以换行结束一条请求，服务端累积读取并限制总时间与请求长度；客户端也累积响应。当前协议每个连接处理一个请求。门牌格式校验、超时和长度上限分别解决业务输入、慢连接和过长报文问题。[server.py](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/server.py)、[DirectoryClient](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/intercom.py)

**追问：** 如果改为长连接怎么办？应明确多条消息的分帧、剩余缓冲处理、请求关联和断线恢复，不能照搬“连接断开就结束一条消息”。

## 04. 客户端为什么不直接连接 MySQL？门牌格式校验算认证吗？

**参考回答：** 目录服务封装登记和查询，客户端无需保存数据库密码或了解表结构。服务端用参数化 SQL，避免把门牌输入拼进 SQL。门牌校验只约束输入格式，并不能证明登记者身份；当前目录协议缺少产品级认证、授权和传输保护。若产品化，应补充可信设备身份、门牌绑定授权及接口访问控制。登记唯一性还应由表约束和事务设计共同保证。[server.py](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/server.py)

**误区：** 参数化 SQL 防注入与接口鉴权解决不同问题；使用了 `SELECT ... FOR UPDATE` 也不代表任何表结构和隔离级别下都不会出现并发首次登记问题。

## 05. 怎样讲清 UDP 呼叫状态机？为什么有 READY 和 111？

**参考回答：** 楼门从 `idle` 进入 `calling`，住户收到 `request` 后进入 `ringing`。住户接听发送 `READY`，进入 `connecting`；楼门收到后发送 `111` 并进入 `active`，住户收到 `111` 后也进入 `active`。接通信号触发业务层启动媒体。拒绝、超时、挂断和媒体异常都需要回到空闲并清理资源。[CallChannel](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/intercom.py)

**追问：** 这是不是 TCP 三次握手？它只是应用层呼叫确认；确认完成不能证明摄像头、声卡或 GStreamer 管线已经成功启动。

## 06. UDP 包丢失、重复或迟到怎么办？

**参考回答：** 当前控制层用定时重发、阶段超时与重复包处理推进状态；重复 `READY` 可促使楼门再发确认，接通处理避免重复创建媒体。摄像头状态用版本号确认，减少旧状态覆盖新状态。楼门每次呼叫重绑临时控制端口，也降低旧包干扰。仍缺少完整随机会话 ID、鉴权和统一消息序列体系，属于有边界的缓解措施。[CallChannel](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/intercom.py)

**追问：** 如何改进？在消息中加入会话 ID、类型、序列号，按操作设计幂等处理与失效规则；认证后才能把来源当作可信身份。不能仅凭源 IP 和端口保证安全。

## 07. 心跳正常是否说明视频和声音都正常？

**参考回答：** 心跳只能帮助判断控制通路是否仍有响应。应用可以继续回复 PING，但摄像头卡住、解码器失败或声卡路由错误仍可能导致媒体异常。当前代码有心跳超时处理，也轮询媒体总线错误；进一步验收应分别观察采集帧、RTP 到包、解码输出和音频缓冲状态。[intercom.py](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/intercom.py)、[media.py](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/media.py)

**误区：** 控制重传没有给 RTP 媒体增加重传；不能把“有心跳”回答成“弱网下通话可靠”。

## 08. 媒体端口如何配对？为什么楼门不固定控制源端口？

**参考回答：** 默认访客视频发往住户 5000，住户视频发往楼门 5001；住户语音发往楼门 5002，访客语音发往住户 5003。配置中的 `send` 表示对端目的端口，应与对端 `recv` 配对。初次控制发往住户 UDP 8000，住户按收到的楼门 `(IP, 源端口)` 回复；这个临时控制端口与固定媒体接收端口互不相同。[端口定义](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/intercom.py)、[媒体构造](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/media.py)

**追问：** 换一组端口就能穿 NAT 吗？不能。当前依赖直接互通的局域网，没有 NAT 穿透、媒体中继或握手协商端口能力。

## 09. 讲一遍摄像头到远端窗口的数据路径。

**参考回答：** 当前 MIPI 兼容配置由 `v4l2src` 取得 UYVY 原始帧，经 `tee` 分为本地预览与网络发送。发送分支将像素格式转换为 I420，交给 `x264enc`，随后解析 H.264、封装 RTP 并用 UDP 发送；接收端依次经过抖动缓冲、RTP 解包、H.264 解析与解码，再转换并显示。Python 管理管线及生命周期，实际处理由插件承担。[media.py](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/media.py)、[ip_mipi.json](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/ip_mipi.json)

**误区：** 历史 USB/MPP 配置与当前 MIPI 软件兼容配置不同；不要把当前路径说成全程硬件编码，也不能说改了像素格式就实现了零拷贝。

## 10. tee 后面为什么加 queue？leaky 有什么代价？

**参考回答：** 预览和编码消费速度可能不同，分支中的队列用于解耦。当前代码将队列限制为少量帧并采用 `leaky=downstream`，拥塞时丢弃旧缓冲，减少展示越来越旧的画面。代价是可能丢帧；队列限制只是控制某一环节的积压，仍需测量整条链路。[项目管线](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/media.py)、[GStreamer queue 文档](https://gstreamer.freedesktop.org/documentation/coreelements/queue.html)

**追问：** 为什么不无限增大队列？实时对讲中旧画面累积会增加交互等待，内存也不会因此无限可用；缓冲策略要服从实时性和连续性的取舍。

## 11. rtpjitterbuffer 的 latency=100 是否表示端到端延迟 100 ms？

**参考回答：** 不是。它为 RTP 乱序、重复和到达抖动提供处理窗口，是链路的一部分；采集、编码、网络、解码、渲染等都可能再引入等待。当前设置 `drop-on-latency=true`，缓冲超限时避免持续积压，但丢包和迟到包的影响仍存在。音视频分开使用 RTP 接收管线，不等于实现了基于 RTCP 的音画同步。[media.py](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/media.py)、[GStreamer rtpjitterbuffer 文档](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpjitterbuffer.html)

**追问：** 如何报告性能？明确测试环境与端到端定义，分别记录多次测量及分布。仓库目前没有可直接引用的延迟或帧率基准。

## 12. 音频为什么使用 S16BE？设备 48 kHz 和网络 16 kHz 是否冲突？

**参考回答：** 本项目 RTP L16 的发送格式为 16 位、大端、单声道，按配置使用 16 kHz。`audioconvert` 处理样本格式与通道转换，`audioresample` 处理采样率转换。硬件录放可以采用它稳定支持的 48 kHz，再转换成网络侧 16 kHz；因此两处采样率可以不同，但必须有正确转换路径与设备配置。[音频发送管线](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/media.py)、[设备配置说明](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/docs/设备更换与配置使用指南.md)

**追问：** 16 kHz、16 位、单声道的原始码率是多少？`16000 × 16 × 1 = 256000 bit/s`，即 32 kB/s，尚未计入 RTP、UDP、IP 等开销。

## 13. 单向无声时，如何用证据缩小范围？

**参考回答：** 先明确“哪台机器的 MIC 到哪台机器的扬声器”。在发送端检查独立录音及采集设备、格式、路由和错误日志；再检查网络发包与目的端口；最后检查接收解包、播放设备、静音和桌面音频流去向。历史联调中既遇到 PulseAudio 选错输出，也遇到底层录放采样率配置导致采集恢复失败，不能见到无声就都归因于 UDP。[项目故障记录](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/README.md)、[media.py](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/media.py)

**误区：** 抓到包只说明有数据到达；还需确认负载格式、内容、解码及播放。历史处理结果也不能代替更换设备后的复测。

## 14. 绿屏问题怎样复盘，如何避免把相关性当根因？

**参考回答：** 先记录实际设备节点、采集 caps、像素格式和完整管线；分段验证采集、转换、编码与解码，必要时对照软件兼容路径。历史故障伴随 RGA 缓冲地址日志，采用 UYVY → I420 → x264enc 配置后恢复。因为多个配置同时变化，只能确认该路径可用，不能宣称已经单独证明某个参数就是唯一根因。[项目首页](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/README.md)、[当前配置](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/ip_mipi.json)

**追问：** 接下来怎么查根因？固定其余条件，逐项改变采集格式、IO 模式及编解码后端，保留日志和同一输入样本，确认可重复性。

## 15. 关闭摄像头为什么还有最后一帧？如何保留语音？

**参考回答：** 停止发送不能保证接收窗口自动清空，视频 sink 可能留下最后一帧。本项目同步摄像头状态后，接收端移除视频接收管线、将其设为 NULL，清理窗口并触发 Qt 重绘；音频发送和接收管线独立保留。重新开启时再按通话状态创建视频接收。摄像头消息带版本号与确认，降低丢包、旧状态覆盖的问题。[MediaManager.set_remote_camera_enabled](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/media.py)、[摄像头修复说明](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/docs/摄像头开关修复说明.md)

**追问：** 为什么不用整次挂断来处理？摄像头开关是通话中的子状态，应单独管理对应媒体资源，并保持双方业务状态一致。

## 16. 怎样保证挂断后还能再次呼叫？产品化还缺什么？

**参考回答：** 结束路径应收敛到统一清理：停止定时器、将各媒体管线置 NULL、移除窗口绑定、清除本次对端和状态，再恢复页面。启动中途失败也必须回收已经创建的管线，重复停止应能安全执行。`MediaManager` 轮询 ERROR/EOS 后触发清理，业务层再结束呼叫。验收要覆盖接听前取消、重复挂断、媒体启动失败和连续多次通话。[media.py](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/media.py)

**追问：** 距离真实门禁还缺什么？当前“开门”是业务消息，没有驱动实体门锁；后续需设备鉴权、授权、防重放、真实执行结果反馈与故障策略，还需弱网、资源、长期运行测试。这些属于后续设计，不能列为已经完成的成果。[验证边界](https://github.com/yukaiquanping/rk3566-linux-intercom/blob/main/README.md)

## 口述练习

每题先用 30～60 秒给出“职责或现象 → 数据路径 → 设计取舍 → 验证边界”，再打开对应代码解释一个函数。对未测量的指标直接说明没有基准数据，并描述准备怎样测量。
