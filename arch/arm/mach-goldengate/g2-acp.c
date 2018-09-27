#include <mach/platform.h>
#include <mach/g2-acp.h>

static dma_addr_t acp_dma_map_page(struct device *dev, struct page *page,
	     unsigned long offset, size_t size, enum dma_data_direction dir,
	     struct dma_attrs *attrs)
{
	return (pfn_to_dma(dev, page_to_pfn(page)) + offset) |
		GOLDENGATE_ACP_BASE;
}

static void acp_dma_unmap_page(struct device *dev, dma_addr_t handle,
		size_t size, enum dma_data_direction dir,
		struct dma_attrs *attrs)
{
}

static void acp_dma_sync_single_for_cpu(struct device *dev,
		dma_addr_t handle, size_t size, enum dma_data_direction dir)
{
}

static void acp_dma_sync_single_for_device(struct device *dev,
		dma_addr_t handle, size_t size, enum dma_data_direction dir)
{
}

static int acp_dma_set_mask(struct device *dev, u64 dma_mask)
{
	if (!dev->dma_mask || !dma_supported(dev, dma_mask))
		return -EIO;

	*dev->dma_mask = dma_mask;

	return 0;
}

struct dma_map_ops acp_dma_ops = {
	.map_page		= acp_dma_map_page,
	.unmap_page		= acp_dma_unmap_page,
	.map_sg			= arm_dma_map_sg,
	.unmap_sg		= arm_dma_unmap_sg,
	.sync_single_for_cpu	= acp_dma_sync_single_for_cpu,
	.sync_single_for_device	= acp_dma_sync_single_for_device,
	.sync_sg_for_cpu	= arm_dma_sync_sg_for_cpu,
	.sync_sg_for_device	= arm_dma_sync_sg_for_device,
	.set_dma_mask		= acp_dma_set_mask,
};
