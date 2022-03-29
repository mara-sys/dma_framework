static DEFINE_MUTEX(dma_list_mutex);
static DEFINE_IDR(dma_idr);
static LIST_HEAD(dma_device_list);


/** 
 * 判断 dma_device 具有的能力和 mask 要求的是否一样
 */
#define dma_device_satisfies_mask(device, mask)


/**
 * dma_chan_get - try to grab a dma channel's parent driver module
 * @chan - channel to grab
 *
 * Must be called under dma_list_mutex
 */
static int dma_chan_get(struct dma_chan *chan)
{
	struct module *owner = dma_chan_to_owner(chan);
	int ret;

	/* The channel is already in use, update client count */
	if (chan->client_count) {
		__module_get(owner);
		goto out;
	}

	if (!try_module_get(owner))
		return -ENODEV;

	/* allocate upon first client reference */
	if (chan->device->device_alloc_chan_resources) {
		ret = chan->device->device_alloc_chan_resources(chan);
		if (ret < 0)
			goto err_out;
	}

	if (!dma_has_cap(DMA_PRIVATE, chan->device->cap_mask))
		balance_ref_count(chan);

out:
	chan->client_count++;
	return 0;

err_out:
	module_put(owner);
	return ret;
}

/**
 * dma_chan_put - drop a reference to a dma channel's parent driver module
 * @chan - channel to release
 *
 * Must be called under dma_list_mutex
 */
static void dma_chan_put(struct dma_chan *chan)
{
	/* This channel is not in use, bail out */
	if (!chan->client_count)
		return;

	chan->client_count--;
	module_put(dma_chan_to_owner(chan));

	/* This channel is not in use anymore, free it */
	if (!chan->client_count && chan->device->device_free_chan_resources)
		chan->device->device_free_chan_resources(chan);
}




/** 
 * 返回一个可用的不繁忙的通道
 * 首先判断：如果 dma_device 通道数大于 1，且是 DMA_PRIVATE 的类型，
 *      如果有一个通道被使用就返回 NULL ？
 * 然后再 dma_device 的 channels list_head 中查找未使用的通道并返回
 */
static struct dma_chan *private_candidate(const dma_cap_mask_t *mask,
					  struct dma_device *dev,
					  dma_filter_fn fn, void *fn_param)

/**  
 * 
 */
static struct dma_chan *find_candidate(struct dma_device *device,
				       const dma_cap_mask_t *mask,
				       dma_filter_fn fn, void *fn_param)
{
	struct dma_chan *chan = private_candidate(mask, device, fn, fn_param);
	int err;

	if (chan) {
		/* Found a suitable channel, try to grab, prep, and return it.
		 * We first set DMA_PRIVATE to disable balance_ref_count as this
		 * channel will not be published in the general-purpose
		 * allocator
		 */
		dma_cap_set(DMA_PRIVATE, device->cap_mask);
		device->privatecnt++;
		err = dma_chan_get(chan);

		if (err) {
			if (err == -ENODEV) {
				dev_dbg(device->dev, "%s: %s module removed\n",
					__func__, dma_chan_name(chan));
				list_del_rcu(&device->global_node);
			} else
				dev_dbg(device->dev,
					"%s: failed to get %s: (%d)\n",
					 __func__, dma_chan_name(chan), err);

			if (--device->privatecnt == 0)
				dma_cap_clear(DMA_PRIVATE, device->cap_mask);

			chan = ERR_PTR(err);
		}
	}

	return chan ? chan : ERR_PTR(-EPROBE_DEFER);
}



/** 
 *  尝试分配一个独占通道
 * mask：capabilities that the channel must satisfy
 * fn：用于处置可用通道的可选的回调
 * fn_param：传递给 dma_filter_fn 的不透明参数 
 */

#define dma_request_channel(mask, x, y) __dma_request_channel(&(mask), x, y)

struct dma_chan *__dma_request_channel(const dma_cap_mask_t *mask,
				       dma_filter_fn fn, void *fn_param)
{
	struct dma_device *device, *_d;
	struct dma_chan *chan = NULL;
	int err;

	/* Find a channel */
	mutex_lock(&dma_list_mutex);
	list_for_each_entry_safe(device, _d, &dma_device_list, global_node) {
		chan = private_candidate(mask, device, fn, fn_param);
		if (chan) {
			/* Found a suitable channel, try to grab, prep, and
			 * return it.  We first set DMA_PRIVATE to disable
			 * balance_ref_count as this channel will not be
			 * published in the general-purpose allocator
			 */
			dma_cap_set(DMA_PRIVATE, device->cap_mask);
			device->privatecnt++;   //请求的通道数加 1
			err = dma_chan_get(chan);

            ...... 错误判断
		}
	}
	mutex_unlock(&dma_list_mutex);

	return chan;
}
EXPORT_SYMBOL_GPL(__dma_request_channel);



int dma_async_device_register(struct dma_device *device)
{

    /* 检查对应的能力是否有对应的回调函数 */
    if (dma_has_cap(DMA_MEMCPY, device->cap_mask) && !device->device_prep_dma_memcpy) {
    dev_err(device->dev,
        "Device claims capability %s, but op is not defined\n",
        "DMA_MEMCPY");
    return -EIO;
	}

    /* 检查几个必须存在的回调函数是否存在 
     * device_tx_status
     * device_issue_pending
     */

    /* 导出 sysfs，统计 dma_device_chancnt 的个数*/

}