/*
 * FILE NAME cs75xx_trng.c
 *
 * BRIEF MODULE DESCRIPTION
 *  Driver for Cortina CS75XX TRNG device.
 *
 *  Copyright 2010 Cortina , Corp.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/hw_random.h>
#include <linux/platform_device.h>
#ifdef CONFIG_HW_RANDOM_CS75XX
#include <linux/jiffies.h>
#endif
#include <asm/io.h>
#include "elptrng.h"

#include <linux/bcd.h>
#include <linux/crypto.h>
#include <crypto/internal/rng.h>
#include <crypto/algapi.h>

#include <linux/err.h>
#include <linux/delay.h>

#ifdef CONFIG_CS75XX_TRNG_DIAG_ON
extern int cs75xx_trng_diag(uint32_t, uint32_t);
#endif

extern unsigned int system_rev;

struct cs75xx_spacc_rng_alg {
	struct device *dev;
	struct crypto_alg crypto;

} ;


struct cs75xx_trng {
	struct device   *dev;
	void __iomem    *base;
	int             irq;
	trng_hw         hw;
	struct cs75xx_spacc_rng_alg c_alg;

};

static struct platform_device *cs75xx_hwrng_dev;



#define CS75XX_MAX_SEED_SIZE TRNG_NONCE_SIZE_WORDS

struct cs75xx_spacc_rng_ctx {
	struct device *dev;
	u8 seed[CS75XX_MAX_SEED_SIZE];
	unsigned int  slen;
};


int cs75xx_spacc_rng_make_random(struct crypto_rng *tfm, u8 *rdata,
			       unsigned int dlen)
{
	struct crypto_alg *alg = crypto_rng_tfm(tfm)->__crt_alg;
	struct cs75xx_spacc_rng_alg *c_alg
		= container_of(alg, struct cs75xx_spacc_rng_alg,
						   crypto);


	struct cs75xx_trng *priv = dev_get_drvdata(c_alg->dev);

	return trng_rand(&priv->hw, (uint8_t *)rdata, dlen);

}

int cs75xx_spacc_rng_reset(struct crypto_rng *tfm, u8 *seed, unsigned int slen)
{
	struct crypto_alg *alg = crypto_rng_tfm(tfm)->__crt_alg;

	struct cs75xx_spacc_rng_alg *c_alg
		= container_of(alg, struct cs75xx_spacc_rng_alg,
						   crypto);

	struct cs75xx_trng *priv = dev_get_drvdata(c_alg->dev);

	if (slen !=0)
	{
		if (slen != CS75XX_MAX_SEED_SIZE)
		{
			printk ("Err[%d]:%s(seedlen %d != MAX_SEEDLEN %d) \n", __LINE__,__func__,slen,CS75XX_MAX_SEED_SIZE);
			return  -EINVAL;
		}
		trng_reseed_nonce(&priv->hw,seed);
	}

    return trng_init(&priv->hw, (uint32_t)priv->base, ELPTRNG_IRQ_PIN_DISABLE, ELPTRNG_NO_RESEED);

}

static void cs75xx_spacc_rng_alg_unregister(struct platform_device *pfdev)
{
	struct device *dev = &pfdev->dev;
	struct cs75xx_trng *priv = dev_get_drvdata(dev);
	struct cs75xx_spacc_rng_alg *c_alg = &(priv->c_alg);

	crypto_unregister_alg(&c_alg->crypto);
}


#define CS75XX_POSTFIX "-cs75xx"


static int cs75xx_spacc_rng_alg_register(struct platform_device *pfdev)
{
	struct device *dev = &pfdev->dev;
	struct cs75xx_trng *priv = dev_get_drvdata(dev);
	int i, err;

	struct cs75xx_spacc_rng_alg *c_alg = &(priv->c_alg);


	if (!c_alg)
		return ERR_PTR(-ENOMEM);


	c_alg->crypto.cra_rng.rng_make_random=cs75xx_spacc_rng_make_random;
	c_alg->crypto.cra_rng.rng_reset=cs75xx_spacc_rng_reset;
	//c_alg->crypto.cra_init = cs75xx_spacc_cra_init;

	c_alg->crypto.cra_flags = CRYPTO_ALG_TYPE_RNG | CRYPTO_ALG_ASYNC;
	c_alg->crypto.cra_type = &crypto_rng_type;
	c_alg->crypto.cra_ctxsize = sizeof(struct cs75xx_spacc_rng_ctx);
	c_alg->crypto.cra_alignmask = 0; //CS75XX_ALIGN_MASK;
	c_alg->crypto.cra_priority = 3000; //CS75XX_CRA_PRIORITY;
	snprintf(c_alg->crypto.cra_driver_name, CRYPTO_MAX_ALG_NAME,	"%s"CS75XX_POSTFIX, pfdev->name);
	c_alg->crypto.cra_module = THIS_MODULE;

	c_alg->dev = dev;
	char *name = NULL;

	err = crypto_register_alg(&c_alg->crypto);
	name = c_alg->crypto.cra_driver_name;
	if (err) {
		dev_err(dev, "%s alg registration failed\n", name);
		kfree(c_alg);
		return err;
	} else {
		//dev_info(dev, "%s\n", name);
	}

	return 0;

}


static int cs75xx_hwrng_data_present(struct hwrng *rng, int wait)
{
	struct cs75xx_trng *trng = platform_get_drvdata(cs75xx_hwrng_dev);
    return 1;
	/* 200us timeout ??? */
	//printk ("Bird [%d]:%s \n", __LINE__,__func__);

	/*if (trng_wait_for_done(&trng->hw, LOOP_WAIT) == ELPTRNG_OK)
		return 1;
	else
		return 0;*/
}


