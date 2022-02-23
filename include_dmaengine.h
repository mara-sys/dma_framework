/**
 * Interleaved Transfer Request
 * ----------------------------
 * 一个 chunk（块）是指：要传输的，地址连续的字节的集合。两个块之间的间隙（gap，
 * 以字节为单位）称为块间间隙（ICG）。ICG 在两个块之间可能会也可能不会发生变化
 * 帧（frame）是连续的 {chunk,icg} pairs 的最小单位，帧重复整数次就指定了一次
 * 传输。一次传输的模板就是由帧的规格、帧的重复次数和别的每次传输的属性构成的。
 *
 * 实际上，客户端驱动程序在其生命周期中，会为其每种传输类型准备一个模板，并在
 * 提交（submit）请求前仅设置“stc_start”和“dst_start”。
 *
 *  |      Frame-1        |       Frame-2       | ~ |       Frame-'numf'  |
 *  |====....==.===...=...|====....==.===...=...| ~ |====....==.===...=...|
 *
 *    ==  Chunk size
 *    ... ICG
 */

/**
 * struct data_chunk - 组成帧的 sactter-gather 列表的元素。
 * @size: 从源地址读取的字节数
 *	  size_dst := fn(op, size_src), 对于目的地址不重要.
 * @icg: 在此块的最后一个 src/dst 地址之后和下一个块的第一个 src/dst 地址之前
 *       要跳过的字节数。
 *	 Ignored for dst(assumed 0), 如果 dst_inc 为真且 dst_sgl 为假.
 *	 Ignored for src(assumed 0), 如果 src_inc 为真且 src_sgl 为假.
 */
struct data_chunk {
	size_t size;
	size_t icg;
};

/**
 * struct dma_interleaved_template - 模板用于传达 DMAC 的传输模式和属性。
 * @src_start: 源地址的第一个块的总线地址。 
 * @dst_start: 目的地址的第一个块的总线地址。
 * @dir: 指定源和目标传输方向的类型。
 * @src_inc: 如果源地址在读取后增加则为真。 
 * @dst_inc: 如果目标地址在写入后增加则为真。 
 * @src_sgl: 如果 sgl[] 的 'icg' 适用于源地址(scattered read)则为真.
 *		否则，源地址中的数据是被连续读取的 (忽略 icg).
 *		如果 src_inc 为 false 则忽略 src_sgl.
 * @dst_sgl: If the 'icg' of sgl[] applies to Destination (scattered write).
 *		Otherwise, destination is filled contiguously (icg ignored).
 *		Ignored if dst_inc is false.
 * @numf: 此 template 中的帧的个数.
 * @frame_size: 帧中的块的个数，即 sgl[] 的大小.
 * @sgl: 组成帧的 {chunk,icg} 对的数组 。
 */
struct dma_interleaved_template {
	dma_addr_t src_start;
	dma_addr_t dst_start;
	enum dma_transfer_direction dir;
	bool src_inc;
	bool dst_inc;
	bool src_sgl;
	bool dst_sgl;
	size_t numf;
	size_t frame_size;
	struct data_chunk sgl[0];
};

/**
 * enum dma_ctrl_flags - 用以增强操作准备、控制完成和通信状态的 DMA 标志。 
 * @DMA_PREP_INTERRUPT - 在此事务完成时触发中断（回调函数） 
 * @DMA_CTRL_ACK - 如果清除，则在客户端确认收到之前不能重用描述符，即有机会建立
 *  任何依赖链 
 * @DMA_PREP_PQ_DISABLE_P - 在生成 Q 的同时防止生成 P 
 * @DMA_PREP_PQ_DISABLE_Q - 在生成 P 的同时防止生成 Q 
 * @DMA_PREP_CONTINUE - 向驱动表明，它正在使用上一次操作的结果的 buffers 作为
 *  源地址；在 PQ 操作的情况下，它将继续使用新源进行计算。
 * @DMA_PREP_FENCE - 告诉驱动程序后续操作取决于此操作的结果。
 */
enum dma_ctrl_flags {
	DMA_PREP_INTERRUPT = (1 << 0),
	DMA_CTRL_ACK = (1 << 1),
	DMA_PREP_PQ_DISABLE_P = (1 << 2),
	DMA_PREP_PQ_DISABLE_Q = (1 << 3),
	DMA_PREP_CONTINUE = (1 << 4),
	DMA_PREP_FENCE = (1 << 5),
};




