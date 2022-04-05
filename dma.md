```shell
# 文档
Documentation
    -->dmaengine
        -->00-INDEX
        -->client.txt   #client 使用 dma 的 API 指南
        -->dmatest.txt  #编译、配置、使用 dmatest 系统
        -->provider.txt #dma controller API 指南
    -->crypto
        -->async-tx-api.txt
# 代码
drivers/dma
    -->dmaengine.c
    -->dmaengine.h
    -->virt-dma.c
    -->virt-dma.h
    -->dmatest.c
linuc/include/linux
    -->dmaengine.h
```



## 1.1 框架分析
### 1.1.1 数据结构
#### 1.1.1.1 数据结构总览
<div align=center>
<img src="dma_framework_images/structure_simple.svg" width="1400">
</div>   

&emsp;&emsp;上面的框图分为了四个层级：dma 硬件、linux 下的 dma 框架、驱动 dma 硬件的 provider、其他需要使用到 dma 的驱动（称为 consumer 或 client）。
1. dma 硬件
&emsp;&emsp;硬件包括一个 sdma（用于内存间数据传输）和一个 gdma（专门用于图像传输）。其中 sdma 有四个通道。
2. dma 框架
* 通用 dma 硬件抽象
  * `dma_device`：对 dma 硬件进行抽象，此结构体是对大多数 dma 共同具有的一些特征的抽象。
* 通用物理通道的抽象
  * `dma_chan`：对物理通道进行抽象，dma_chan 与实际的物理通道是一一对应的。
  * `dma_async_tx_descriptor`：对通道上一次具体的传输进行控制的描述符。
* 通用虚拟通道的抽象
  * `virt_dma_chan`：基于物理通道 dma_chan，又抽象出了虚拟通道。软件会管理这些虚拟通道的传输请求。物理通道与虚拟通道的关系并不是一一对应，而是一个物理通道可以抽象出多个虚拟通道（？？？我怎么觉得是虚拟通道与物理通道也是一一对应的呢，只不过是可以在一个虚拟通道上申请多个传输描述符而已）。
  * `virt_dma_desc`：既然对物理通道进行了虚拟化，理所当然的也对物理通道传输描述符进行了虚拟化。其实 virt_dma_desc 只是对 dma_async_tx_descriptor 进行了一下简单的包装而已。

3. dma provider
* 厂家 dma 硬件抽象
  * `gsdma_engine`：基于 dma_device 和具体硬件，自己抽象出一个结构体，它除了包括 dma_device，还会包括例如基地址、时钟、中断特有的等信息，是对 dma 硬件用到的所有资源的抽象。
* 厂家通道的抽象
  * `sdma_channel`、`gdma_channel`：基于 virt_dma_chan 和硬件，定义的自己的通道结构体。其实，在大多数驱动中，同一模块不同通道间的操作逻辑都是相同的，所以只需要抽象一个通道结构体就可以，但是我们的硬件中，虽然 sdma 和 gdma 在同一模块中，但是他们两者的硬件配置逻辑基本不一样，所以就抽象出了两个通道。
  * `sdma_desc`、`gdma_desc`：基于 virt_dma_desc 抽象出的用于控制上面两类通道的传输描述符。一个通道上可以同时申请多个传输描述符。

4. dma consumer
&emsp;&emsp;consumer 使用 dma 时，首先会申请一个通道，返回结构体`dma_chan`，接着会在这个物理通道上面申请一个传输描述符，申请成功将此传输描述符挂载到对应通道的队列中，再通过启动传输函数发起传输。

#### 1.1.1.2 数据结构定义
<div align=center>
<img src="dma_framework_images/structure_detailed.svg" width="1400">
</div>   
&emsp;&emsp;实线表示由箭头起始处的结构体派生出箭头末尾处的结构体，虚线表示包含关系。

#### 1.1.1.3 结构体成员含义
1. dma_device_list
&emsp;&emsp;在 dmaengine.c 中，有一个全局变量 dma_device_list，它是整个系统支持的所有 dma_device（系统可能不止一个 dma_device） 的链表，每个 dma_device 的 global_node 节点都会挂载到此链表上。

2. dma_device
* chancnt：设备支持的通道个数
* privatecnt：使用 dma_request_channel 申请使用了的通道个数
* channels：一个 dma 设备中所有 dma_chan 的链表
* global_node：这个节点会挂载到 dma_device_list。

### 1.1.2 cookie 分析






## 1.6 内核文档翻译
### 1.6.1 client.txt
&emsp;&emsp;注意：有关 async_tx 中 DMA 引擎的使用，请参阅：Documentation/crypto/async-tx-api.txt 
&emsp;&emsp;下面是设备驱动程序编写者如何使用 DMA 引擎的 Slave-DMA API 的指南。 这仅适用于slave DMA 使用。
&emsp;&emsp;slave DMA 的使用包括以下步骤：
1. 分配一个 DMA slave 通道
2. 设置从机和控制器具体参数
3. 获取事务描述符
4. 提交交易
5. 发出待处理的请求并等待回调通知 

