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























