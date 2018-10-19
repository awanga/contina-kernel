/*
 * Driver for USB Mass Storage compliant devices
 *
 * Current development and maintenance by:
 *   (c) 1999-2002 Matthew Dharm (mdharm-usb@one-eyed-alien.net)
 *
 * Developed with the assistance of:
 *   (c) 2000 David L. Brown, Jr. (usb-storage@davidb.org)
 *   (c) 2002 Alan Stern (stern@rowland.org)
 *
 * Initial work by:
 *   (c) 1999 Michael Gee (michael@linuxspecific.com)
 *
 * This driver is based on the 'USB Mass Storage Class' document. This
 * describes in detail the protocol used to communicate with such
 * devices.  Clearly, the designers had SCSI and ATAPI commands in
 * mind when they created this document.  The commands are all very
 * similar to commands in the SCSI-II and ATAPI specifications.
 *
 * It is important to note that in a number of cases this class
 * exhibits class-specific exemptions from the USB specification.
 * Notably the usage of NAK, STALL and ACK differs from the norm, in
 * that they are used to communicate wait, failed and OK on commands.
 *
 * Also, for certain devices, the interrupt endpoint is used to convey
 * status of a command.
 *
 * Please see http://www.one-eyed-alien.net/~mdharm/linux-usb for more
 * information about this driver.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/highmem.h>
#include <linux/export.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>

#include "usb.h"
#include "protocol.h"
#include "debug.h"
#include "scsiglue.h"
#include "transport.h"

#ifdef CONFIG_ARCH_GOLDENGATE
#include <mach/gpio_alloc.h>
#endif /* CONFIG_ARCH_GOLDENGATE */

/***********************************************************************
 * Protocol routines
 ***********************************************************************/

void usb_stor_pad12_command(struct scsi_cmnd *srb, struct us_data *us)
{
	/*
	 * Pad the SCSI command with zeros out to 12 bytes.  If the
	 * command already is 12 bytes or longer, leave it alone.
	 *
	 * NOTE: This only works because a scsi_cmnd struct field contains
	 * a unsigned char cmnd[16], so we know we have storage available
	 */
	for (; srb->cmd_len < 12; srb->cmd_len++)
		srb->cmnd[srb->cmd_len] = 0;

	/* send the command to the transport layer */
	usb_stor_invoke_transport(srb, us);
}

void usb_stor_ufi_command(struct scsi_cmnd *srb, struct us_data *us)
{
	/*
	 * fix some commands -- this is a form of mode translation
	 * UFI devices only accept 12 byte long commands
	 *
	 * NOTE: This only works because a scsi_cmnd struct field contains
	 * a unsigned char cmnd[16], so we know we have storage available
	 */

	/* Pad the ATAPI command with zeros */
	for (; srb->cmd_len < 12; srb->cmd_len++)
		srb->cmnd[srb->cmd_len] = 0;

	/* set command length to 12 bytes (this affects the transport layer) */
	srb->cmd_len = 12;

	/* XXX We should be constantly re-evaluating the need for these */

	/* determine the correct data length for these commands */
	switch (srb->cmnd[0]) {

		/* for INQUIRY, UFI devices only ever return 36 bytes */
	case INQUIRY:
		srb->cmnd[4] = 36;
		break;

		/* again, for MODE_SENSE_10, we get the minimum (8) */
	case MODE_SENSE_10:
		srb->cmnd[7] = 0;
		srb->cmnd[8] = 8;
		break;

		/* for REQUEST_SENSE, UFI devices only ever return 18 bytes */
	case REQUEST_SENSE:
		srb->cmnd[4] = 18;
		break;
	} /* end switch on cmnd[0] */

	/* send the command to the transport layer */
	usb_stor_invoke_transport(srb, us);
}

#if defined(GPIO_USB_STORAGE_LED_0) || defined(GPIO_USB_STORAGE_LED_1)
extern spinlock_t usbled_lock;
extern unsigned long usbled_flags;

extern int usbled_id[2];
extern int usbled_start[2];
extern int usbled_status[2];
extern int usbled_count[2];
extern int usbled_idle_count[2];
#endif