#### 1.6.1.1 Allocate a DMA slave channel
&emsp;&emsp;通道分配在 slave dma 上下文中略有不同，client drivers 通常只需要来自特定 DMA 控制器的通道，甚至在某些情况下需要指定的通道。
&emsp;&emsp;使用`dma_request_channel()`API 请求通道。 
&emsp;&emsp;接口：
```c
struct dma_chan *dma_request_channel(dma_cap_mask_t mask,
			dma_filter_fn filter_fn,
			void *filter_param);
```
&emsp;&emsp;上面的 dma_filter_fn 定义为：
```c
typedef bool (*dma_filter_fn)(struct dma_chan *chan, void *filter_param);
```
&emsp;&emsp;`filter_fn`参数是可选的，但强烈建议用于从通道和循环通道，因为它们通常需要获取特定的 DMA 通道。 
&emsp;&emsp;当可选的`filter_fn`参数为 NULL 时，dma_request_channel() 只返回满足 capability 掩码的第一个通道。
&emsp;&emsp;否则，将为每个具有“掩码”功能的空闲通道调用一次“filter_fn”例程。 当找到指定的 DMA 通道时，“filter_fn”应返回“true”。
&emsp;&emsp;通过此接口分配的通道对调用者来说是专有的，直到 dma_release_channel() 被调用。 
#### 1.6.1.2 Set slave and controller specific parameters
&emsp;&emsp;下一步总是将一些特定信息传递给 DMA 驱动程序。slave DMA 可以使用的大多数通用信息都在 struct dma_slave_config 中。 这允许客户端为外设指定 DMA 方向、DMA 地址、总线宽度、DMA 突发长度等。
&emsp;&emsp;如果某些 DMA 控制器有更多参数要发送，那么它们应该尝试将 struct dma_slave_config 嵌入到它们的 controller 特定结构中。 如果需要，这使客户端可以灵活地传递更多参数。
&emsp;&emsp;接口：
```c
int dmaengine_slave_config(struct dma_chan *chan,
			struct dma_slave_config *config)
```
&emsp;&emsp;请参阅 dmaengine.h 中的 dma_slave_config 结构体定义，了解结构体成员的详细说明。请注意`direction`成员将消失，因为它复制了 prepare 调用中给出的方向。 
#### 1.6.1.3 Get a descriptor for transaction
&emsp;&emsp;对于从机使用来说，DMA-engine 支持的各种从机传输模式包括：
* slave_sg：DMA a list of scatter gather buffers from/to a peripheral.
* dma_cyclic：从/向 外设执行循环 DMA 操作直到操作被明确停止。
* interleaved_dma：这对 Slave 和 M2M 客户端很常见。 对于驱动程序可能已经知道设备 fifo 的从地址。 可以通过为“dma_interleaved_template”成员设置适当的值来表达各种类型的操作。 

&emsp;&emsp;此传输 API 的非 NULL 返回表示给定传输的`descriptor`。 \
&emsp;&emsp;接口：
```c
struct dma_async_tx_descriptor *dmaengine_prep_slave_sg(
	struct dma_chan *chan, struct scatterlist *sgl,
	unsigned int sg_len, enum dma_data_direction direction,
	unsigned long flags);

struct dma_async_tx_descriptor *dmaengine_prep_dma_cyclic(
	struct dma_chan *chan, dma_addr_t buf_addr, size_t buf_len,
	size_t period_len, enum dma_data_direction direction);

struct dma_async_tx_descriptor *dmaengine_prep_interleaved_dma(
	struct dma_chan *chan, struct dma_interleaved_template *xt,
	unsigned long flags);
```
&emsp;&emsp;外围驱动程序应该在调用`dmaengine_prep_slave_sg()`之前为 DMA 操作映射了 scatterlist，并且必须保持 scatterlist 映射直到 DMA 操作完成。 必须使用 DMA struct device 映射 scatterlist。 如果稍后需要同步映射，也必须使用 DMA struct device 调用 dma_sync_*_for_*()。 
&emsp;&emsp;所以，正常的设置应该是这样的： 
```c
nr_sg = dma_map_sg(chan->device->dev, sgl, sg_len);
if (nr_sg == 0)
	/* error */

desc = dmaengine_prep_slave_sg(chan, sgl, nr_sg, direction, flags);
```
&emsp;&emsp;一旦获得描述符，就可以添加回调信息，然后必须提交描述符。 一些 DMA 引擎驱动程序可能在成功准备和提交之间持有自旋锁，因此这两个操作紧密配对非常重要。
&emsp;&emsp;注意：
&emsp;&emsp;尽管`async_tx API`指定 completion callback 例程不能提交任何新操作，但 slave/循环 DMA 并非如此。
&emsp;&emsp;对于 slave DMA，在调用回调函数之前，后续事务可能无法提交，因此允许 slave DMA 回调准备和提交新事务。
&emsp;&emsp;对于循环 DMA，回调函数可能希望通过 `dmaengine_terminate_all()`终止 DMA。
&emsp;&emsp;因此，在调用可能导致死锁的回调函数之前，DMA 引擎驱动程序删除所有锁是非常重要的。
&emsp;&emsp;请注意，回调将始终从 DMA 引擎 tasklet 中调用，而不是从中断上下文中调用。

