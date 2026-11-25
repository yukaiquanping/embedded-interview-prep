# Linux 驱动项目面试题（20 题）

适用方向：嵌入式 Linux、驱动开发、板级移植。以 [linux-advanced-drivers](https://github.com/yukaiquanping/linux-advanced-drivers) 的 **STM32MP157 / Linux 5.4.31** 学习工程为背景。接口名称和返回值按 Linux 5.4 讨论；换内核或厂商 BSP 时，应查目标源码与绑定文档。

每题先独立口述 1～2 分钟，再核对回答。以下参考回答区分仓库已有内容与建议改进；改进方案不代表已经提交到原项目或通过实机验收。

## L01｜怎样介绍这个项目，个人工作边界是什么？

**参考回答：**这是一个驱动学习与复习工程，围绕设备树、内核驱动、用户态接口，把 Input、I2C、SPI、PWM、显示、摄像头和声卡的数据路径串起来。我可以结合代码解释资源获取、事件上报、用户数据复制和错误处理，并指出原始示例的缺陷。摄像头和声卡部分主要是已有 BSP 驱动的配置、移植与验证方法，不应说成从零编写了完整控制器驱动。

**追问与边界：**哪些功能有实测证据？仓库记录 LCD 控制台字体从 8×16 放大到 16×32 并可恢复；其他参考示例本次整理未重新上板，不能宣称所有驱动均已修复或长期稳定运行。介绍成果时应说明板型、内核、操作、日志与实际结果。

依据：[项目说明与验证范围](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/README.md)、[已知代码问题](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/notes/07-code-review.md)。

## L02｜设备树的 `compatible`、`reg`、`status` 分别有什么作用？

**参考回答：**`compatible` 按绑定约定描述设备型号，用于驱动匹配；`status` 决定节点是否可用；`reg` 的含义由父总线与绑定决定，不能统一解释成寄存器地址。本项目中 SI7006 的 `reg = <0x40>` 是 I2C 7 位从地址，SPI 子节点的 `reg = <0>` 是片选编号，DSI panel 的 `reg` 是虚拟通道号。GPIO、电源、时钟、pinctrl、endpoint 等共同描述板上连接，设备树不会替代驱动实现。

**追问与误区：**改了 DTS 为什么无效？确认编译的是启动流程实际加载的 DTB；确认修改节点、父控制器 `status`、驱动配置和接线一致。`fixed-clock` 描述已有时钟，不会凭空产生一根有波形的时钟线。

依据：[I2C](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/notes/02-i2c.md)、[SPI](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/notes/03-spi.md)、[显示](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/notes/05-display.md)。

## L03｜加了设备树节点却没有进入 `probe()`，如何排查？

**参考回答：**先看设备是否创建，再看驱动是否注册和匹配，最后看依赖是否就绪。确认运行内核和 DTB 版本、父控制器是否启用、模块是否安装加载、`compatible` 是否与匹配表一致。platform 设备、I2C client、SPI device 的枚举路径不同，不能看到一个节点就认定总线已创建设备。若已进入 `probe()` 又失败，则保留原始错误码，并检查 GPIO、IRQ、clock、regulator、DMA 等资源。

**追问与误区：**`-EPROBE_DEFER` 是什么？通常表示依赖的资源提供者尚未就绪，驱动核心后续可以重试；不应把它一律改为 `-ENODEV`，也不应在 probe 中无限等待依赖。日志要记录失败阶段，避免只打印“初始化失败”。

依据：[摄像头分层排错](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/docs/camera-porting.md)、[Linux 5.4 驱动模型](https://www.kernel.org/doc/html/v5.4/driver-api/infrastructure.html)。

## L04｜按键从 GPIO 中断到应用 `read()`，经过哪些对象？

**参考回答：**路径是 GPIO 中断 → 按键驱动 → Input core → evdev handler → `/dev/input/eventX` → 应用。驱动分配 `input_dev`，声明 `EV_KEY` 和支持的按键码，注册设备；中断或后续处理读取状态，调用 `input_report_key()`，最后用 `input_sync()` 标记这组事件完成。`input_dev` 描述输入设备，handler 处理设备事件，handle 是两者之间的连接。platform device 与另外注册的 input device 是不同对象。

**追问与误区：**两个进程各自打开同一个 event 节点，是否轮流拿事件？通常各自有读取缓冲，都会收到事件；独占抓取另论。不要把 evdev 理解为多个进程共享消费一个普通队列。事件编号范围也不是由 CPU 是 32 位还是 64 位决定。

依据：[Input 数据路径与接口](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/notes/01-input.md)。

## L05｜多按键示例为何存在初始化竞争？怎样调整顺序？

**参考回答：**原示例在循环中先 `devm_request_irq()`，后面才分配并注册 `input_dev`。中断申请成功后可能立即触发，处理函数却访问尚未初始化的 `key_dev->input_dev`。应先完成私有数据、按键条目、GPIO 与 input 设备的初始化和注册，再让可能上报事件的中断生效；失败回滚也要按依赖关系安排。

**追问与误区：**卸载时反过来呢？必须先停止并同步会访问 input 对象的中断，以及相关延迟工作、定时器，然后再注销输入设备。只把申请接口改成 `devm_*`，不能自动修复手工注销时的访问竞争。

依据：[原始多按键驱动](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/reference/input/input_keys/input_keys.c)、[代码审查记录](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/notes/07-code-review.md)。

## L06｜`GPIO_ACTIVE_LOW`、消抖和可睡眠 GPIO 应如何处理？

**参考回答：**描述符 API 的 `gpiod_get_value()` 返回按有效极性转换后的逻辑值；active-low 时物理低电平是逻辑 1，不能再习惯性取反。若 GPIO 控制器的访问可能睡眠，应在合适的线程上下文使用 `_cansleep()` 接口，不能在普通硬中断里做可能睡眠的总线操作。机械按键还要考虑抖动：可按硬件能力设置消抖，或使用定时器／延迟工作在稳定后确认状态。

**追问与误区：**能在中断里 `msleep(20)` 消抖吗？不能在普通硬中断处理函数中睡眠。把采样推迟到合适上下文后，还需处理取消工作与卸载并发。当前原始按键示例不能因有中断就视为已实现可靠消抖。

依据：[Input 笔记](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/notes/01-input.md)、[Linux 5.4 GPIO consumer API](https://www.kernel.org/doc/html/v5.4/driver-api/gpio/consumer.html)。

## L07｜全局设备指针为什么影响多实例？互斥锁和自旋锁怎么选？

**参考回答：**第二个设备 probe 时覆盖全局指针，前一个设备的中断、ioctl 或 remove 可能操作错误对象。应为每个设备分配私有数据，通过 drvdata、clientdata、`file->private_data` 或 IRQ 的 `dev_id` 找回对应实例。对于共享配置和状态，按访问上下文选择锁：会睡眠的同步 I2C／SPI 等操作通常放在可睡眠上下文，用 mutex 串行化；需要与硬中断共享的短临界区可考虑 spinlock，并按是否与本 CPU 中断竞争选择中断保存形式。

**追问与误区：**“给全局指针加锁”够吗？不够。锁能保护访问，却不会自动建立设备归属、引用计数或解绑后的存活期；也不能持有自旋锁再做可能睡眠的 I/O。当前示例的多实例与解绑生命周期属于待完善项。

依据：[Input 原码](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/reference/input/input_keys/input_keys.c)、[SPI 原码](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/reference/spi/m74hc595_drv/m74hc595_drv.c)。

## L08｜用了 `devm_*`，`remove()` 就可以完全不做清理吗？

**参考回答：**`devm_*` 把资源释放挂到设备生命周期，能简化 probe 失败与解绑时的释放路径，但申请失败仍要检查。它主要解决资源归还，不等于业务停机：仍需让设备停止产生事件，停止 DMA、取消工作、同步中断，并处理对上层暴露的接口。混用手工释放与托管资源时，要明确所有权，避免重复释放或释放顺序错误。

**追问与误区：**`kzalloc()` 的对象能直接不管吗？不能；普通分配要在相应错误路径与退出路径释放。资源的“停止使用”和“归还资源”是两个步骤，回收顺序应遵循对象依赖关系。

依据：[已知问题](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/notes/07-code-review.md)、[Linux 5.4 devres](https://www.kernel.org/doc/html/v5.4/driver-api/driver-model/devres.html)。

## L09｜读取 SI7006，为什么不能只检查 `ret < 0`？

**参考回答：**不同 I2C 接口成功返回值含义不同：`i2c_transfer()` 返回完成的消息个数，应等于请求数；`i2c_master_recv()` 返回字节数；本项目的 `i2c_smbus_read_i2c_block_data(..., 2, buf)` 应返回 2。非负但不足长度不能当作完整数据。先保留并返回负错误码，再检查短传输；拼接 SI7006 的高字节与低字节时核对端序，并屏蔽低两位状态位。

**追问与误区：**两个独立 send/recv 等同于一个组合传输吗？不能保证；需要 repeated START 的协议应使用适配的组合消息。项目两字节读取片段没有读取或验证 CRC，hold-master 模式还要求适配器支持时钟延展，不能宣称已完成完整传感器容错。

依据：[SI7006 读取片段](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/examples/i2c/si7006_read.c)、[I2C 笔记](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/notes/02-i2c.md)。

## L10｜ioctl 如何安全地把数据交给用户态？

**参考回答：**先校验命令和参数，再执行设备操作并检查结果，最后通过 `copy_to_user()` 返回数据。不能把用户地址直接当普通内核指针解引用。`copy_to_user()`／`copy_from_user()` 返回未复制的字节数，非零通常转为 `-EFAULT`。设备读取可能返回负错误码时，先用有符号类型检查，不能先存入无符号变量再把它当测量值复制出去。

**追问与误区：**`_IOW(..., int)` 自动复制一个 int 吗？不会，它编码方向和大小，真正复制仍由驱动实现。ABI 应统一命令、固定宽度载荷、长度和参数传递方式；SPI 原例编码为 int 却只复制两字节，需要应用与驱动一起修改。未知命令宜返回 `-ENOTTY`。

依据：[I2C 用户数据处理](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/notes/02-i2c.md)、[SPI 两字节协议](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/notes/03-spi.md)。

## L11｜`spi_write()` 后还需要 `spi_sync()` 吗？缓冲区有什么要求？

**参考回答：**不需要，`spi_write()` 已是同步封装；若需要多段传输，则构造 `spi_transfer`，加入 `spi_message` 后提交 `spi_sync()`。两者成功通常返回 0，负值应传回调用方。同步调用返回前缓冲区要一直有效；异步调用则必须保留到完成回调。传输缓冲还要满足 SPI 控制器的 DMA 约束，不能认为任何栈地址都适合通用 SPI DMA。

**追问与误区：**74HC595 收到 SPI 字节就一定更新输出吗？还要核对移位时钟与锁存时钟的连接。CPOL 决定空闲电平，CPHA 决定在哪个边沿采样；只有结合两者才能说出具体上升沿或下降沿。不能脱离数据手册和接线照抄模式。

依据：[SPI 笔记](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/notes/03-spi.md)、[74HC595 驱动](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/reference/spi/m74hc595_drv/m74hc595_drv.c)。

## L12｜SPI 示例有哪些可以现场指出的缺陷？如何考虑解绑？

**参考回答：**ioctl 忽略同步发送返回值，会把发送失败报告成成功；读取 `m74-name` 不检查函数返回值，还使用未初始化的局部指针；部分 probe 失败路径泄漏内存；全局指针不能支持多实例；协议大小与实际复制长度不一致。可以逐项明确修复，但这些仍是原仓库记录的问题，不能表述成已经修好。

**追问与边界：**`misc_deregister()` 后立即释放对象安全吗？已有打开文件仍可能保留引用，需要禁止新请求、串行化正在执行的操作、标记离线并设计引用计数／关闭后释放路径。`.owner = THIS_MODULE` 保护模块代码并不等于自动保护设备私有对象；devm 也不能替代打开文件的生命周期管理。

依据：[SPI 原码](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/reference/spi/m74hc595_drv/m74hc595_drv.c)、[已知问题说明](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/notes/07-code-review.md)。

## L13｜PWM 的周期、占空时间和频率如何对应？原例错在哪里？

**参考回答：**周期为 T，频率为 1/T，占空比为 duty/T。Linux 5.4 的 state 周期和占空时间用 ns：周期 1,000,000 ns 是 1 kHz，占空时间 250,000 ns 是 25%。`pwm_config(pwm, duty_ns, period_ns)` 第二参是占空时间，第三参是周期。原蜂鸣器例子的 `SET_DUTY` 改了第三参、`SET_PERIOD` 改了第二参，命名与行为反了；还忽略返回值，并在 disable 后未按意图恢复输出。

**追问与误区：**怎样修改多个参数？读取或初始化 state，校验 `duty_cycle <= period` 和单位，再统一设置 period、duty、enabled，检查 `pwm_apply_state()` 返回值。`pwm_get_state()` 取得框架状态，不能当作示波器测得的波形；实际输出精度受硬件影响。

依据：[PWM 原始蜂鸣器驱动](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/reference/pwm/beeper_drv/beeper_drv.c)、[PWM 笔记](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/notes/04-pwm.md)、[Linux 5.4 PWM API](https://www.kernel.org/doc/html/v5.4/driver-api/pwm.html)。

## L14｜DRM 显示为什么不能一律按 `width * 4` 计算行偏移？

**参考回答：**显示缓冲区可能有行对齐填充，真实行跨度是驱动返回的 `pitch`。像素地址要按行跨度和像素格式计算，并检查图片边界；固定按宽乘四只能在特定格式且紧密排列时成立。项目显示链为内存像素 → LTDC → DSI → ST7701 panel，背光另走 PWM，触摸另走 I2C 与 Input。

**追问与误区：**把 JPG 写进 `/dev/tty1` 能显示图片吗？不能。应用需解码图片、选择 connector/mode/CRTC、创建并映射缓冲区、写入像素后设置显示。还要处理超大图片、灰度 JPEG、错误回滚与退出恢复；原始 DRM 示例的这些边界处理并不完整。

依据：[显示数据路径与 pitch](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/notes/05-display.md)、[显示示例问题](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/notes/07-code-review.md)。

## L15｜GT911 触摸屏为什么同时涉及 I2C 和 Input？

**参考回答：**I2C 是驱动与触摸控制器交换状态和坐标的总线，Input 是向应用提供统一事件的子系统。触摸发生后控制器触发中断，驱动读取触点数据，再通过 slot、触点状态、绝对坐标和 sync 上报。设备树描述 I2C 地址、中断、复位与匹配信息，内核需启用 Goodix、I2C、Input 和 evdev 等支持。

**追问与误区：**tslib 负责创建触摸设备吗？它是用户态库，从 input 节点读取和处理触摸事件。`event0` 编号会变化，应通过 `/proc/bus/input/devices` 和 sysfs 的设备名确认实际节点；显示正常也不代表触摸链路正常。

依据：[GT911 笔记](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/notes/06-touchscreen.md)、[GT911 设备树片段](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/examples/devicetree/gt911.dtsi)。

## L16｜OV5640 并口摄像头和 USB UVC 摄像头如何区分排错？

**参考回答：**板载 OV5640 通过 I2C 配置传感器，图像从并口进入 DCMI，再通过 DMA 和 V4L2 提供采集；需要核对电源、xclk、复位、PWDN、并口引脚和双向 endpoint。USB UVC 则先有 Host/PHY、VBUS 和 USB 枚举，再由 UVC 驱动绑定并提供 V4L2 节点，不需要复制 OV5640 的设备树。

**追问与误区：**出现 `/dev/video0` 就算调通吗？只是一个检查点。还要确认节点能力、枚举支持格式和尺寸、实际持续采集并查看图像。USB 摄像头可产生多个节点，编号和用途均需确认；MJPG 数据也不能直接送进只处理 YUYV 的转换函数。

依据：[摄像头移植与分层排错](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/docs/camera-porting.md)。

## L17｜V4L2 MMAP 采集的顺序是什么？QBUF、DQBUF 如何区别？

**参考回答：**通常先 open、查询能力与枚举格式，设置并检查实际格式，再 REQBUFS；按返回的缓冲数量逐个 QUERYBUF 和 mmap，全部 QBUF 后 STREAMON。循环等待就绪、DQBUF 取得已完成帧、按有效长度处理、再 QBUF 交还缓冲区；结束 STREAMOFF、munmap、释放相关对象并 close。QUERYBUF 的 offset 用于 mmap，不是让应用直接解引用的物理地址。

**追问与误区：**原采集程序有哪些问题？逐次 QUERYBUF 未更新 index，可能重复映射同一缓冲；mmap 应判断 `MAP_FAILED`；初始化失败应退出；缓冲数量、尺寸、行跨度、有效数据长度要使用驱动实际返回值，并处理每一步失败。QBUF 是交回可用缓冲区，DQBUF 是取回已完成缓冲区。

依据：[采集原码问题](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/reference/camera/README.md)、[Linux 5.4 V4L2 MMAP](https://www.kernel.org/doc/html/v5.4/media/uapi/v4l/mmap.html)。

## L18｜ASoC 中 CPU DAI、Codec 和板级声卡分别做什么？

**参考回答：**本项目中 STM32 SAI 负责 SoC 侧数字音频接口及 DMA 数据路径，CS42L51 提供 ADC/DAC 等 Codec 能力，板级 `audio-graph-card` 把 CPU DAI、Codec DAI 和路由连接为声卡。I2C 用于配置 Codec，PCM 数据走 SAI/I2S，不能说音频数据经 I2C 播放。先确认声卡与 PCM 枚举，再用 aplay/arecord 测试，最后排查自写应用。

**追问与计算：**48 kHz、每帧 2 个 32 bit 时隙，BCLK 为 48,000×2×32 = 3.072 MHz；`mclk-fs=256` 时 MCLK 为 12.288 MHz。应用是 S16_LE 不代表总线时隙一定为 16 bit。MCLK 来源与 BCLK/LRCLK 主从角色要分别核对。

依据：[SAI2 与 CS42L51 移植说明](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/docs/audio-porting.md)。

## L19｜ALSA 的 frame 是什么？为什么 `snd_pcm_writei()` 要处理短写？

**参考回答：**frame 包含同一采样时刻所有声道的样本。交错的 16 位双声道 PCM 每 frame 为 4 字节，16 位单声道是 2 字节；不能一律用字节数除以四。`snd_pcm_writei()` 的数量与成功返回值以 frame 为单位，短写时应移动指针并继续发送剩余帧，负值则按错误类型恢复或退出。播放 underrun 表示应用供数不及时等原因使播放缓冲耗尽，不能当作普通成功。

**追问与误区：**WAV 都是固定 44 字节头吗？合法 RIFF/WAV 可以包含不同 chunk，应解析格式和 data chunk；还要处理 fread 的有效字节、尾块、EOF 和错误。原例固定帧大小、固定头部并缺少短写处理，应先用标准工具确认声卡，再修正应用。

依据：[ALSA 参考程序约束](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/reference/audio/README.md)、[ALSA PCM 官方文档](https://www.alsa-project.org/alsa-doc/alsa-lib/pcm.html)。

## L20｜交叉编译成功、板上却不能运行，怎样形成完整排查证据？

**参考回答：**先用 `file` 确认目标架构，用目标工具链的 `readelf -d` 查看真实动态依赖，检查加载器、SONAME、符号链接、库搜索路径与 ABI。不要把主机 x86 库复制到 ARM，也不要仅把不同主版本库改名伪装成所需版本。ALSA 还需要完整配置目录和正确运行环境；登录后能运行而开机脚本失败时，应查脚本环境和声卡就绪顺序。

**追问与验收：**如何说明“移植成功”？记录板型、BSP／内核、启动 DTB、工具链、配置、设备节点、实际格式和操作；摄像头提供连续帧与样图，声卡分别验证录音、播放和通道。保留失败日志与修复前后差异。一次生成文件不能证明图像有效，一段立体声文件不能证明两路输入独立，更不能推导长期稳定性或性能提升百分比。

依据：[摄像头构建与部署](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/docs/camera-porting.md)、[声卡运行库与验收](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/docs/audio-porting.md)、[公共环境与实测记录](https://github.com/yukaiquanping/linux-advanced-drivers/blob/main/docs/porting-environment.md)。

## 自查评分

每题按 0～3 分自评，共 60 分：0 分为无法解释；1 分为能说接口名；2 分为能讲清数据路径与错误处理；3 分为能联系本项目代码、指出边界并给出验证办法。错题优先记录“哪个对象、谁拥有资源、何时可能并发、成功返回值是什么”，再回到对应源文件核对。