/**
 * struct dma_chan - devices 提供 DMA channels, clients 使用它们
 * @device: 指向提供此通道的 dma 设备的指针，始终为 !%NULL 
 * @cookie: last cookie value returned to client
 * @completed_cookie: last completed cookie for this channel
 * @chan_id: channel ID for sysfs
 * @dev: class device for sysfs
 * @device_node: used to add this to the device chan list
 * @local: per-cpu pointer to a struct dma_chan_percpu
 * @client_count: how many clients are using this channel
 * @table_count: number of appearances in the mem-to-mem allocation table
 * @private: private data for certain client-channel associations
 */
struct dma_chan {
	struct dma_device *device;
	dma_cookie_t cookie;
	dma_cookie_t completed_cookie;

	/* sysfs */
	int chan_id;
	struct dma_chan_dev *dev;

	struct list_head device_node;
	struct dma_chan_percpu __percpu *local;
	int client_count;
	int table_count;
	void *private;
};

/**
 * struct dma_chan_dev - relate sysfs device node to backing channel device
 * @chan: driver channel device
 * @device: sysfs device
 * @dev_id: parent dma_device dev_id
 * @idr_ref: reference count to gate release of dma_device dev_id
 */
struct dma_chan_dev {
	struct dma_chan *chan;
	struct device device;
	int dev_id;
	atomic_t *idr_ref;
};

/**
 * struct dma_slave_config - dma slave channel 运行时配置
 * @direction: 数据是否应该在这个从通道上输入或输出，现在。 DMA_MEM_TO_DEV 和 
 * DMA_DEV_TO_MEM 是合法值。 已弃用，驱动程序应使用 device_prep_slave_sg 和 
 * device_prep_dma_cyclic 函数的方向参数或 dma_interleaved_template 结构中
 * 的 dir 字段。 
 * @src_addr: DMA slave data 应该从此物理地址读取 (RX), 如果 source 是 memory，
 * 忽略此参数。
 * @dst_addr: DMA slave data 应该向此物理地址写入 (TX), 如果 source 是 memory，
 * 忽略此参数。
 * @src_addr_width: this is the width in bytes of the source (RX)
 * register where DMA data shall be read. If the source
 * is memory this may be ignored depending on architecture.
 * 合法值: 1, 2, 4, 8.
 * @dst_addr_width: same as src_addr_width but for destination
 * target (TX) mutatis mutandis.
 * @src_maxburst: 一次突发中可以发送到设备的最大字数（注意：字，以 src_addr_width 
 * 成员为单位，而不是字节）。 通常是 I/O 外设上 FIFO 深度的一半，因此您不会溢出它。 
 * 这可能适用于也可能不适用于内存的源。 
 * @dst_maxburst: same as src_maxburst but for destination target
 * mutatis mutandis.
 * @device_fc: Flow Controller Settings. Only valid for slave channels. Fill
 * with 'true' if peripheral should be flow controller. Direction will be
 * selected at Runtime.
 * @slave_id: Slave requester id. Only valid for slave channels. The dma
 * slave peripheral will have unique id as dma requester which need to be
 * pass as slave config.
 *
 * 该结构作为配置数据传递给 DMA 引擎，以便在运行时为 DMA 传输设置特定通道。 DMA 
 * 设备/引擎必须为 dma_device 结构中的附加回调提供支持，device_config 和此结
 * 构将作为参数传递给函数。 
 *
 * The rationale for adding configuration information to this struct is as
 * follows: if it is likely that more than one DMA slave controllers in
 * the world will support the configuration option, then make it generic.
 * If not: if it is fixed so that it be sent in static from the platform
 * data, then prefer to do that.
 */
struct dma_slave_config {
	enum dma_transfer_direction direction;
	dma_addr_t src_addr;
	dma_addr_t dst_addr;
	enum dma_slave_buswidth src_addr_width;
	enum dma_slave_buswidth dst_addr_width;
	u32 src_maxburst;
	u32 dst_maxburst;
	bool device_fc;
	unsigned int slave_id;
};

/**
 * enum dma_residue_granularity - Granularity of the reported transfer residue
 * @DMA_RESIDUE_GRANULARITY_DESCRIPTOR: Residue reporting is not support. The
 *  DMA channel is only able to tell whether a descriptor has been completed or
 *  not, which means residue reporting is not supported by this channel. The
 *  residue field of the dma_tx_state field will always be 0.
 * @DMA_RESIDUE_GRANULARITY_SEGMENT: Residue is updated after each successfully
 *  completed segment of the transfer (For cyclic transfers this is after each
 *  period). This is typically implemented by having the hardware generate an
 *  interrupt after each transferred segment and then the drivers updates the
 *  outstanding residue by the size of the segment. Another possibility is if
 *  the hardware supports scatter-gather and the segment descriptor has a field
 *  which gets set after the segment has been completed. The driver then counts
 *  the number of segments without the flag set to compute the residue.
 * @DMA_RESIDUE_GRANULARITY_BURST: Residue is updated after each transferred
 *  burst. This is typically only supported if the hardware has a progress
 *  register of some sort (E.g. a register with the current read/write address
 *  or a register with the amount of bursts/beats/bytes that have been
 *  transferred or still need to be transferred).
 */