struct cs75xx_sha1 {
	uint32_t	hash[5];
	uint64_t	total64;
	uint8_t wbuffer[64];
};


#define rotl32(w, s) (((w) << (s)) | ((w) >> (32 - (s))))

static void sha1_process_block64(struct cs75xx_sha1 *ctx)
{
	unsigned t;
	uint32_t W[80], a, b, c, d, e;
	const uint32_t *words = (uint32_t*) ctx->wbuffer;

	for (t = 0; t < 16; ++t) {
		W[t] = ntohl(*words);
		words++;
	}

	for (/*t = 16*/; t < 80; ++t) {
		uint32_t T = W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16];
		W[t] = rotl32(T, 1);
	}

	a = ctx->hash[0];
	b = ctx->hash[1];
	c = ctx->hash[2];
	d = ctx->hash[3];
	e = ctx->hash[4];

/* Reverse byte order in 32-bit words   */
#define ch(x,y,z)        ((z) ^ ((x) & ((y) ^ (z))))
#define parity(x,y,z)    ((x) ^ (y) ^ (z))
#define maj(x,y,z)       (((x) & (y)) | ((z) & ((x) | (y))))
/* A normal version as set out in the FIPS. This version uses   */
/* partial loop unrolling and is optimised for the Pentium 4    */
#define rnd(f,k) \
	do { \
		uint32_t T = a; \
		a = rotl32(a, 5) + f(b, c, d) + e + k + W[t]; \
		e = d; \
		d = c; \
		c = rotl32(b, 30); \
		b = T; \
	} while (0)

	for (t = 0; t < 20; ++t)
		rnd(ch, 0x5a827999);

	for (/*t = 20*/; t < 40; ++t)
		rnd(parity, 0x6ed9eba1);

	for (/*t = 40*/; t < 60; ++t)
		rnd(maj, 0x8f1bbcdc);

	for (/*t = 60*/; t < 80; ++t)
		rnd(parity, 0xca62c1d6);
#undef ch
#undef parity
#undef maj
#undef rnd

	ctx->hash[0] += a;
	ctx->hash[1] += b;
	ctx->hash[2] += c;
	ctx->hash[3] += d;
	ctx->hash[4] += e;
}


