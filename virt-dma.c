
/**  
 * 根据 dma_chan 获取 virt_dma_chan 的地址
 */
static inline struct virt_dma_chan *to_virt_chan(struct dma_chan *chan)
{
	return container_of(chan, struct virt_dma_chan, chan);
}

/**  
 * 根据 dma_async_tx_descriptor 获取 virt_dma_desc 的地址
 */
static struct virt_dma_desc *to_virt_desc(struct dma_async_tx_descriptor *tx)
{
	return container_of(tx, struct virt_dma_desc, tx);
}

/** 
 * 根据 tx 描述符获取 virt_dma_chan 和 virt_dma_desc
 * 给 tx 描述符申请一个 cookie 值
 * 添加 list_head：在 virt_dma_chan 的 desc_submitted 链表中插入 virt_dma_desc 的 node 节点
 * 返回值为 cookie 的值
 */
dma_cookie_t vchan_tx_submit(struct dma_async_tx_descriptor *tx)


/** 
 * 从 virt_dma_chan 的 desc_issued 链表中查找和 cookie 值相等的 virt_dma_desc
 */
struct virt_dma_desc *vchan_find_desc(struct virt_dma_chan *vc,
	dma_cookie_t cookie)


/** 
 * tasklet handles 
 * 一个 DMA descriptor 完成以后回调此函数并释放 descriptor
 * 输入参数 arg 是 virt_dma_chan 的地址
 */
static void vchan_complete(unsigned long arg)





/** 
 * 对 virt_dma_chan 的 dma_chan 的 cookie 初始化，值为 1
 * 初始化 virt_dma_chan 的 desc_submitted、desc_issued、desc_completed
 * 初始化 tasklet_struct
 * 将 virt_dma_chan 的 dma_chan 的 list_head device_node 添加到 dma_device 
 * 的 list_head channels
 */
void vchan_init(struct virt_dma_chan *vc, struct dma_device *dmadev)