#### 1.6.1.4 Submit the transaction
&emsp;&emsp;准备好描述符并添加回调信息后，必须将其放置在 DMA 引擎驱动程序挂起队列中。
&emsp;&emsp;接口：
```c
dma_cookie_t dmaengine_submit(struct dma_async_tx_descriptor *desc)
```
&emsp;&emsp;这将返回一个 cookie，可用于通过本文档未涵盖的其他 DMA 引擎调用检查 DMA 引擎活动的进度。
&emsp;&emsp;dmaengine_submit() 不会启动 DMA 操作，它只是将其添加到待处理队列中。 为此，请参阅步骤 5，dma_async_issue_pending。 
#### 1.6.1.5 Issue pending DMA requests and wait for callback notification
&emsp;&emsp;挂起队列中的事务可以通过调用 issue_pending API 来激活。 如果通道空闲，则启动队列中的第一个事务，随后的事务排队。
&emsp;&emsp;在每个 DMA 操作完成后，队列中的下一个启动并触发一个 tasklet。 tasklet 然后将调用客户端驱动程序 completion callback 例程以进行通知（如果已设置）。 
&emsp;&emsp;接口：
```c
void dma_async_issue_pending(struct dma_chan *chan);
```
#### 1.6.1.6 更多的 APIs
1. `int dmaengine_terminate_all(struct dma_chan *chan)`
&emsp;&emsp;这会导致 DMA 通道的所有活动停止，并且可能会丢弃 DMA FIFO 中尚未完全传输的数据。
&emsp;&emsp;任何未完成的传输都不会调用回调函数。 
2. `int dmaengine_pause(struct dma_chan *chan)`
&emsp;&emsp;这会暂停 DMA 通道上的活动，而不会丢失数据。 
3. `int dmaengine_resume(struct dma_chan *chan)`
&emsp;&emsp;恢复先前暂停的 DMA 通道。恢复当前未暂停的通道是无效的。 
4. `enum dma_status dma_async_is_tx_complete(struct dma_chan *chan,
        dma_cookie_t cookie, dma_cookie_t *last, dma_cookie_t *used)`
&emsp;&emsp;这可用于检查通道的状态。 有关此 API 的更完整描述，请参阅 include/linux/dmaengine.h 中的文档。
&emsp;&emsp;这可以与 dma_async_is_complete() 和从 dmaengine_submit() 返回的 cookie 结合使用，以检查特定 DMA 事务的完成情况。 
&emsp;&emsp;注意：并非所有 DMA 引擎驱动程序都可以为正在运行的 DMA 通道返回可靠信息。建议 DMA 引擎用户在使用此 API 之前暂停或停止（通过 dmaengine_terminate_all()）通道。 