void cs75xx_sha1(uint8_t * src_buf,char *res_buf, int len)
{
		int src_fd, hash_len, count;

		uint8_t *hash_value = NULL;
		//uint8_t *temp_buf = NULL;

		struct cs75xx_sha1 context_sha1;

		context_sha1.hash[0] = 0x67452301;
		context_sha1.hash[1] = 0xefcdab89;
		context_sha1.hash[2] = 0x98badcfe;
		context_sha1.hash[3] = 0x10325476;
		context_sha1.hash[4] = 0xc3d2e1f0;
		context_sha1.total64 = 0;

		hash_len = 20;

		/** to implement update(in_buf, count, &context);**/

		unsigned in_buf = context_sha1.total64 & 63;
		unsigned add = 64 - in_buf;

		context_sha1.total64 += len;

		while (len >= add) {	/* transfer whole blocks while possible  */
			memcpy(context_sha1.wbuffer + in_buf, src_buf, add);
			src_buf = (const char *)src_buf + add;
			len -= add;
			add = 64;
			in_buf = 0;
			sha1_process_block64(&context_sha1);
		}

		memcpy(context_sha1.wbuffer + in_buf, src_buf, len);

		/** to implement final(src_buf, &context);  */
		unsigned pad;

		in_buf = context_sha1.total64  & 63;
		/* Pad the buffer to the next 64-byte boundary with 0x80,0,0,0... */
		context_sha1.wbuffer[in_buf++] = 0x80;

		/* This loop iterates either once or twice, no more, no less */
		while (1) {
			pad = 64 - in_buf;
			memset(context_sha1.wbuffer + in_buf, 0, pad);
			in_buf = 0;
			/* Do we have enough space for the length count? */
			if (pad >= 8) {
				/* Store the 64-bit counter of bits in the buffer in BE format */
				uint64_t t = context_sha1.total64 << 3;
				/* t = hton64(t); _LITTLE_ENDIAN */
				t= (((uint64_t)htonl(t)) << 32) | htonl(t >> 32);
				/* wbuffer is suitably aligned for this */
				*(uint64_t *) (&context_sha1.wbuffer[64 - 8]) = t;
			}
			sha1_process_block64(&context_sha1);
			if (pad >= 8)
				break;
		}

		in_buf =  5;
		/* This way we do not impose alignment constraints on resbuf: */
		/*  (BB_LITTLE_ENDIAN) */
		unsigned i;
		for (i = 0; i < in_buf; ++i)
			context_sha1.hash[i] = htonl(context_sha1.hash[i]);

		memcpy(res_buf, context_sha1.hash, hash_len);

}

#define rtc_base_addr 0xF4920000