enum dma_residue_granularity {
	DMA_RESIDUE_GRANULARITY_DESCRIPTOR = 0,
	DMA_RESIDUE_GRANULARITY_SEGMENT = 1,
	DMA_RESIDUE_GRANULARITY_BURST = 2,
};


/**
 * struct dma_async_tx_descriptor - 异步 transaction descriptor
 * ---dma generic offload fields---
 * @cookie: 跟踪此事务的 cookie，如果此 tx 位于 dependency list 中，则设置为 
 * -EBUSY 
 * @flags: 用以增强操作准备、控制完成和通信状态的 DMA 标志。 
 * @phys: physical address of the descriptor
 * @chan: target channel for this operation
 * @tx_submit: accept the descriptor, assign ordered cookie and mark the
 * descriptor pending. To be pushed on .issue_pending() call
 * @callback: routine to call after this operation is complete
 * @callback_param: general parameter to pass to the callback routine
 * ---async_tx api specific fields---
 * @next: at completion submit this descriptor
 * @parent: pointer to the next level up in the dependency chain
 * @lock: protect the parent and next pointers
 */
struct dma_async_tx_descriptor {
	dma_cookie_t cookie;
	enum dma_ctrl_flags flags; /* not a 'long' to pack with cookie */
	dma_addr_t phys;
	struct dma_chan *chan;
	dma_cookie_t (*tx_submit)(struct dma_async_tx_descriptor *tx);
	dma_async_tx_callback callback;
	void *callback_param;
	struct dmaengine_unmap_data *unmap;
#ifdef CONFIG_ASYNC_TX_ENABLE_CHANNEL_SWITCH
	struct dma_async_tx_descriptor *next;
	struct dma_async_tx_descriptor *parent;
	spinlock_t lock;
#endif
};


/**
 * struct dma_tx_state - 报告传输的状态。
 * @last: 上次完成的DMA cookie 
 * @used: 上次发布的 DMA cookie（即正在进行的） 
 * @residue: 如果在驱动程序中实现了状态 DMA_IN_PROGRESS 和 DMA_PAUSED，
 * 则它表示在选定传输上剩余的要传输的字节数，否则为 0 
 */
struct dma_tx_state {
	dma_cookie_t last;
	dma_cookie_t used;
	u32 residue;
};



/**
 * struct dma_device - info on the entity supplying DMA services
 * @chancnt: 支持多少个 DMA 通道 
 * @privatecnt: dma_request_channel 请求了多少个 DMA 通道 
 * @channels: the list of struct dma_chan
 * @global_node: list_head for global dma_device_list
 * @cap_mask: one or more dma_capability flags
 * @max_xor: xor 源的最大数量，如果没有此功能，则为 0 
 * @max_pq: PQ 源的最大数量和 PQ-continue 能力 
 * @copy_align: alignment shift for memcpy operations
 * @xor_align: alignment shift for xor operations
 * @pq_align: alignment shift for pq operations
 * @dev_id: unique device ID
 * @dev: struct device reference for dma mapping api
 * @src_addr_widths: 设备支持的 src 地址宽度的位掩码
 * @dst_addr_widths: 设备支持的 dst 地址宽度的位掩码
 * @directions: 设备支持的 slave 方向的位掩码，因为枚举类型 dma_transfer_direction 
 * 没有定义成每种类型一个位的形式，dma 控制器应以 (1 << <TYPE>) 的方式填充，并且控制器
 * 也应检查是否相同。
 * @residue_granularity: granularity of the transfer residue reported
 *	by tx_status
 * @device_alloc_chan_resources: allocate resources and return the
 *	number of allocated descriptors
 * @device_free_chan_resources: release DMA channel's resources
 * @device_prep_dma_memcpy: prepares a memcpy operation
 * @device_prep_dma_xor: prepares a xor operation
 * @device_prep_dma_xor_val: prepares a xor validation operation
 * @device_prep_dma_pq: prepares a pq operation
 * @device_prep_dma_pq_val: prepares a pqzero_sum operation
 * @device_prep_dma_interrupt: prepares an end of chain interrupt operation
 * @device_prep_slave_sg: prepares a slave dma operation
 * @device_prep_dma_cyclic: prepare a cyclic dma operation suitable for audio.
 *	The function takes a buffer of size buf_len. The callback function will
 *	be called after period_len bytes have been transferred.
 * @device_prep_interleaved_dma: Transfer expression in a generic way.
 * @device_config: 将新配置推送配置到通道，返回 0 或错误代码 
 * @device_pause: 暂停通道上发生的任何传输。 返回 0 或错误代码 
 * @device_resume: 恢复先前暂停的通道上的任何传输。 返回 0 或错误代码 
 * @device_terminate_all: 中止通道上的所有传输。 返回 0 或错误代码 
 * @device_tx_status: poll for transaction completion, the optional
 *	txstate parameter can be supplied with a pointer to get a
 *	struct with auxiliary transfer status information, otherwise the call
 *	will just return a simple status code
 * @device_issue_pending: push pending transactions to hardware
 * 将待处理的事务推送到硬件 
 */
