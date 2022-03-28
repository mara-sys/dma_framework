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
 * dma_cookie_status - 根据传输的参数 cookie 来判断拥有此 cookie 的描述符是否完成了传输
 * @chan: 物理通道的抽象
 * @cookie: 我们要检查是否传输完成的 cookie
 * @state: dma_tx_state structure to return last/used cookies
 *
 * Report the status of the cookie, filling in the state structure if
 * non-NULL.  No locking is required.
 * 
 * cookie 的逻辑是这样子的：对于一个物理通道的抽象 dma_chan，它有一个成员 cookie，cookie
 * 的类型是 s32（当变成负值以后，在申请 cookie 时，会将其设为 1，这就解释了为什么在 
 * dma_cookie_assign 函数中会有 cookie < DMA_MIN_COOKIE 的判断语句了）。每发起一次传输
 * 会有一个描述符，该描述符的 cookie 值是 dma_chan 中的 cookie 值加 1，每完成一次传输会
 * 将描述符的 cookie 值赋给 dma_chan 中的 completed_cookie。也就是说，所有“夹在” dma_chan
 * 的 completed_cookie 和 cookie 值之间的描述符的 cookie 值，都是未完成传输的。相反的区间
 * 就都是已完成传输的。
 * 由于 dma_chan 的 cookie 的类型是 s32，因此会存在溢出变为负值，再次 assign 重新设为 1 
 * 的情况。那么，“夹在”的意思就是下面两个示例中，省略号的区间：
 * 1. 计数没有溢出
 *          1、2、3、4、completed_cookie、······、cookie、 
 * 2. 计数溢出
 *          ······、cookie、cookie+1、cookie+2、cookie+n、completed_cookie、······
 * 调用函数 dma_async_is_complete 来判断 cookie 传输的状态，此函数的逻辑就是上面所述。
 * （啧啧啧，没错，合理，我觉得就是这样！！！）
 */
static inline enum dma_status dma_cookie_status(struct dma_chan *chan,
	dma_cookie_t cookie, struct dma_tx_state *state)
{
	dma_cookie_t used, complete;

	used = chan->cookie;
	complete = chan->completed_cookie;
	barrier();
	if (state) {
		state->last = complete;
		state->used = used;
		state->residue = 0;
	}
	return dma_async_is_complete(cookie, complete, used);
}

/* 这个在我要写的驱动里，硬件没有对应的功能 */
static inline void dma_set_residue(struct dma_tx_state *state, u32 residue)


