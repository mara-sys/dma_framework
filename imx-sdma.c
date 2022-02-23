



static struct dma_async_tx_descriptor *sdma_prep_sg(
		struct dma_chan *chan,
		struct scatterlist *dst_sg, unsigned int dst_nents,
		struct scatterlist *src_sg, unsigned int src_nents,
		enum dma_transfer_direction direction, unsigned long flags)
{
	struct sdma_channel *sdmac = to_sdma_chan(chan);
	struct sdma_engine *sdma = sdmac->sdma;
	int ret, i, count;
	int channel = sdmac->channel;
	struct scatterlist *sg_src = src_sg, *sg_dst = dst_sg;
	struct sdma_desc *desc;

	if (!chan)
		return NULL;

	dev_dbg(sdma->dev, "setting up %d entries for channel %d.\n",
			src_nents, channel);

	desc = sdma_transfer_init(sdmac, direction, src_nents);
	if (!desc)
		goto err_out;

	for_each_sg(src_sg, sg_src, src_nents, i) {
		struct sdma_buffer_descriptor *bd = &desc->bd[i];
		int param;

		bd->buffer_addr = sg_src->dma_address;

		if (direction == DMA_MEM_TO_MEM) {
			BUG_ON(!sg_dst);
			bd->ext_buffer_addr = sg_dst->dma_address;
		}

		count = sg_dma_len(sg_src);

		if (count > SDMA_BD_MAX_CNT) {
			dev_err(sdma->dev, "SDMA channel %d: maximum bytes for sg entry exceeded: %d > %d\n",
					channel, count, SDMA_BD_MAX_CNT);
			ret = -EINVAL;
			goto err_bd_out;
		}

		bd->mode.count = count;
		desc->des_count += count;

		if (direction == DMA_MEM_TO_MEM)
			ret = check_bd_buswidth(bd, sdmac, count,
						sg_dst->dma_address,
						sg_src->dma_address);
		else
			ret = check_bd_buswidth(bd, sdmac, count, 0,
						sg_src->dma_address);
		if (ret)
			goto err_bd_out;

		param = BD_DONE | BD_EXTD | BD_CONT;

		if (i + 1 == src_nents) {
			param |= BD_INTR;
			param |= BD_LAST;
			param &= ~BD_CONT;
		}

		dev_dbg(sdma->dev, "entry %d: count: %d dma: 0x%pad %s%s\n",
				i, count, &sg_src->dma_address,
				param & BD_WRAP ? "wrap" : "",
				param & BD_INTR ? " intr" : "");

		bd->mode.status = param;
		if (direction == DMA_MEM_TO_MEM)
			sg_dst = sg_next(sg_dst);
	}

	sdmac->chn_count = desc->des_count;
	return vchan_tx_prep(&sdmac->vc, &desc->vd, flags);

err_bd_out:
	sdma_free_bd(desc);
	kfree(desc);
err_out:
	dev_dbg(sdma->dev, "Can't get desc.\n");
	return NULL;
}




static struct dma_async_tx_descriptor *sdma_prep_memcpy_sg(
		struct dma_chan *chan,
		struct scatterlist *dst_sg, unsigned int dst_nents,
		struct scatterlist *src_sg, unsigned int src_nents,
		unsigned long flags)
{
	return sdma_prep_sg(chan, dst_sg, dst_nents, src_sg, src_nents,
			   DMA_MEM_TO_MEM, flags);
}

static struct dma_async_tx_descriptor *sdma_prep_slave_sg(
		struct dma_chan *chan, struct scatterlist *sgl,
		unsigned int sg_len, enum dma_transfer_direction direction,
		unsigned long flags, void *context)
{
	return sdma_prep_sg(chan, NULL, 0, sgl, sg_len, direction, flags);
}