static void cs75xx_seed_gen(struct cs75xx_trng *trng)
{
		uint32_t * addr_RTC_RTCALM,* addr_RTC_ALMSEC,* addr_RTC_BCDSEC,* addr_RTC_RTCPEND;
		uint32_t * addr_arm_timer ,* addr_per_sys_timer1 ,* addr_per_sys_timer0;
		uint32_t  rand_num;
		uint32_t data_string[5];
		uint32_t hash_string[5];

		/*** get random value by RTC ***/
		/*0. initiliza address*/
		addr_RTC_RTCALM = rtc_base_addr + 0x04;
		addr_RTC_ALMSEC = rtc_base_addr + 0x08;
		addr_RTC_BCDSEC = rtc_base_addr + 0x24;
		addr_RTC_RTCPEND= rtc_base_addr + 0x44;
		addr_arm_timer = 0xf8000604;
		addr_per_sys_timer1 = 0xf007028c;
		addr_per_sys_timer0 = 0xf0070290;

		/*a. read the current RTC time*/
		*addr_RTC_RTCALM = 0x00; /* disable rtc alarm */
		*addr_RTC_RTCPEND = *addr_RTC_RTCPEND & 0xFE;

		/*b. Start the ARM timer - in SMP, it would start that automatically  */
		/*c. Set the RCT alarm for 2 sec*/
#if 0  /*orignal software workaround*/
		rand_num = bcd2bin (*addr_RTC_BCDSEC ) + 2;
		if (rand_num >= 60)
			rand_num =rand_num - 60;
		*addr_RTC_ALMSEC = bin2bcd( rand_num );
		*addr_RTC_RTCALM = 0x81; // enable rtc alarm
		while ((readl(addr_RTC_RTCPEND) & 0x01) ==0)
		{
		  mdelay(200);
		}
#else
		/*ARM timer[1..0] + PER_TMR_CNT2[1..0](@0xf0070044) */
		rand_num =readl(addr_arm_timer);
		rand_num = ((rand_num << 8) & 0xFF00) + (readl(0xf0070044) & 0xFF);
#endif


		/*f. Assemble a data string */
		uint32_t  data_timer0;
		data_timer0 =*(addr_per_sys_timer0);
		data_string[0] = *(addr_per_sys_timer1);
		data_string[1] = ((readl(rtc_base_addr + 0x28) & (0x7F) )<< 7) /* BCDMIN[6..0]*/ + (*(addr_RTC_BCDSEC) & 0x7F) /*BCDSEC[6..0]*/
						+((readl(rtc_base_addr + 0x2c) & (0x3F))<< 14)/*BCDHOUR[5..0]*/ + ((readl(rtc_base_addr + 0x30) & (0x3F))<< 20) /*BCDDATE[5..0]*/
						+((readl(rtc_base_addr + 0x34) & (0x07))<< 26) /*BCDDAY[2..0]*/ + ((readl(rtc_base_addr + 0x38) & (0x1F))<< 29) /*BCDMON[2..0]*/
						;
		data_string[2] = ((readl(rtc_base_addr + 0x38) & (0x1F))>> 3)/* BCDMON[4..3]  */+((readl(rtc_base_addr + 0x3c) & (0xFFFF))<< 2) /*BCDYEAR[15..0]*/
						+(data_timer0 << 18) /*PER_SYS_TIMER0[13..0] */
						;
		data_string[3] = (data_timer0 >>14) /*PER_SYS_TIMER0[31..14] */ + ((rand_num & 0xFFFF) << 18);
		data_string[4] = ((rand_num & 0xFFFF) >> 14);


		cs75xx_sha1(data_string,hash_string,17);
	    int i;
		for (i=1 ;i<4 ;i++)
		{
			data_string[i] = (data_string[i] >>3) + ((data_string[i+1] & 0x07) <<29);
		}
		printk ("rand_num, %04x, nonce_string ,%08x%08x%08x%08x%08x",rand_num, data_string[4], data_string[3],data_string[2],data_string[1]);
		printk ("%08x%08x%08x%08x%08x,", hash_string[4],hash_string[3],hash_string[2],hash_string[1],hash_string[0]);
		/*** feed the seed to DATA0-3 ***/
		uint32_t * addr;
		addr = trng->base;

		*addr = TRNG_NONCE_RESEED; //0x40000000;
		for (i=0 ;i<4 ;i++)
		{
			*(trng->hw.trng_data+i) = hash_string[i];
		}
		trng_dump_registers(&trng->hw);

		*addr = TRNG_NONCE_RESEED_LD | TRNG_NONCE_RESEED; //0x60000000;

		*(trng->hw.trng_data) = hash_string[4];
		for (i=1 ;i<4 ;i++)
		{
			*(trng->hw.trng_data+i) = data_string[i];
		}
		trng_dump_registers(&trng->hw);

		*addr = TRNG_NONCE_RESEED_LD | TRNG_NONCE_RESEED | TRNG_NONCE_RESEED_SELECT; //0x70000000;

		*addr = TRNG_BUSY; //0x00000001;

}

static int cs75xx_hwrng_data_read(struct hwrng *rng, u32 *data)
{
	struct cs75xx_trng *trng = platform_get_drvdata(cs75xx_hwrng_dev);

	trng_rand(&trng->hw, (uint8_t *)data, 16);

	return 16;
}

static int cs75xx_hwrng_init(struct hwrng *rng)
{
	struct cs75xx_trng *trng = platform_get_drvdata(cs75xx_hwrng_dev);

	int rc;	
		
	if ((system_rev == 0x7542a0) || (system_rev == 0x7522a0)) {
		printk("seed by sha1\n");
		rc = trng_init(&trng->hw, (uint32_t)trng->base, ELPTRNG_IRQ_PIN_DISABLE, ELPTRNG_NO_RESEED);

		/**Need to reseed by nonce manually for BUG 28891 ***/
		cs75xx_seed_gen(trng);
		trng_dump_registers(&trng->hw);
		rc = ELPTRNG_OK;
	} else {		
		rc = trng_init(&trng->hw, (uint32_t)trng->base, ELPTRNG_IRQ_PIN_DISABLE, ELPTRNG_RESEED);
	}

	trng->hw.initialized = ELPTRNG_INIT;
	return rc;
}

