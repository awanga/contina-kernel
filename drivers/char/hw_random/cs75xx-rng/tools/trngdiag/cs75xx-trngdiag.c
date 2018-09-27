#include <linux/errno.h>
#include <linux/random.h>
#include <linux/jiffies.h>
#include "elptrng.h"        /* for trng interface */
#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>

extern unsigned int system_rev;
#define READBUFSIZE TRNG_DATA_SIZE_BYTES


void dump_bytes (uint8_t * buf, uint32_t len)
{
   uint32_t i;
   for (i = 0; i < len; i++) {
      printk (KERN_DEBUG "%02X", buf[i]);
   }
}

/* Useful function to get data from the trng with optional nonce seeding */
static int32_t prng_get_data (trng_hw * trng, uint8_t * sw_buffer, uint32_t buffersize, uint8_t do_nonce)
{
	uint8_t seed_buffer[TRNG_NONCE_SIZE_BYTES];
	int32_t res;

	if (do_nonce) {
		/* Set the nonce with system prng data */
		get_random_bytes(seed_buffer, TRNG_NONCE_SIZE_BYTES);
		printk(KERN_DEBUG "seed_value: \t");

		dump_bytes(seed_buffer, TRNG_NONCE_SIZE_BYTES);
		printk(KERN_DEBUG "\n");

		if ((res = trng_reseed_nonce (trng, (uint32_t *) seed_buffer)) != ELPTRNG_OK) {
			printk ("prng_get_data: Error: can't seed buffer '%d'..... FAILED\n", res);
			return res;
		}
	}

	memset (sw_buffer, 0, sizeof (sw_buffer));
	if ((res = trng_rand (trng, sw_buffer, buffersize)) != ELPTRNG_OK) {
		printk ("prng_get_data: Error: can't generate random number ..... FAILED [%d]\n", res);
		return res;
	}

   return ELPTRNG_OK;
}

struct file *openFile(char *path,int flag,int mode)
{
	struct file *fp;
	mm_segment_t oldfs;
	oldfs = get_fs();
	set_fs(get_ds());

	fp=filp_open(path, flag, mode);

	set_fs(oldfs);
	if (fp)
		return fp;
	else
		return NULL;
}

int readFile(struct file *fp,char *buf,int readlen)
{
	mm_segment_t oldfs = get_fs ();
	int ret = -1;

	set_fs(get_ds());

	if (fp->f_op && fp->f_op->read)
		ret= fp->f_op->read(fp,buf,readlen, &fp->f_pos);

	set_fs (oldfs);
	return ret;

}

int writeFile(struct file *fp,char *buf,int len)
{
	mm_segment_t oldfs = get_fs ();
	int ret = -1;

	set_fs(get_ds());

	if (fp->f_op && fp->f_op->write)
		ret= fp->f_op->write(fp,buf,len, &fp->f_pos);
	//ret = vfs_write(fp, buf, len, &offset);

	set_fs (oldfs);
	return ret;
}

int closeFile(struct file *fp)
{
	mm_segment_t oldfs = get_fs ();
	set_fs(get_ds());

	filp_close(fp,NULL);
	set_fs (oldfs);
	return 0;
}


/* Write buffers of rng data to a file 'rand.bin' */
static int32_t write_rng (trng_hw * trng, uint32_t bytes_per_buf, uint32_t iterations)
{
	unsigned char buf[bytes_per_buf];
	int i, j, res;
	int ret;
	struct file *fp;
	unsigned long long offset =0;

	fp=openFile("/rand.bin",O_CREAT | O_RDWR, 0644);
	printk(KERN_DEBUG "File pointer = %p\n", fp);
	for (i = 0; i < iterations; i++) {
		if ((res = trng_rand (trng, buf, sizeof (buf))) < 0) {
			printk ("rng_test: Error: can't generate random number ..... FAILED [%d]\n", res);
			return res;
		}
		
		if (fp!=NULL)
		{
			writeFile(fp,buf,bytes_per_buf);
		}
	}
	closeFile(fp);

   return ELPTRNG_OK;
}

static int32_t print_batch_rng (trng_hw * trng, uint32_t bytes_per_buf, uint32_t iterations)
{
	unsigned char buf[bytes_per_buf];
	int i, j, res;
	int ret;
	
	unsigned long long offset =0;
	
	for (i = 0; i < iterations; i++) {
		if ((res = trng_rand (trng, buf, sizeof (buf))) < 0) {
			printk ("rng_test: Error: can't generate random number ..... FAILED [%d]\n", res);
			return res;
		}

		for (j = 0; j < bytes_per_buf; j++) {
			printk(KERN_DEBUG "%02X", buf[j]);

			if (((j+1) % 4) == 0)
				printk(KERN_DEBUG "  ");
			if (((j+1) % 32) == 0)
				printk(KERN_DEBUG "\n");
		}
		
	}
	
   return ELPTRNG_OK;
}

