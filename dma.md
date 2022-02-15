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


### 1.6.1 dmatest.txt









