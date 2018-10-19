#ifndef __CS_MUT_H__
#define __CS_MUT_H__

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/rbtree.h>

/***** kmalloc family *****/

void *cs_malloc(size_t size, gfp_t flags);
void cs_free(void *ptr);

#define cs_zalloc(size, flags) cs_malloc(size, (flags)|__GFP_ZERO)

/***** initialization *****/

int cs_kmem_cache_create(void);

#endif /* __CS_MUT_H__ */
