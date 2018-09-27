/* include this file if the platform implements the dma_ DMA Mapping API
 * and wants to provide the pci_ DMA Mapping API in terms of it */

#ifndef _ASM_GENERIC_PCI_DMA_COMPAT_H
#define _ASM_GENERIC_PCI_DMA_COMPAT_H

#include <linux/dma-mapping.h>

static inline int
pci_dma_supported(struct pci_dev *hwdev, u64 mask)
{
	return dma_supported(hwdev == NULL ? NULL : &hwdev->dev, mask);
}

static inline void *
pci_alloc_consistent(struct pci_dev *hwdev, size_t size,
		     dma_addr_t *dma_handle)
{
	return dma_alloc_coherent(hwdev == NULL ? NULL : &hwdev->dev, size, dma_handle, GFP_ATOMIC);
}

static inline void
pci_free_consistent(struct pci_dev *hwdev, size_t size,
		    void *vaddr, dma_addr_t dma_handle)
{
	dma_free_coherent(hwdev == NULL ? NULL : &hwdev->dev, size, vaddr, dma_handle);
}

static inline dma_addr_t
pci_map_single(struct pci_dev *hwdev, void *ptr, size_t size, int direction)
{
	/* debug_Aaron on 2013/02/25 added for PCIe ACP support */
#ifdef CONFIG_CS752X_PROC
	extern unsigned int cs_acp_enable;
	if ((cs_acp_enable & CS75XX_ACP_ENABLE_PCI_RX && direction == PCI_DMA_FROMDEVICE) ||
		(cs_acp_enable & CS75XX_ACP_ENABLE_PCI_TX && direction == PCI_DMA_TODEVICE)) {
		/* Sanity check. G2 support DDR size up to 1GB */
		if(virt_to_phys(ptr) > PHYS_OFFSET && (virt_to_phys(ptr) < PHYS_OFFSET + SZ_1G))
			return virt_to_phys(ptr)| GOLDENGATE_ACP_BASE;
		else
			return dma_map_single(hwdev == NULL ? NULL : &hwdev->dev, ptr, size, (enum dma_data_direction)direction);
	}
#endif
	return dma_map_single(hwdev == NULL ? NULL : &hwdev->dev, ptr, size, (enum dma_data_direction)direction);
}

static inline void
pci_unmap_single(struct pci_dev *hwdev, dma_addr_t dma_addr,
		 size_t size, int direction)
{
#ifdef CONFIG_CS752X_PROC
	extern unsigned int cs_acp_enable;
	if (dma_addr & GOLDENGATE_ACP_BASE) 
	{
		if ((cs_acp_enable & CS75XX_ACP_ENABLE_PCI_RX && direction == PCI_DMA_FROMDEVICE) ||
			(cs_acp_enable & CS75XX_ACP_ENABLE_PCI_TX && direction == PCI_DMA_TODEVICE)) 
			return;
		else
			dma_addr &= ~GOLDENGATE_ACP_BASE;
	}
#endif
	dma_unmap_single(hwdev == NULL ? NULL : &hwdev->dev, dma_addr, size, (enum dma_data_direction)direction);
}

static inline dma_addr_t
pci_map_page(struct pci_dev *hwdev, struct page *page,
	     unsigned long offset, size_t size, int direction)
{
#ifdef CONFIG_CS752X_PROC
	extern unsigned int cs_acp_enable;
	if ((cs_acp_enable & CS75XX_ACP_ENABLE_PCI_RX && direction == PCI_DMA_FROMDEVICE) ||
		(cs_acp_enable & CS75XX_ACP_ENABLE_PCI_TX && direction == PCI_DMA_TODEVICE)) {
		/* Sanity check. G2 support DDR size up to 1GB */
		if(page_to_phys(page) > PHYS_OFFSET && (page_to_phys(page) < PHYS_OFFSET + SZ_1G))
			return page_to_phys(page)| GOLDENGATE_ACP_BASE;
		else
			return dma_map_page(hwdev == NULL ? NULL : &hwdev->dev, page, offset, size,\
				 (enum dma_data_direction)direction);
	}
#endif
	return dma_map_page(hwdev == NULL ? NULL : &hwdev->dev, page, offset, size, (enum dma_data_direction)direction);
}

