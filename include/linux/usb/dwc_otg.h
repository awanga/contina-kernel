/**
 * =======================================================================
 * Add for Synopsys function for porting OK.  
 * =======================================================================        
 **/
 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/ioport.h>

struct lm_device {
	struct device		dev;
	struct resource		resource;
	unsigned int		irq;
	unsigned int		id;
	void			*lm_drvdata;
};

struct lm_driver {
	struct device_driver	driver;
	int			(*probe)(struct lm_device *);
	void			(*remove)(struct lm_device *);
//	int			(*suspend)(struct lm_device *, u32);
//	int			(*resume)(struct lm_device *);
};


//int lm_device_register(struct lm_device *dev);

#define lm_set_drvdata(lm,d)	do { (lm)->lm_drvdata = (d); } while (0)


#define to_lm_device(d)	container_of(d, struct lm_device, dev)
#define to_lm_driver(d)	container_of(d, struct lm_driver, driver)