static void cs75xx_hwrng_cleanup(struct hwrng *rng)
{
	struct cs75xx_trng *trng = platform_get_drvdata(cs75xx_hwrng_dev);

	trng_close(&trng->hw);
}

static struct hwrng cs75xx_hwrng = {
	.name		= "g2_rng",
	.init		= cs75xx_hwrng_init,
	.cleanup	= cs75xx_hwrng_cleanup,
	.data_present	= cs75xx_hwrng_data_present,
	.data_read	= cs75xx_hwrng_data_read,
};

static int __devinit cs75xx_trng_probe(struct platform_device *pdev)
{
	struct cs75xx_trng *trng;
	struct resource *res_mem;
	int err;	
	/*Set GLOBAL_SCRATCH bit5 = 1*/
	uint32_t * addr;
	uint32_t global_scratch;
	addr = 0xf00000c0;
	global_scratch = readl(addr);
	*(addr) = global_scratch | 0x20;
	
	dev_info(&pdev->dev, "Function: %s, pdev->name = %s\n", __func__, pdev->name);

	trng = kzalloc(sizeof(struct cs75xx_trng), GFP_KERNEL);
	if (!trng) {
		dev_err(&pdev->dev, "\nFunc: %s - can't allocate memory for %s device\n", __func__, "trng");
		return -ENOMEM;
	}
	memset(trng, 0, sizeof(struct cs75xx_trng));
	trng->dev = &pdev->dev;

	res_mem = platform_get_resource_byname(pdev, IORESOURCE_IO, "trng");
	if (!res_mem) {
		dev_err(&pdev->dev, "Func: %s - can't get resource %s\n", __func__, "trng");
		goto fail;
	}

	trng->base = ioremap(res_mem->start, resource_size(res_mem));
	if (!trng->base) {
		dev_err(&pdev->dev, "Func: %s - unable to remap %s %d memory \n",
		        __func__, "trng", resource_size(res_mem));
		goto fail;
	}

	platform_set_drvdata(pdev, trng);
	cs75xx_hwrng_dev = pdev;

	err = hwrng_register(&cs75xx_hwrng);
	if (err) {
		dev_err(&pdev->dev, KERN_ERR "RNG registering failed (%d)\n", err);
		goto fail;
	}

#ifdef CONFIG_CS75XX_TRNG_DIAG_ON
	{
		printk(KERN_DEBUG "CS75XX TRNG self-test \n");
		ELPHW_WRITE_REG(trng->base, TRNG_NONCE_RESEED_LD | TRNG_NONCE_RESEED | TRNG_NONCE_RESEED_SELECT);
		ELPHW_WRITE_REG(trng->base, TRNG_BUSY);
		cs75xx_trng_diag((uint32_t)trng->base, (uint32_t) jiffies);
	}
#endif

	err = cs75xx_spacc_rng_alg_register(pdev);
	if (err) {
		dev_err(&pdev->dev, KERN_ERR "RNG ALG registering failed (%d)\n", err);
		goto fail;
	}

	return 0;

fail:
	if (trng->base)
		iounmap(trng->base);

	kfree(trng);

	return -ENODEV;
}

static int __devexit cs75xx_trng_remove(struct platform_device *pdev)
{
	struct cs75xx_trng *trng = platform_get_drvdata(pdev);

	cs75xx_spacc_rng_alg_unregister(pdev);

	hwrng_unregister(&cs75xx_hwrng);

	if (trng->base)
		iounmap(trng->base);

	kfree(trng);

	return 0;
}

static struct platform_driver cs75xx_trng_platform_driver = {
	.probe	= cs75xx_trng_probe,
	.remove	= __devexit_p(cs75xx_trng_remove),
	.driver	= {
		.owner = THIS_MODULE,
		.name  = "cs75xx_trng",
	},
};

static int __init cs75xx_trng_init(void)
{
	return platform_driver_register(&cs75xx_trng_platform_driver);
}

static void __exit cs75xx_trng_exit(void)
{
	platform_driver_unregister(&cs75xx_trng_platform_driver);
}

module_init(cs75xx_trng_init);
module_exit(cs75xx_trng_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cortina CS75XX TRNG driver");