struct dma_device {

	unsigned int chancnt;
	unsigned int privatecnt;
	struct list_head channels;
	struct list_head global_node;
	dma_cap_mask_t  cap_mask;
	unsigned short max_xor;
	unsigned short max_pq;
	u8 copy_align;
	u8 xor_align;
	u8 pq_align;
	#define DMA_HAS_PQ_CONTINUE (1 << 15)

	int dev_id;
	struct device *dev;

	u32 src_addr_widths;
	u32 dst_addr_widths;
	u32 directions;
	enum dma_residue_granularity residue_granularity;

	int (*device_alloc_chan_resources)(struct dma_chan *chan);
	void (*device_free_chan_resources)(struct dma_chan *chan);

	struct dma_async_tx_descriptor *(*device_prep_dma_memcpy)(
		struct dma_chan *chan, dma_addr_t dst, dma_addr_t src,
		size_t len, unsigned long flags);
	struct dma_async_tx_descriptor *(*device_prep_dma_xor)(
		struct dma_chan *chan, dma_addr_t dst, dma_addr_t *src,
		unsigned int src_cnt, size_t len, unsigned long flags);
	struct dma_async_tx_descriptor *(*device_prep_dma_xor_val)(
		struct dma_chan *chan, dma_addr_t *src,	unsigned int src_cnt,
		size_t len, enum sum_check_flags *result, unsigned long flags);
	struct dma_async_tx_descriptor *(*device_prep_dma_pq)(
		struct dma_chan *chan, dma_addr_t *dst, dma_addr_t *src,
		unsigned int src_cnt, const unsigned char *scf,
		size_t len, unsigned long flags);
	struct dma_async_tx_descriptor *(*device_prep_dma_pq_val)(
		struct dma_chan *chan, dma_addr_t *pq, dma_addr_t *src,
		unsigned int src_cnt, const unsigned char *scf, size_t len,
		enum sum_check_flags *pqres, unsigned long flags);
	struct dma_async_tx_descriptor *(*device_prep_dma_interrupt)(
		struct dma_chan *chan, unsigned long flags);
	struct dma_async_tx_descriptor *(*device_prep_dma_sg)(
		struct dma_chan *chan,
		struct scatterlist *dst_sg, unsigned int dst_nents,
		struct scatterlist *src_sg, unsigned int src_nents,
		unsigned long flags);

	struct dma_async_tx_descriptor *(*device_prep_slave_sg)(
		struct dma_chan *chan, struct scatterlist *sgl,
		unsigned int sg_len, enum dma_transfer_direction direction,
		unsigned long flags, void *context);
	struct dma_async_tx_descriptor *(*device_prep_dma_cyclic)(
		struct dma_chan *chan, dma_addr_t buf_addr, size_t buf_len,
		size_t period_len, enum dma_transfer_direction direction,
		unsigned long flags);
	struct dma_async_tx_descriptor *(*device_prep_interleaved_dma)(
		struct dma_chan *chan, struct dma_interleaved_template *xt,
		unsigned long flags);

	int (*device_config)(struct dma_chan *chan,
			     struct dma_slave_config *config);
	int (*device_pause)(struct dma_chan *chan);
	int (*device_resume)(struct dma_chan *chan);
	int (*device_terminate_all)(struct dma_chan *chan);

	enum dma_status (*device_wait_tasklet)(struct dma_chan *chan);
	enum dma_status (*device_tx_status)(struct dma_chan *chan,
					    dma_cookie_t cookie,
					    struct dma_tx_state *txstate);
	void (*device_issue_pending)(struct dma_chan *chan);
};






#define dma_request_channel(mask, x, y) __dma_request_channel(&(mask), x, y)










