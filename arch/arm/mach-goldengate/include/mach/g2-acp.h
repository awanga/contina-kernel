#ifndef MACH_G2_ACP_H
#define MACH_G2_ACP_H

#include <linux/dma-mapping.h>

void goldengate_acp_update(void);
extern struct dma_map_ops acp_dma_ops;

#endif /* MACH_G2_ACP_H */