### 1.6.2 dmatest.txt
&emsp;&emsp;这个小文档介绍了如何使用 dmatest 模块测试 DMA 驱动程序。 
&emsp;&emsp;测试套件仅适用于至少具有以下一项功能的通道：DMA_MEMCPY（内存到内存）、DMA_MEMSET（常量到内存或内存到内存，当模拟时）、DMA_XOR、DMA_PQ。 
#### 1.6.2.1 How to build the test module
&emsp;&emsp;menuconfig 包含一个可以通过以下路径找到的选项： 
```shell
Device Drivers -> DMA Engine support -> DMA Test client
```
&emsp;&emsp;在配置文件中该选项称为 CONFIG_DMATEST。 dmatest 可以构建为模块或内核。 让我们考虑一下这些情况。 
#### 1.6.2.2 When dmatest is built as a module
&emsp;&emsp;使用示例 
```shell
    % modprobe dmatest channel=dma0chan0 timeout=2000 iterations=1 run=1
```
&emsp;&emsp;或：
```shell
    % modprobe dmatest
    % echo dma0chan0 > /sys/module/dmatest/parameters/channel
    % echo 2000 > /sys/module/dmatest/parameters/timeout
    % echo 1 > /sys/module/dmatest/parameters/iterations
    % echo 1 > /sys/module/dmatest/parameters/run
```
&emsp;&emsp;或 on the kernel command line：
```shell
    dmatest.channel=dma0chan0 dmatest.timeout=2000 dmatest.iterations=1 dmatest.run=1
```
&emsp;&emsp;hint：可以通过运行以下命令来提取可用频道列表： 
```shell
    % ls -1 /sys/class/dma/
```
&emsp;&emsp;一旦启动，就会发出类似“dmatest: Started 1 threads using dma0chan0”的消息。 之后只报告测试失败消息，直到测试停止。 
&emsp;&emsp;请注意，运行新测试不会停止任何正在进行的测试。
&emsp;&emsp;以下命令返回测试的状态：
```shell
    % cat /sys/module/dmatest/parameters/run
```
&emsp;&emsp;要等待测试完成，用户空间可以 poll “run” 直到它为假，或者使用等待参数。 在加载模块时指定 'wait=1' 会导致模块初始化暂停，直到测试运行完成，而读取 /sys/module/dmatest/parameters/wait 会在返回之前等待任何正在运行的测试完成。 例如，以下脚本在退出之前等待 42 次测试完成。 请注意，如果“迭代”设置为“无限”，则禁用等待。 
&emsp;&emsp;示例：
```shell
    % modprobe dmatest run=1 iterations=42 wait=1
    % modprobe -r dmatest
```
&emsp;&emsp;或：
```shell
    % modprobe dmatest run=1 iterations=42
    % cat /sys/module/dmatest/parameters/wait
    % modprobe -r dmatest
```
#### 1.6.2.3 When built-in in the kernel
&emsp;&emsp;提供给内核命令行的模块参数将用于第一次执行的测试。 用户获得控制后，可以使用相同或不同的参数重新运行测试。 有关详细信息，请参阅上面的“第 2 部分 - 将 dmatest 构建为模块时”部分。 
&emsp;&emsp;在这两种情况下，模块参数都用作测试用例的实际值。 您总是可以在运行时通过运行检查它们：
```shell
    % grep -H . /sys/module/dmatest/parameters/*
```
#### 1.6.2.4 Gathering the test results
&emsp;&emsp;测试结果以以下格式打印到内核日志缓冲区：
```shell
    "dmatest: result <channel>: <test id>: '<error msg>' with src_off=<val> dst_off=<val> len=<val> (<err code>)"
```
&emsp;&emsp;示例输出：
```shell
    % dmesg | tail -n 1
    dmatest: result dma0chan0-copy0: #1: No errors with src_off=0x7bf dst_off=0x8ad len=0x3fea (0)
```
&emsp;&emsp;消息格式在不同类型的错误之间是统一的。 括号中的数字代表附加信息，例如 错误代码、错误计数器或状态。 测试线程还会在完成时发出一个摘要行，列出执行的测试数量、失败的数量和结果代码。 
&emsp;&emsp;示例：
```c
    % dmesg | tail -n 1
    dmatest: dma0chan0-copy0: summary 1 test, 0 failures 1000 iops 100000 KB/s (0)
```
&emsp;&emsp;还会发出数据错误比较错误的详细信息，但不要遵循上述格式。 