static inline void
pci_unmap_page(struct pci_dev *hwdev, dma_addr_t dma_address,
	       size_t size, int direction)
{
#ifdef CONFIG_CS752X_PROC
	extern unsigned int cs_acp_enable;
	if (dma_address & GOLDENGATE_ACP_BASE) {
		if (cs_acp_enable & (CS75XX_ACP_ENABLE_PCI_RX | CS75XX_ACP_ENABLE_PCI_TX))
			return;
		else
			dma_address &= ~GOLDENGATE_ACP_BASE;
	}
#endif
	dma_unmap_page(hwdev == NULL ? NULL : &hwdev->dev, dma_address, size, (enum dma_data_direction)direction);
}

static inline int
pci_map_sg(struct pci_dev *hwdev, struct scatterlist *sg,
	   int nents, int direction)
{
	return dma_map_sg(hwdev == NULL ? NULL : &hwdev->dev, sg, nents, (enum dma_data_direction)direction);
}

static inline void
pci_unmap_sg(struct pci_dev *hwdev, struct scatterlist *sg,
	     int nents, int direction)
{
	dma_unmap_sg(hwdev == NULL ? NULL : &hwdev->dev, sg, nents, (enum dma_data_direction)direction);
}

static inline void
pci_dma_sync_single_for_cpu(struct pci_dev *hwdev, dma_addr_t dma_handle,
		    size_t size, int direction)
{
#ifdef CONFIG_CS752X_PROC
	extern unsigned int cs_acp_enable;
	if (dma_handle & GOLDENGATE_ACP_BASE) {
		if ((cs_acp_enable & CS75XX_ACP_ENABLE_PCI_RX && direction == PCI_DMA_FROMDEVICE) ||
			(cs_acp_enable & CS75XX_ACP_ENABLE_PCI_TX && direction == PCI_DMA_TODEVICE))
			return;
		else
			dma_handle &= ~GOLDENGATE_ACP_BASE;
	}
#endif
	dma_sync_single_for_cpu(hwdev == NULL ? NULL : &hwdev->dev, dma_handle, size, (enum dma_data_direction)direction);
}

static inline void
pci_dma_sync_single_for_device(struct pci_dev *hwdev, dma_addr_t dma_handle,
		    size_t size, int direction)
{
#ifdef CONFIG_CS752X_PROC
	extern unsigned int cs_acp_enable;
	if (dma_handle & GOLDENGATE_ACP_BASE) {
		if ((cs_acp_enable & CS75XX_ACP_ENABLE_PCI_RX && direction == PCI_DMA_FROMDEVICE) ||
			(cs_acp_enable & CS75XX_ACP_ENABLE_PCI_TX && direction == PCI_DMA_TODEVICE))
			return;
		else
			dma_handle &= ~GOLDENGATE_ACP_BASE;
	}
#endif
	dma_sync_single_for_device(hwdev == NULL ? NULL : &hwdev->dev, dma_handle, size, (enum dma_data_direction)direction);
}

static inline void
pci_dma_sync_sg_for_cpu(struct pci_dev *hwdev, struct scatterlist *sg,
		int nelems, int direction)
{
	dma_sync_sg_for_cpu(hwdev == NULL ? NULL : &hwdev->dev, sg, nelems, (enum dma_data_direction)direction);
}

static inline void
pci_dma_sync_sg_for_device(struct pci_dev *hwdev, struct scatterlist *sg,
		int nelems, int direction)
{
	dma_sync_sg_for_device(hwdev == NULL ? NULL : &hwdev->dev, sg, nelems, (enum dma_data_direction)direction);
}

static inline int
pci_dma_mapping_error(struct pci_dev *pdev, dma_addr_t dma_addr)
{
	return dma_mapping_error(&pdev->dev, dma_addr);
}

#ifdef CONFIG_PCI
static inline int pci_set_dma_mask(struct pci_dev *dev, u64 mask)
{
	return dma_set_mask(&dev->dev, mask);
}

static inline int pci_set_consistent_dma_mask(struct pci_dev *dev, u64 mask)
{
	return dma_set_coherent_mask(&dev->dev, mask);
}
#endif

#endif
