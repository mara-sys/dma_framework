```shell
# 文档
Documentation
    -->dmaengine
        -->00-INDEX
        -->client.txt   #client 使用 dma 的 API 指南
        -->dmatest.txt  #编译、配置、使用 dmatest 系统
        -->provider.txt #dma controller API 指南
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















## 1.6 内核文档翻译
### 1.6.1 client.txt
&emsp;&emsp;注意：有关 async_tx 中 DMA 引擎的使用，请参阅：Documentation/crypto/async-tx-api.txt 
&emsp;&emsp;下面是设备驱动程序编写者如何使用 DMA 引擎的 Slave-DMA API 的指南。 这仅适用于skave DMA 使用。
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
  * 
