### 1.6.3 provider.txt
#### 1.6.3.1 硬件介绍
&emsp;&emsp;大多数 slave DMA 控制器具有相同的通用的操作规则。
&emsp;&emsp;它们有给定数量的通道用于 DMA 传输，以及给定数量的请求行。 
&emsp;&emsp;请求和通道几乎是正交的。 通道可用于为多个或任何请求提供服务。简单来说，通道是进行复制的实体，并请求涉及哪些端点。
&emsp;&emsp;请求线路实际上对应于从符合 DMA 条件的设备到控制器本身的物理线路。 每当设备想要开始传输时，它都会通过断言该请求线来断言 DMA 请求 (DRQ)。 
&emsp;&emsp;一个非常简单的 DMA 控制器只考虑一个参数：传输大小。 在每个时钟周期，它将一个字节的数据从一个缓冲区传输到另一个缓冲区，直到达到传输大小。
&emsp;&emsp;这在现实世界中效果不佳，因为从设备可能需要在单个周期内传输特定数量的位。例如，在执行简单的内存复制操作时，我们可能希望传输物理总线允许尽可能多的数据以最大限度地提高性能，但我们的音频设备可能具有更窄的 FIFO，需要一次精确写入 16 位或 24 位数据。这就是为什么大多数（如果不是全部）DMA 控制器都可以使用称为传输宽度的参数来调整它的原因。 
&emsp;&emsp;此外，一些 DMA 控制器，无论何时将 RAM 用作源或目标，都可以将内存中的读取或写入分组到缓冲区中，因此不会有很多小内存访问，这不是很有效，你会得到几个更大的传输。 这是使用一个称为 burst size 的参数来完成的，该参数定义了在控制器将传输拆分为较小的子传输的情况下允许进行多少次单次读取/写入。
&emsp;&emsp;我们理论上的 DMA 控制器将只能进行涉及单个连续数据块的传输。 但是，我们通常进行的一些传输不是，并且希望将数据从非连续缓冲区复制到连续缓冲区，这称为 scatter-gather。 
&emsp;&emsp;DMAEngine，至少对于 mem2dev 传输，需要对 scatter-gather 的支持。 所以我们这里有两种情况：要么我们有一个不支持它的非常简单的 DMA 控制器，我们必须用软件实现它，或者我们有一个更高级的 DMA 控制器，它在硬件分散中实现 scatter-gather。
&emsp;&emsp;后者通常使用一组要传输的块进行编程，每当开始传输时，控制器都会检查该集合，执行我们在那里编程的任何操作。 
&emsp;&emsp;该集合通常是表或链表。然后，您将表的地址及其元素数量或列表的第一项推送到 DMA 控制器的一个通道，并且每当断言 DRQ 时，它将通过集合以知道从哪里获取数据。
&emsp;&emsp;无论哪种方式，此集合的格式完全取决于您的硬件。每个 DMA 控制器都需要不同的结构，但是对于每个块，它们都需要至少源地址和目标地址，是否应该增加这些地址以及我们之前看到的三个参数：突发大小，传输宽度和传输大小。 
&emsp;&emsp;最后一件事是，通常从属设备默认不会发出 DRQ，如果您愿意使用 DMA，就必须首先在从属设备驱动程序中启用此功能。
&emsp;&emsp;这些只是一般的内存到内存（也称为 mem2mem）或内存到设备（mem2dev）类型的传输。 大多数设备通常支持 dmaengine 支持的其他类型的传输或内存操作，将在本文档后面详细介绍。 
#### 1.6.3.2 Linux 支持的 DMA
&emsp;&emsp;从历史上看，DMA 控制器驱动程序是使用异步（async） TX API 实现的，卸载内存复制、XOR、加密等操作，基本上是任何内存到内存的操作。
&emsp;&emsp;随着时间的推移，出现了对内存到设备传输的需求，并且扩展了 dmaengine。 如今，异步 TX API 被编写为 dmaengine 之上的一层，并充当客户端。 尽管如此，dmaengine 在某些情况下仍能容纳该 API，并做出了一些设计选择以确保其保持兼容。
&emsp;&emsp;有关 Async TX API 的更多信息，请查看 Documentation/crypto/async-tx-api.txt 中的相关文档文件。 
#### 1.6.3.3 DMAEngine 注册 
##### 1.6.3.3.1 struct dma_device Initialization
&emsp;&emsp;就像任何其他内核框架一样，整个 DMAEngine 注册依赖于驱动程序填充结构并针对框架进行注册。在我们的例子中，该结构是 dma_device。
&emsp;&emsp;您需要在驱动程序中做的第一件事是分配此结构。 任何常用的内存分配器都可以，但您还需要在其中初始化一些字段： 
* channels：例如，应该使用 INIT_LIST_HEAD 宏将其初始化为列表
* src_addr_widths：应包含支持的源传输宽度的位掩码 
* dst_addr_widths：应包含支持的目标传输宽度的位掩码 
* directions：应包含支持的 slave 方向的位掩码（即不包括 mem2mem 传输） 
* residue_granularity：
  * 报告给 dma_set_residue 的转移残基的粒度。 
  * 这可以是：
    * Descriptor：您的设备不支持任何类型的残留报告。 框架只会知道特定的事务描述符已完成。 
    * Segment：您的设备能够报告已传输的块 
    * Burst：您的设备能够报告已传输的 burst
* dev：应该保存指向与当前驱动程序实例关联的结构设备的指针
##### 1.6.3.3.2 Supported transaction types
&emsp;&emsp;接下来您需要设置您的设备（和驱动程序）支持的事务类型。
&emsp;&emsp;我们的 dma_device 结构有一个名为 cap_mask 的字段，其中包含支持的各种类型的事务，您需要使用 dma_cap_set 函数修改此掩码，并根据您作为参数支持的事务类型使用各种标志。
&emsp;&emsp;所有这些功能都在 dma_transaction_type 枚举中定义，位于 include/linux/dmaengine.h 
&emsp;&emsp;目前，可用的类型有： 
* DMA_MEMCPY：该设备能够进行内存到内存的复制 
* DMA_XOR：
  * 该设备能够对内存区域执行异或操作
  * 用于加速 XOR 密集型任务，例如 RAID5 
* DMA_XOR_VAL：该设备能够使用 XOR 算法对内存缓冲区执行奇偶校验。 
* DMA_PQ：该设备能够执行 RAID6 P+Q 计算，P 是简单的 XOR，Q 是 Reed-Solomon 算法。 
* DMA_PQ_VAL：该设备能够使用 RAID6 P+Q 算法对内存缓冲区执行奇偶校验。 
* DMA_INTERRUPT：
  * 该设备能够触发将产生周期性中断的虚拟传输 
  * 由客户端驱动程序用于注册回调，该回调将通过 DMA 控制器中断定期调用 
* DMA_SG：
  * 该设备支持内存到内存的 scatter-gather 传输。 
  * 即使一个普通的 memcpy 看起来像一个 scatter-gather 传输的特殊情况，只有一个块要传输，它是 mem2mem 传输案例中的一种不同的交易类型 
* DMA_PRIVATE：这些设备仅支持从属传输，因此不适用于异步传输。 
* DMA_ASYNC_TX：不能由设备设置，需要时由框架设置 
* DMA_SLAVE：
  * 该设备可以处理设备到内存的传输，包括 scatter-gather 传输。 
  * 虽然在 mem2mem 案例中，我们有两种不同的类型来处理要复制的单个块或它们的集合，但在这里，我们只有一个应该处理两者的事务类型。 
  * 如果要传输单个连续的内存缓冲区，只需构建一个只有一项的分散列表。
