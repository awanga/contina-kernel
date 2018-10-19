/*
 * cs_mut.c: Memory Usage Tracing for Cortina NE driver
 *
 */
#include <linux/version.h>
#include <linux/types.h>
#include "cs_mut.h"

struct kmem_cache *cache_16 = NULL;	/* cache for 16-byte objects */

/*************************** kmalloc family ***************************/

void *cs_malloc(size_t size, gfp_t flags)
{
#ifdef CONFIG_CS75XX_SMALL_SLAB
	if ((size <= 16) && (cache_16 != NULL))
		return kmem_cache_alloc(cache_16, flags);
	else
		return kmalloc(size, flags);
#else
	return kmalloc(size, flags);
#endif
}

void cs_free(void *ptr)
{
	kfree(ptr);
}

/*************************** initialization ***************************/

int cs_kmem_cache_create(void)
{
#ifdef CONFIG_CS75XX_SMALL_SLAB
	cache_16 = kmem_cache_create("cs75xx_cache_16", 16, 0, SLAB_PANIC, NULL);
	if (cache_16 == NULL) {
		printk(KERN_INFO "ERROR! Fail to create cache for 16-byte objects!\n");
		return -1;
	}
#endif

	return 0;
}

