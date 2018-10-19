/*
 *  ram_panic.c
 *
 *  Copyright (C) 2018 Greenwave Systems
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#include <linux/pstore.h>

#define RAM_PANIC_MAGIC 0x56768997

/* we have a issue with the bit's decaying while uboot is doing
 * it's mem init, so we need to take that into account...
 * we store the len in 2 versions, and we check the characters
 * for 'invalid' values
 */

struct ram_panic {
	unsigned long magic;
	unsigned len;
	unsigned not_len;
	char text[];
};

unsigned ram_panic_pages = 3; /* one page can just contain a simple panic dump, so up that */
void *ram_panic_mem;

#define ram_panic_save ((struct ram_panic *) ram_panic_mem)

#define RAM_PANIC_TEXT_SIZE ((ram_panic_pages << PAGE_SHIFT) - sizeof(struct ram_panic))

static char ram_panic_fix_char(char c)
{
	if (c >= 0x7f) c = '?';
	else if (c < ' ' && c != '\n') c = '?';
	return c;
}

static void ram_panic_memcpy(char *d, const char *s, unsigned len)
{
	while (len-- > 0) *d++ = ram_panic_fix_char(*s++);
}

/* capture directly from kmsg_dump - alternative is pstore interface... */

/* so is older text in circular buffer, sn is newer */
static void ram_panic_dump(struct kmsg_dumper *dumper, enum kmsg_dump_reason r)
{
	unsigned long s = RAM_PANIC_TEXT_SIZE - 1;
	char *dst = ram_panic_save->text;
	if (!ram_panic_save) return;
	if (r != KMSG_DUMP_OOPS && r != KMSG_DUMP_PANIC) return;
#if 0
	{
		unsigned h = scnprintf(dst, s,
			"dump so %p lo %lu sn %p ln %lu size %lu\n", so, lo, sn, ln, lo+ln);
		dst += h;
		s -= h;
	}
	n = min(ln, s);
	if (n < ln) sn += ln - n; /* tail part */
	s -= n;
	o = min(lo, s);
	if (o < lo) so += lo - o; /* add tail part if room */
	ram_panic_memcpy(dst, so, o);
	dst += o;
	ram_panic_memcpy(dst, sn, n);
	dst += n;
	*dst = '\0';
#endif

	kmsg_dump_get_buffer(dumper, true, dst, s, NULL);

	ram_panic_save->len = dst - ram_panic_save->text;
	ram_panic_save->not_len = ~ram_panic_save->len;
	ram_panic_save->magic = RAM_PANIC_MAGIC;
	dma_map_single(NULL, ram_panic_save, ram_panic_save->len+1, DMA_TO_DEVICE);
	pr_emerg("ram panic: message saved %u bytes\n", ram_panic_save->len);
}
static struct kmsg_dumper ram_panic_dumper = {
	dump : ram_panic_dump,
};

static int __init ram_panic_has_panic(void)
{
	if (!ram_panic_save) return 0;
	if (ram_panic_save->magic != RAM_PANIC_MAGIC) {
		pr_emerg("ram panic: bad magic %lx at addr %p\n",
			ram_panic_save->magic, ram_panic_save);
	} else if (ram_panic_save->len != ~ram_panic_save->not_len) {
		pr_emerg("ram panic: bad lengths 0x%x vs ~ 0x%x\n",
			ram_panic_save->len, ram_panic_save->not_len);
	} else if (ram_panic_save->len >= RAM_PANIC_TEXT_SIZE) {
		pr_emerg("ram panic: bad len %d max %d\n",
			ram_panic_save->len, RAM_PANIC_TEXT_SIZE);
	} else if (ram_panic_save->len) {
		/* fixup any 'decayed' chars */
		char *sp = ram_panic_save->text;
		char *ep = sp + ram_panic_save->len;
		char *cp;
		char *op = NULL;
		pr_emerg("ram panic: message present len %d\n", ram_panic_save->len);
		for (cp = sp; cp < ep; ++cp) {
			char c = ram_panic_fix_char(*cp);
			if (c == *cp) continue;
			if (!op) op = cp;
			*cp = c;
		}
		if (op) {
			pr_emerg("ram panic: data changed offset %d\n", op - sp);
		}
		/* ensure terminating NULL still there */
		*ep = '\0';
		return 1;
	}
	ram_panic_save->magic = RAM_PANIC_MAGIC;
	ram_panic_save->len = 0;
	ram_panic_save->not_len = ~ram_panic_save->len;
	return 0;
}

/* 'readers', either proc fs or re-printk */

#if CONFIG_PROC_FS
/* make a PROC reader */

static unsigned ram_panic_len;

static int p_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int p_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	int err;
	size_t l;
	if (*ppos >= ram_panic_len) return 0;
	l = min(count, (size_t)(ram_panic_len - *ppos));
	err = copy_to_user(buf, ram_panic_save->text + *ppos, l);
	if (err) return -EFAULT;
	*ppos += l;
	return (int) l;
}

static int p_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations p_op = {
	.read    = p_read,
	.open    = p_open,
	.release = p_release,
};

static int __init ram_panic_init_show(void)
{
	if (ram_panic_has_panic()) {
		ram_panic_len = ram_panic_save->len;
		proc_create("last-panic-msg", S_IRUSR, NULL, &p_op);
	}
	return 0;
}

#else /* !CONFIG_PROC_FS */
/* re-print it so it ends up in /var/log/message */

static char * __init ram_panic_eol(char *cp, char *ep)
{
	while (cp < ep && *cp && *cp != '\n') ++cp;
	return cp;
}

static int __init ram_panic_init_show(void)
{
	if (ram_panic_has_panic()) {
		char *sp = ram_panic_save->text;
		char *ep = sp + ram_panic_save->len;
		char *cp;
		pr_emerg("ram panic: reboot from panic/oops:\n");
		for (; (cp = ram_panic_eol(sp, ep)) < ep; sp = cp+1) {
			int last = *cp == '\0';
			if (*cp == '\n') *cp = '\0';
			if (sp + 3 < cp) { /* strip printk code */
				if (sp[0] == '<' && sp[2] == '>') sp += 3;
			}
			if (sp < cp)
				pr_emerg("ram panic: %s\n", sp);
			if (last) break;
		}
		pr_emerg("ram panic: dump ends - %u bytes\n", cp - ram_panic_save->text);
	}
	return 0;
}
#endif

static int __init ram_panic_init(void)
{
	if (ram_panic_save) {
		ram_panic_init_show();
		/* zap it for next reboot */
		ram_panic_save->len = 0;
		ram_panic_save->not_len = ~ram_panic_save->len;
		kmsg_dump_register(&ram_panic_dumper);
	}
	return 0;
}
late_initcall(ram_panic_init);