* DMA_CYCLIC
  * 该设备可以处理循环传输。 
  * 循环传输是块集合将在自身上循环的传输，最后一个项目指向第一个。 
  * 它通常用于音频传输，您希望在单个环形缓冲区上进行操作，您将使用音频数据填充该缓冲区。
* DMA_INTERLEAVE
  * 该设备支持交错传输。 
  * 这些传输可以将数据从非连续缓冲区传输到非连续缓冲区，而 DMA_SLAVE 可以将数据从非连续数据集传输到连续目标缓冲区。 
  * 它通常用于 2d 内容传输，在这种情况下，您希望将一部分未压缩数据直接传输到显示器进行打印 

&emsp;&emsp;这些不同的类型随时间的变化也会影响源地址和目标地址。
&emsp;&emsp;指向 RAM 的地址通常在每次传输后递增（或递减）。如果是环形缓冲区，它们可能会循环（DMA_CYCLIC）。指向设备寄存器（例如 FIFO）的地址通常是固定的。

##### 1.6.3.3.3 Device operations
&emsp;&emsp;我们的 dma_device 结构体还需要一些函数指针来实现实际的逻辑，现在我们已经描述了我们能够执行的操作。
&emsp;&emsp;我们必须在那里填写并因此必须实现的功能显然取决于您报告的支持的传输类型。 
* device_alloc_chan_resources
* device_free_chan_resources
  * 每当驱动程序在与该驱动程序关联的通道上第一次/最后一次调用 dma_request_channel 或 dma_release_channel 时，都会调用这些函数。
  * 他们负责分配/释放所有需要的资源，以使该通道对您的驱动程序有用。 
  * 这些函数可以休眠。 
* device_prep_dma_*
  * 这些功能与您之前注册的功能相匹配。 
  * 这些函数都获取与正在准备的传输相关的缓冲区或分散列表，并应从中创建硬件描述符或硬件描述符列表 
  * 这些函数可以从中断上下文中调用 
  * 您可能进行的任何分配都应该使用 GFP_NOWAIT 标志，以免潜在地休眠，但没有也不会耗尽紧急池。 
  * 驱动程序应尝试在探测时预先分配在传输设置期间可能需要的任何内存，以避免对 nowait 分配器施加太大压力。 
  * 它应该返回 dma_async_tx_descriptor 结构的唯一实例，进一步表示此特定传输。 
  * 这个结构可以使用函数 dma_async_tx_descriptor_init 来初始化。 
  * 您还需要在此结构中设置两个字段： 
    * flags：TODO：它可以由驱动程序本身修改，还是应该始终是参数中传递的标志 
    * tx_submit：指向您必须实现的函数的指针，该函数应该将当前事务描述符推送到挂起队列，等待调用 issue_pending。 
* device_issue_pending
  * 获取待处理队列中的第一个事务描述符，并开始传输。每当该传输完成时，它应该移动到列表中的下一个事务。
  * 该函数可以在中断上下文中调用 
* device_tx_status
  * 应该报告给定通道上剩余的字节数
  * 应该只关心作为参数传递的事务描述符，而不是给定通道上当前活动的描述符 
  * tx_state 参数可能为 NULL 
  * 应该使用 dma_set_residue 报告它 
  * 在循环转移的情况下，它应该只考虑当前周期。 
  * 该函数可以在中断上下文中调用。 
* device_config
  * 使用作为参数给出的配置重新配置通道 
  * 此命令不应同步执行，也不应在任何当前排队的传输上执行，而应仅在后续传输上执行 
  * 在这种情况下，该函数将接收一个 dma_slave_config 结构指针作为参数，它将详细说明要使用的配置。 
  * 尽管该结构包含一个方向字段，但该字段已被弃用，取而代之的是提供给 prep_* 函数的方向参数 
  * 此调用仅对从属操作是强制性的。 不应为 memcpy 操作设置或期望设置此项。如果驱动程序同时支持这两者，它应该只将此调用用于从属操作，而不用于 memcpy 操作。 
* device_pause
  * 暂停通道上的传输 
  * 该命令应该在通道上同步操作，立即暂停给定通道的工作 
* device_resume
  * 恢复通道上的传输 
  * 该命令应该在通道上同步操作，立即暂停给定通道的工作 
* device_terminate_all
  * 中止通道上所有未决和正在进行的传输 
  * 该命令应该在通道上同步操作，立即终止所有通道 

##### 1.6.3.3.4 杂项说明（应该记录的东西，但真的不知道把它们放在哪里） 
* dma_run_dependencies
  * 应该在异步 TX 传输结束时调用，并且在从传输情况下可以忽略。 
  * 确保在将相关操作标记为完成之前运行相关操作。 
* dma_cookie_t
  * 它是一个 DMA 事务 ID，会随着时间的推移而增加。 
  * 自从引入将其抽象出来的 virt-dma 后，它就不再真正相关了。 
* DMA_CTRL_ACK
  * 未记录的功能 
  * 除了与重用 DMA 事务描述符或在 async-tx API 中添加其他事务有关之外，没有人真正了解它的含义 
  * 在从 API 的情况下无用 