void usb_stor_transparent_scsi_command(struct scsi_cmnd *srb,
				       struct us_data *us)
{
	/* send the command to the transport layer */
	usb_stor_invoke_transport(srb, us);

#if defined(GPIO_USB_STORAGE_LED_0) || defined(GPIO_USB_STORAGE_LED_1)
{
	int usb_id;

	usb_id = -1;

#ifdef GPIO_USB_STORAGE_LED_0
	if (!strncmp(us->pusb_dev->devpath, "1.", strlen("1.")) ||
	    (!strncmp(us->pusb_dev->devpath, "1", strlen("1")) && (us->pusb_dev->portnum == 1))) {
		usb_id = 0;
	}
#endif

#ifdef GPIO_USB_STORAGE_LED_1
	if (!strncmp(us->pusb_dev->devpath, "2.", strlen("2.")) ||
	    (!strncmp(us->pusb_dev->devpath, "2", strlen("2")) && (us->pusb_dev->portnum == 2))) {
		usb_id = 1;
	}
#endif

	if (usb_id != -1) {
		spin_lock(&usbled_lock);
		/*printk("%s:%d,%d,%d\n", __FUNCTION__, srb->cmd_len, usbled_count[usb_id], srb->transfersize);*/
		if (usbled_start[usb_id]) {
			usbled_count[usb_id] += srb->transfersize;
			usbled_idle_count[usb_id]++;

			// toggle detect
			if (usbled_count[usb_id] > 4*1024) {
				usbled_status[usb_id] ^= 1;
				gpio_set_value(usbled_id[usb_id], usbled_status[usb_id]);

				usbled_count[usb_id] = 0;
			}
		}

		spin_unlock(&usbled_lock);
	}
}
#endif
}
EXPORT_SYMBOL_GPL(usb_stor_transparent_scsi_command);

/***********************************************************************
 * Scatter-gather transfer buffer access routines
 ***********************************************************************/

/*
 * Copy a buffer of length buflen to/from the srb's transfer buffer.
 * Update the **sgptr and *offset variables so that the next copy will
 * pick up from where this one left off.
 */
unsigned int usb_stor_access_xfer_buf(unsigned char *buffer,
	unsigned int buflen, struct scsi_cmnd *srb, struct scatterlist **sgptr,
	unsigned int *offset, enum xfer_buf_dir dir)
{
	unsigned int cnt = 0;
	struct scatterlist *sg = *sgptr;
	struct sg_mapping_iter miter;
	unsigned int nents = scsi_sg_count(srb);

	if (sg)
		nents = sg_nents(sg);
	else
		sg = scsi_sglist(srb);

	sg_miter_start(&miter, sg, nents, dir == FROM_XFER_BUF ?
		SG_MITER_FROM_SG: SG_MITER_TO_SG);

	if (!sg_miter_skip(&miter, *offset))
		return cnt;

	while (sg_miter_next(&miter) && cnt < buflen) {
		unsigned int len = min_t(unsigned int, miter.length,
				buflen - cnt);

		if (dir == FROM_XFER_BUF)
			memcpy(buffer + cnt, miter.addr, len);
		else
			memcpy(miter.addr, buffer + cnt, len);

		if (*offset + len < miter.piter.sg->length) {
			*offset += len;
			*sgptr = miter.piter.sg;
		} else {
			*offset = 0;
			*sgptr = sg_next(miter.piter.sg);
		}
		cnt += len;
	}
	sg_miter_stop(&miter);

	return cnt;
}
EXPORT_SYMBOL_GPL(usb_stor_access_xfer_buf);

/*
 * Store the contents of buffer into srb's transfer buffer and set the
 * SCSI residue.
 */
void usb_stor_set_xfer_buf(unsigned char *buffer,
	unsigned int buflen, struct scsi_cmnd *srb)
{
	unsigned int offset = 0;
	struct scatterlist *sg = NULL;

	buflen = min(buflen, scsi_bufflen(srb));
	buflen = usb_stor_access_xfer_buf(buffer, buflen, srb, &sg, &offset,
			TO_XFER_BUF);
	if (buflen < scsi_bufflen(srb))
		scsi_set_resid(srb, scsi_bufflen(srb) - buflen);
}
EXPORT_SYMBOL_GPL(usb_stor_set_xfer_buf);