int cs75xx_trng_diag(uint32_t base_addr, uint32_t seedval)
{
	int32_t ret;
	uint8_t buff[READBUFSIZE], buff2[READBUFSIZE];
	trng_hw trng;
	int32_t good_chip = 1;
	
	if ((system_rev == 0x7542a0) || (system_rev == 0x7522a0)) {
		good_chip = 0;
	}
	/* Initialize trng hardware */
	/* Don't enable IRQ pin and don't reseed */
	ret = trng_init (&trng, base_addr, ELPTRNG_IRQ_PIN_DISABLE, ELPTRNG_NO_RESEED);
	if (ret != ELPTRNG_OK) {
		printk ("%s: ERR[line %d]: trng_init: %d\n", __func__, __LINE__, ret);
		return ret;
	}
	/* Dump the registers to see if they make sense */
	trng_dump_registers (&trng);

	/* Initialize trng hardware */
	/* Enable IRQ pin and reseed */

	if (good_chip == 1) {	//  Hardware Reseed is not working
		ret = trng_init (&trng, base_addr, ELPTRNG_IRQ_PIN_ENABLE, ELPTRNG_RESEED);
		if (ret != ELPTRNG_OK) {
			printk ("ERR[line %d]: trng_init: %d\n", __LINE__, ret);
			return ret;
		}
		/* Dump the registers to see if they make sense */
		trng_dump_registers (&trng);
	}
	printk (KERN_DEBUG "trng_init: [PASSED]\n");
	printk (KERN_DEBUG "\n");

	/* Start the actual tests */

	/* Test 1: */
	/* Should be able to read random data */

	if ((ret = trng_rand (&trng, buff, READBUFSIZE)) != ELPTRNG_OK) {
		printk ("%s: Error[line %d]: can't generate random number ..... FAILED [%d]\n", __func__, __LINE__, ret);
		goto END;
	} else {
		printk (KERN_DEBUG "Rand data: ");
		dump_bytes (buff, READBUFSIZE);
		printk (KERN_DEBUG " [PASSED]\n");
	}
	printk (KERN_DEBUG "\n");

	/* Test 2: */
	/* Given different initial nonce seeds the rng data is different */

	printk (KERN_DEBUG "Unique Nonce Reseed test: \n");

	/* Get random data with a random nonce seed */
	srandom32 (jiffies);         /* Seed the system prng with a time stamp */
	ret = prng_get_data (&trng, buff2, READBUFSIZE, 1);
	if (ret != ELPTRNG_OK) {
		printk ("%s: ERR[line %d]: prng_get_data: %d\n", __func__, __LINE__, ret);
		goto END;
	}
	printk (KERN_DEBUG "Reseed 1: \t");
	dump_bytes (buff2, READBUFSIZE);
	printk (KERN_DEBUG "\n");

	/* Get random data with a random nonce seed */
	/* The system prng will create nonce with it's next unique data */
	ret = prng_get_data (&trng, buff, READBUFSIZE, 1);
	if (ret != ELPTRNG_OK) {
		printk ("%s: ERR[line %d]: prng_get_data: %d\n", __func__, __LINE__, ret);
		goto END;
	}
	printk (KERN_DEBUG "Reseed 2: \t");
	dump_bytes (buff, READBUFSIZE);

	/* Compare the data buffers */
	if (memcmp (buff, buff2, READBUFSIZE) != 0) {
		printk (KERN_DEBUG " [PASSED]\n");
	} else {
		printk (KERN_DEBUG " [FAILED]\n");
		ret = -1;
		goto END;
	}
	printk (KERN_DEBUG "\n");

	/* Test 3: */
	/* Given the same initial nonce seed the rng data is the same */

	printk (KERN_DEBUG "Same Nonce Reseed test:\n");

	//srandom32 (seedval);
	uint8_t seed_buffer[TRNG_NONCE_SIZE_BYTES];
	get_random_bytes(seed_buffer, TRNG_NONCE_SIZE_BYTES);
	printk(KERN_DEBUG "seed_value: \t");
	dump_bytes(seed_buffer, TRNG_NONCE_SIZE_BYTES);
	printk(KERN_DEBUG "\n");

	trng_reseed_nonce (&trng, (uint32_t *) seed_buffer);

	ret = prng_get_data (&trng, buff, READBUFSIZE, 0);
	if (ret != ELPTRNG_OK) {
		printk ("%s: ERR[line %d]: prng_get_data: %d\n", __func__, __LINE__, ret);
		goto END;
	}
	printk (KERN_DEBUG "Reseed 1: \t");
	dump_bytes (buff, READBUFSIZE);
	printk (KERN_DEBUG "\n");

	//srandom32 (seedval);
	trng_reseed_nonce (&trng, (uint32_t *) seed_buffer);
	ret = prng_get_data (&trng, buff2, READBUFSIZE, 0);
	if (ret != ELPTRNG_OK) {
		printk ("%s: ERR[line %d]: prng_get_data: %d\n", __func__, __LINE__, ret);
		goto END;
	}
	printk (KERN_DEBUG "Reseed 2: \t");
	dump_bytes (buff2, READBUFSIZE);

	if (memcmp (buff, buff2, READBUFSIZE) == 0) {
		printk (KERN_DEBUG " [PASSED]\n");
	} else {
		printk (KERN_DEBUG " [FAILED]\n");
		ret = -1;
		goto END;
	}
	printk (KERN_DEBUG "\n");

#if 0
	if (good_chip == 1) {	
		/* Test 4: */
		/* Reset the hardware and test to see if a nonce reseed is correct */

		printk ("Nonce Reseed while initial seed is underway:\n");
		trng_reseed_nonce (&trng, (uint32_t *) seed_buffer);
		ret = prng_get_data (&trng, buff2, READBUFSIZE, 0);
		if (ret != ELPTRNG_OK) {
			printk ("%s: ERR[line %d]: prng_get_data: %d\n", __func__, __LINE__, ret);
			goto END;
		}
		printk ("Run 1: \t\t");
		dump_bytes (buff, READBUFSIZE);
		printk ("\n");

		/* reset hardware which should kick off hw reseed */
		trng_reset (&tif);
		
		trng_reseed_nonce (&trng, (uint32_t *) seed_buffer);
		ret = prng_get_data (&trng, buff, READBUFSIZE, 0);
		if (ret != ELPTRNG_OK) {
			printk ("%s: ERR[line %d]: prng_get_data: %d\n", __func__, __LINE__, ret);
			goto END;
		}
		printk ("Run 2: \t\t");
		dump_bytes (buff, READBUFSIZE);

		if (memcmp (buff, buff2, READBUFSIZE) == 0) {
			printk (" [PASSED]\n ");
		} else {
			printk (" [FAILED]\n");
			ret = -1;
			goto END;
		}
		printk ("\n");
	}
#endif
	/* Test 5: */
	/* Force a nonce reseed while a random reseed is processing */
	if (good_chip == 1) {	//  Hardware Reseed is not working

		printk (KERN_DEBUG "Nonce Reseed while random reseed is underway:\n");

		trng_reseed_nonce (&trng, (uint32_t *) seed_buffer);
		ret = prng_get_data (&trng, buff2, READBUFSIZE, 0);
		if (ret != ELPTRNG_OK) {
			printk (KERN_DEBUG "%s: ERR[line %d]: prng_get_data_nonce: %d\n", __func__, __LINE__, ret);
		}
		printk (KERN_DEBUG "Run 1: \t\t");
		dump_bytes (buff2, READBUFSIZE);
		printk (KERN_DEBUG "\n");

		/* start a random reseed operation without waiting */
		if ((ret = trng_reseed_random (&trng, ELPTRNG_NO_WAIT)) != ELPTRNG_OK) {
			printk ("trng_reseed_random: failed %d\n", ret);
			goto END;
		}

		trng_reseed_nonce (&trng, (uint32_t *) seed_buffer);
		ret = prng_get_data (&trng, buff, READBUFSIZE, 0);
		if (ret != ELPTRNG_OK) {
      		printk ("%s: ERR[line %d]: prng_get_data: %d\n", __func__, __LINE__, ret);
		}

		printk (KERN_DEBUG "Run 2: \t\t");
		dump_bytes (buff, READBUFSIZE);
		/* previous buffer should be the same as this one */
		if (memcmp (buff, buff2, READBUFSIZE) == 0) {
			printk (KERN_DEBUG " [PASSED]\n");
		} else {
			printk (KERN_DEBUG " [FAILED]\n");
			ret = -1;
			goto END;
		}
		printk (KERN_DEBUG "\n");
	}
	
	/* Test 6: */
	/* Generate a large amount of random data */

	ret = print_batch_rng (&trng, 1024, 2);
	if (ret != ELPTRNG_OK) {
		printk ("%s: ERR[line %d]: write_rng: %d\n", __func__, __LINE__, ret);
		goto END;
	}
	printk (KERN_DEBUG "Large generate test: [PASSED]\n");

END:	
	
	trng_close (&trng);	
	return ret;
}