##### 1.6.3.3.5 General Design Notes
&emsp;&emsp;您将看到的大多数 DMAEngine 驱动程序都基于类似的设计，该设计在处理程序中处理传输中断的结束，但将大部分工作推迟到一个小任务中，包括在前一次传输结束时开始新的传输。
&emsp;&emsp;虽然这是一个相当低效的设计，因为传输间的延迟不仅是中断延迟，还有 tasklet 的调度延迟，这会使通道处于空闲状态，从而降低全局传输速率。
&emsp;&emsp;你应该避免这种做法，而不是在你的 tasklet 中选择一个新的传输，而是将该部分移动到中断处理程序，以获得更短的空闲窗口（无论如何我们都无法真正避免）。 

### 1.6.4 async-tx-api.txt
异步传输/转换 API 
#### 1.6.4.1 介绍
&emsp;&emsp;async_tx API 提供了用于描述异步大容量内存传输/转换链的方法，并支持事务间依赖关系。 它被实现为一个 dmaengine 客户端，可以平滑不同硬件卸载引擎实现的细节。 写入 API 的代码可以针对异步操作进行优化，并且 API 将使操作链适合可用的卸载资源。 
#### 1.6.4.2 GENEALOGY
&emsp;&emsp;API 最初设计用于使用英特尔(R) Xscale 系列 I/O 处理器中的卸载引擎卸载 md-raid5 驱动程序的内存副本和异或奇偶校验计算。 它还建立在“dmaengine”层之上，该层是为使用英特尔(R) I/OAT 引擎卸载网络堆栈中的内存副本而开发的。 因此，以下设计特征浮出水面： 
1. 隐式同步路径：API 的用户不需要知道他们运行的平台是否具有卸载功能。 当引擎可用时，该操作将被卸载，否则将在软件中执行。 
2. 跨通道依赖链：API 允许提交依赖操作链，如 raid5 情况下的 xor->copy->xor。 API 自动处理从一个操作转换到另一个操作意味着硬件通道切换的情况。 
3. dmaengine 扩展以支持“memcpy”之外的多个客户端和操作类型 

#### 1.6.4.3 用法
##### 1.6.4.3.1 general format of the API
```c
struct dma_async_tx_descriptor *
async_<operation>(<op specific parameters>, struct async_submit ctl *submit)
```
##### 1.6.4.3.2 Supported operations
* memcpy：源缓冲区和目标缓冲区之间的内存复制 
* memset：用字节值填充目标缓冲区 
* xor：xor 一系列源缓冲区并将结果写入目标缓冲区 
* xor_val：xor 一系列源缓冲区，如果结果为零，则设置一个标志。 该实现防止写入内存 
* pq：从一系列源缓冲区生成 p+q（raid6 syndrome） 
* pq_val：验证 p 和或 q 缓冲区是否与给定的一系列源同步 
* datap：(raid6_datap_recov) 从给定的源恢复一个raid6数据块和p块 
* 2data：(raid6_2data_recov) 从给定源恢复 2 个 raid6 数据块 

##### 1.6.4.3.3 Descriptor management
&emsp;&emsp;返回值是<u>非 NULL 并且当操作已进入异步执行队列时指向“描述符”</u>。 描述符是回收的资源，在驱动程序的控制下，将在操作完成时重复使用。 当应用程序需要提交一系列操作时，它必须保证在提交依赖项之前描述符不会自动回收。 这要求应用程序在允许驱动程序回收（或释放）描述符之前确认所有描述符。 可以通过以下方法之一确认描述符： 
* 如果不提交子操作，则设置 ASYNC_TX_ACK 标志 
* 将未确认的描述符作为对另一个 async_tx 调用的依赖项提交将隐式设置已确认状态。 
* 在描述符上调用 async_tx_ack()。

##### 1.6.4.3.4 When does the operation execute
&emsp;&emsp;async_\<operation\> 调用返回后，操作不会立即发出。 驱动批处理操作，通过减少管理通道所需的 mmio 周期数来提高性能。 一旦达到特定于驱动程序的阈值，驱动程序就会自动发出挂起的操作。 应用程序可以通过调用 async_tx_issue_pending_all() 来强制执行此事件。 这对所有通道都有效，因为应用程序不知道通道到操作的映射。 
##### 1.6.4.3.5 When does the operation complete
&emsp;&emsp;应用程序有两种方法可以了解操作的完成情况。 
1. 调用 dma_wait_for_async_tx()。 此调用导致 CPU 在轮询操作是否完成时旋转。 它处理依赖链和发出挂起的操作。 
2. 指定一个完成的回调函数。 如果驱动程序支持中断，则回调例程在 tasklet 上下文中运行，或者如果在软件中同步执行操作，则在应用程序上下文中调用它。 回调可以在对 async_\<operation\> 的调用中设置，或者当应用程序需要提交一个未知长度的链时，它可以使用 async_trigger_callback() 例程在链的末尾设置一个完成中断/回调。 

