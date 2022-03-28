/* 此文件中的函数是给 dma driver provider 使用的，不是给 consumer 使用的 */

/**
 * 将参数 chan 的 cookie 和 completed_cookie 都初始化为 1
 * @chan: 待初始化的 dma channel
 */
static inline void dma_cookie_init(struct dma_chan *chan)


/**
 * 为 descriptor 申请一个 cookie
 * @tx: 待申请 cookie 的 descriptor
 *
 * 给 descriptor 申请一个在每个通道中唯一的非零的 cookie 值。（物理通道与多个描述符的对应？）
 * Note: 调用者应持有 lock 以防止并发。
 * 
 * 如果描述符对应的物理通道的抽象结构 dma_chan 的 cookie 不合法，则 tx 和 dma_chan 的
 * cookie 都赋值为 1；否则 tx 的 cookie 赋值为 dma_chan 的 cookie + 1（含义应该是：前
 * 面的 cookie 已经被别的 tx 使用了）
 */
static inline dma_cookie_t dma_cookie_assign(struct dma_async_tx_descriptor *tx)

/**
 * dma_cookie_complete - complete a descriptor
 * @tx: 待完成的描述符
 *
 * 通过更新通道完成 cookie 标记来标记此描述符完成。 将描述符 cookie 归零以防止意外重复完成。 
 *
 * Note: Note: 调用者应持有 lock 以防止并发。
 *
 * 物理通道的 completed_cookie 变成此完成的 descriptor 的 cookie
 * tx->chan->completed_cookie = tx->cookie;
 * 此描述符的 cookie 变成 0。
 * tx->cookie = 0;
 */
static inline void dma_cookie_complete(struct dma_async_tx_descriptor *tx)


/**
 * 淦，看的第三遍，我觉得我应该理解了这个逻辑了
 * dma_cookie_status - report cookie status
 * @chan: dma channel
 * @cookie: cookie we are interested in
 * @state: dma_tx_state structure to return last/used cookies
 *
 * Report the status of the cookie, filling in the state structure if
 * non-NULL.  No locking is required.
 */
static inline enum dma_status dma_cookie_status(struct dma_chan *chan,
	dma_cookie_t cookie, struct dma_tx_state *state)






