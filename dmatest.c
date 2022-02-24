static unsigned int test_buf_size = 16384;
module_param(test_buf_size, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(test_buf_size, "Size of the memcpy test buffer");

/* 
 * 使用 module_param 导出下面这些变量到 sysfs 中
 * unsigned int test_buf_size
 * char test_channel[20]
 * char test_device[32]
 * unsigned int threads_per_chan
 * unsigned int max_channels
 * unsigned int iterations
 * unsigned int dmatest
 * unsigned int xor_sources
 * unsigned int pq_sources
 * int timeout
 * bool noverify
 * bool norandom
 * bool verbose
 */

// 下面设置了 sysfs 中 run 参数的回调函数
static int dmatest_run_set(const char *val, const struct kernel_param *kp);
static int dmatest_run_get(char *val, const struct kernel_param *kp);
static const struct kernel_param_ops run_ops = {
	.set = dmatest_run_set,
	.get = dmatest_run_get,
};
static bool dmatest_run;
module_param_cb(run, &run_ops, &dmatest_run, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(run, "Run the test (default: false)");


// 检查 dmatest_info 中的每个通道，并检查每个通道的每个 list_head 
// 上是否有传输任务，如果有任务正在运行，返回 true 
static bool is_threaded_test_run(struct dmatest_info *info)

/* 产生一个 long 的随机数 */
static unsigned long dmatest_random(void)

/* 产生一个值 */
static inline u8 gen_inv_idx(u8 index, bool is_memset)

/* 产生源的值 */
inline u8 gen_src_value(u8 index, bool is_memset)

/* 生成目的地址的值 */
inline u8 gen_dst_value(u8 index, bool is_memset)

/* 源地址值初始化 */
static void dmatest_init_srcs(u8 **bufs, unsigned int start, unsigned int len,
		unsigned int buf_size, bool is_memset)

/* 目的地址值初始化 */
static void dmatest_init_dsts(u8 **bufs, unsigned int start, unsigned int len,
		unsigned int buf_size, bool is_memset)

/* 打印错误情况的信息 */
static void dmatest_mismatch(u8 actual, u8 pattern, unsigned int index,
		unsigned int counter, bool is_srcbuf, bool is_memset)

/* 验证传输的值是否正确 */
static unsigned int dmatest_verify(u8 **bufs, unsigned int start,
		unsigned int end, unsigned int counter, u8 pattern,
		bool is_srcbuf, bool is_memset)

/* 运行时间 */
static unsigned long long dmatest_persec(s64 runtime, unsigned int val)

/* 速率 */
static unsigned long long dmatest_KBs(s64 runtime, unsigned long long len)

/* 主要的测试函数 */
static int dmatest_func(void *data)