##### 1.6.4.3.6 constraints 约束
1. 在 IRQ 上下文中不允许调用 async_\<operation\>。 如果不违反约束 #2，则允许其他上下文。 
2. 完成回调例程不能提交新操作。 这导致<u>同步情况下的递归</u>和<u>异步情况下获得两次 spin_locks</u>。 

##### 1.6.4.3.7 示例
&emsp;&emsp;执行 xor->copy->xor 操作，其中每个操作取决于前一个操作的结果： 
```c
void callback(void *param)
{
	struct completion *cmp = param;

	complete(cmp);
}

void run_xor_copy_xor(struct page **xor_srcs,
		      int xor_src_cnt,
		      struct page *xor_dest,
		      size_t xor_len,
		      struct page *copy_src,
		      struct page *copy_dest,
		      size_t copy_len)
{
	struct dma_async_tx_descriptor *tx;
	addr_conv_t addr_conv[xor_src_cnt];
	struct async_submit_ctl submit;
	addr_conv_t addr_conv[NDISKS];
	struct completion cmp;

	init_async_submit(&submit, ASYNC_TX_XOR_DROP_DST, NULL, NULL, NULL,
			  addr_conv);
	tx = async_xor(xor_dest, xor_srcs, 0, xor_src_cnt, xor_len, &submit)

	submit->depend_tx = tx;
	tx = async_memcpy(copy_dest, copy_src, 0, 0, copy_len, &submit);

	init_completion(&cmp);
	init_async_submit(&submit, ASYNC_TX_XOR_DROP_DST | ASYNC_TX_ACK, tx,
			  callback, &cmp, addr_conv);
	tx = async_xor(xor_dest, xor_srcs, 0, xor_src_cnt, xor_len, &submit);

	async_tx_issue_pending_all();

	wait_for_completion(&cmp);
}
```
&emsp;&emsp;有关标志的更多信息，请参见 include/linux/async_tx.h。 有关更多实现示例，请参阅 drivers/md/raid5.c 中的 ops_run_* 和 ops_complete_* 例程。 

#### 1.6.4.4 驱动程序开发说明 
##### 1.6.4.4.1 Conformance points
&emsp;&emsp;dmaengine 驱动程序需要有一些一致性的点来适应应用程序使用 async_tx API 所做的假设： 
1. Completion callbacks are expected to happen in tasklet context
   预计完成回调将在 tasklet 上下文中发生 
2. dma_async_tx_descriptor fields are never manipulated in IRQ context
   dma_async_tx_descriptor 字段永远不会在 IRQ 上下文中被操作 
3. 在描述符清理路径中使用 async_tx_run_dependencies() 来处理依赖操作的提交 

##### 1.6.4.4.2 
&emsp;&emsp;“我的应用程序需要对硬件通道进行独占控制” 这个要求主要源于使用 DMA 引擎驱动程序来支持设备到内存操作的情况。 由于许多平台特定的原因，无法共享执行这些操作的通道。 对于这些情况，提供了 dma_request_channel() 接口。 
&emsp;&emsp;接口是：
```c
struct dma_chan *dma_request_channel(dma_cap_mask_t mask,
				    dma_filter_fn filter_fn,
				    void *filter_param);
```
&emsp;&emsp;dma_filter_fn 定义如下：
```c
typedef bool (*dma_filter_fn)(struct dma_chan *chan, void *filter_param);
```
&emsp;&emsp;当可选的 'filter_fn' 参数设置为 NULL 时，dma_request_channel 只返回满足能力掩码的第一个通道。 否则，当掩码参数不足以指定必要的通道时，可以使用 filter_fn 例程来配置系统中的可用通道。 系统中的每个空闲通道都会调用一次 filter_fn 例程。 在看到合适的通道时 filter_fn 返回 DMA_ACK，它将该通道标记为来自 dma_request_channel 的返回值。 通过此接口分配的通道对调用者来说是专有的，直到 dma_release_channel() 被调用。 
&emsp;&emsp;DMA_PRIVATE 功能标志用于标记不应由通用分配器使用的 dma 设备。 如果知道通道将始终是私有的，则可以在初始化时设置它。 或者，当 dma_request_channel() 找到未使用的“公共”频道时设置它。 
&emsp;&emsp;在实现驱动程序和消费者时需要注意几点： 
1. 一旦通道被私有分配，通用分配器将不再考虑它，即使在调用 dma_release_channel() 之后也是如此。 
2. 由于功能是在设备级别指定的，因此具有多个通道的 dma_device 将具有所有通道公共或所有通道私有。 

#### 1.6.4.4 SOURCE
```shell
include/linux/dmaengine.h: core header file for DMA drivers and api users
drivers/dma/dmaengine.c: offload engine channel management routines
drivers/dma/: location for offload engine drivers
include/linux/async_tx.h: core header file for the async_tx api
crypto/async_tx/async_tx.c: async_tx interface to dmaengine and common code
crypto/async_tx/async_memcpy.c: copy offload
crypto/async_tx/async_xor.c: xor and xor zero sum offload
```




















