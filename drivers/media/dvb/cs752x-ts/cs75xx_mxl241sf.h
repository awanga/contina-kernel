
#ifndef _CS75XX_MXL241SF_H
#define _CS75XX_MXL241SF_H

#include <linux/dvb/frontend.h>

struct mxl241sf_config {
	uint8_t	demod_address;

	uint32_t xtal_freq;
};

extern struct dvb_frontend *	mxl241sf_attach(const struct mxl241sf_config *,
						struct i2c_adapter *);

#endif /* !_CS75XX_MXL241SF_H */
