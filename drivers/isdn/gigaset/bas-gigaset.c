/*
 * USB driver for Gigaset 307x base via direct USB connection.
 *
 * Copyright (c) 2001 by Hansjoerg Lipp <hjlipp@web.de>,
 *                       Tilman Schmidt <tilman@imap.cc>,
 *                       Stefan Eilers <Eilers.Stefan@epost.de>.
 *
 * Based on usb-gigaset.c.
 *
 * =====================================================================
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation; either version 2 of
 *	the License, or (at your option) any later version.
 * =====================================================================
 * ToDo: ...
 * =====================================================================
 * Version: $Id: bas-gigaset.c,v 1.52.4.19 2006/02/04 18:28:16 hjlipp Exp $
 * =====================================================================
 */

#include "gigaset.h"

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/usb.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

/* Version Information */
#define DRIVER_AUTHOR "Tilman Schmidt <tilman@imap.cc>, Hansjoerg Lipp <hjlipp@web.de>, Stefan Eilers <Eilers.Stefan@epost.de>"
#define DRIVER_DESC "USB Driver for Gigaset 307x"


/* Module parameters */

static int startmode = SM_ISDN;
static int cidmode = 1;

module_param(startmode, int, S_IRUGO);
module_param(cidmode, int, S_IRUGO);
MODULE_PARM_DESC(startmode, "start in isdn4linux mode");
MODULE_PARM_DESC(cidmode, "Call-ID mode");

#define GIGASET_MINORS     1
#define GIGASET_MINOR      16
#define GIGASET_MODULENAME "bas_gigaset"
#define GIGASET_DEVFSNAME  "gig/bas/"
#define GIGASET_DEVNAME    "ttyGB"

#define IF_WRITEBUF 256 //FIXME

/* Values for the Gigaset 307x */
#define USB_GIGA_VENDOR_ID      0x0681
#define USB_GIGA_PRODUCT_ID     0x0001
#define USB_4175_PRODUCT_ID     0x0002
#define USB_SX303_PRODUCT_ID    0x0021
#define USB_SX353_PRODUCT_ID    0x0022

/* table of devices that work with this driver */
static struct usb_device_id gigaset_table [] = {
	{ USB_DEVICE(USB_GIGA_VENDOR_ID, USB_GIGA_PRODUCT_ID) },
	{ USB_DEVICE(USB_GIGA_VENDOR_ID, USB_4175_PRODUCT_ID) },
	{ USB_DEVICE(USB_GIGA_VENDOR_ID, USB_SX303_PRODUCT_ID) },
	{ USB_DEVICE(USB_GIGA_VENDOR_ID, USB_SX353_PRODUCT_ID) },
	{ } /* Terminating entry */
};

MODULE_DEVICE_TABLE(usb, gigaset_table);

/* Get a minor range for your devices from the usb maintainer */
#define USB_SKEL_MINOR_BASE	200

/*======================= local function prototypes =============================*/

/* This function is called if a new device is connected to the USB port. It
 * checks whether this new device belongs to this driver.
 */
static int gigaset_probe(struct usb_interface *interface,
			 const struct usb_device_id *id);

/* Function will be called if the device is unplugged */
static void gigaset_disconnect(struct usb_interface *interface);


/*==============================================================================*/

struct bas_cardstate {
	struct usb_device       *udev;		/* USB device pointer */
	struct usb_interface    *interface;	/* interface for this device */
	unsigned char		minor;		/* starting minor number */

	struct urb              *urb_ctrl;	/* control pipe default URB */
	struct usb_ctrlrequest	dr_ctrl;
	struct timer_list	timer_ctrl;	/* control request timeout */

	struct timer_list	timer_atrdy;	/* AT command ready timeout */
	struct urb              *urb_cmd_out;	/* for sending AT commands */
	struct usb_ctrlrequest	dr_cmd_out;
	int			retry_cmd_out;

	struct urb              *urb_cmd_in;	/* for receiving AT replies */
	struct usb_ctrlrequest	dr_cmd_in;
	struct timer_list	timer_cmd_in;	/* receive request timeout */
	unsigned char           *rcvbuf;	/* AT reply receive buffer */

	struct urb              *urb_int_in;	/* URB for interrupt pipe */
	unsigned char		int_in_buf[3];

	spinlock_t		lock;		/* locks all following */
	atomic_t		basstate;	/* bitmap (BS_*) */
	int			pending;	/* uncompleted base request */
	int			rcvbuf_size;	/* size of AT receive buffer */
						/* 0: no receive in progress */
	int			retry_cmd_in;	/* receive req retry count */
};

/* status of direct USB connection to 307x base (bits in basstate) */
#define BS_ATOPEN	0x001
#define BS_B1OPEN	0x002
#define BS_B2OPEN	0x004
#define BS_ATREADY	0x008
#define BS_INIT		0x010
#define BS_ATTIMER	0x020


static struct gigaset_driver *driver = NULL;
static struct cardstate *cardstate = NULL;

/* usb specific object needed to register this driver with the usb subsystem */
static struct usb_driver gigaset_usb_driver = {
	.name =         GIGASET_MODULENAME,
	.probe =        gigaset_probe,
	.disconnect =   gigaset_disconnect,
	.id_table =     gigaset_table,
};

/* get message text for USB status code
 */
static char *get_usb_statmsg(int status)
{
	static char unkmsg[28];

	switch (status) {
	case 0:
		return "success";
	case -ENOENT:
		return "canceled";
	case -ECONNRESET:
		return "canceled (async)";
	case -EINPROGRESS:
		return "pending";
	case -EPROTO:
		return "bit stuffing or unknown USB error";
	case -EILSEQ:
		return "Illegal byte sequence (CRC mismatch)";
	case -EPIPE:
		return "babble detect or endpoint stalled";
	case -ENOSR:
		return "buffer error";
	case -ETIMEDOUT:
		return "timed out";
	case -ENODEV:
		return "device not present";
	case -EREMOTEIO:
		return "short packet detected";
	case -EXDEV:
		return "partial isochronous transfer";
	case -EINVAL:
		return "invalid argument";
	case -ENXIO:
		return "URB already queued";
	case -EAGAIN:
		return "isochronous start frame too early or too much scheduled";
	case -EFBIG:
		return "too many isochronous frames requested";
	case -EMSGSIZE:
		return "endpoint message size zero";
	case -ESHUTDOWN:
		return "endpoint shutdown";
	case -EBUSY:
		return "another request pending";
	default:
		snprintf(unkmsg, sizeof(unkmsg), "unknown error %d", status);
		return unkmsg;
	}
}

/* usb_pipetype_str
 * retrieve string representation of USB pipe type
 */
static inline char *usb_pipetype_str(int pipe)
{
	if (usb_pipeisoc(pipe))
		return "Isoc";
	if (usb_pipeint(pipe))
		return "Int";
	if (usb_pipecontrol(pipe))
		return "Ctrl";
	if (usb_pipebulk(pipe))
		return "Bulk";
	return "?";
}

/* dump_urb
 * write content of URB to syslog for debugging
 */
static inline void dump_urb(enum debuglevel level, const char *tag,
                            struct urb *urb)
{
#ifdef CONFIG_GIGASET_DEBUG
	int i;
	IFNULLRET(tag);
	dbg(level, "%s urb(0x%08lx)->{", tag, (unsigned long) urb);
	if (urb) {
		dbg(level,
		    "  dev=0x%08lx, pipe=%s:EP%d/DV%d:%s, "
		    "status=%d, hcpriv=0x%08lx, transfer_flags=0x%x,",
		    (unsigned long) urb->dev,
		    usb_pipetype_str(urb->pipe),
		    usb_pipeendpoint(urb->pipe), usb_pipedevice(urb->pipe),
		    usb_pipein(urb->pipe) ? "in" : "out",
		    urb->status, (unsigned long) urb->hcpriv,
		    urb->transfer_flags);
		dbg(level,
		    "  transfer_buffer=0x%08lx[%d], actual_length=%d, "
		    "bandwidth=%d, setup_packet=0x%08lx,",
		    (unsigned long) urb->transfer_buffer,
		    urb->transfer_buffer_length, urb->actual_length,
		    urb->bandwidth, (unsigned long) urb->setup_packet);
		dbg(level,
		    "  start_frame=%d, number_of_packets=%d, interval=%d, "
		    "error_count=%d,",
		    urb->start_frame, urb->number_of_packets, urb->interval,
		    urb->error_count);
		dbg(level,
		    "  context=0x%08lx, complete=0x%08lx, iso_frame_desc[]={",
		    (unsigned long) urb->context,
		    (unsigned long) urb->complete);
		for (i = 0; i < urb->number_of_packets; i++) {
			struct usb_iso_packet_descriptor *pifd = &urb->iso_frame_desc[i];
			dbg(level,
			    "    {offset=%u, length=%u, actual_length=%u, "
			    "status=%u}",
			    pifd->offset, pifd->length, pifd->actual_length,
			    pifd->status);
		}
	}
	dbg(level, "}}");
#endif
}

/* read/set modem control bits etc. (m10x only) */
static int gigaset_set_modem_ctrl(struct cardstate *cs, unsigned old_state,
                                  unsigned new_state)
{
	return -EINVAL;
}

static int gigaset_baud_rate(struct cardstate *cs, unsigned cflag)
{
	return -EINVAL;
}

static int gigaset_set_line_ctrl(struct cardstate *cs, unsigned cflag)
{
	return -EINVAL;
}

/* error_hangup
 * hang up any existing connection because of an unrecoverable error
 * This function may be called from any context and takes care of scheduling
 * the necessary actions for execution outside of interrupt context.
 * argument:
 *	B channel control structure
 */
static inline void error_hangup(struct bc_state *bcs)
{
	struct cardstate *cs = bcs->cs;

	dbg(DEBUG_ANY,
	    "%s: scheduling HUP for channel %d", __func__, bcs->channel);

	if (!gigaset_add_event(cs, &bcs->at_state, EV_HUP, NULL, 0, NULL)) {
		//FIXME what should we do?
		return;
	}

	gigaset_schedule_event(cs);
}

/* error_reset
 * reset Gigaset device because of an unrecoverable error
 * This function may be called from any context and takes care of scheduling
 * the necessary actions for execution outside of interrupt context.
 * argument:
 *	controller state structure
 */
static inline void error_reset(struct cardstate *cs)
{
	//FIXME try to recover without bothering the user
	err("unrecoverable error - please disconnect the Gigaset base to reset");
}

/* check_pending
 * check for completion of pending control request
 * parameter:
 *	urb	USB request block of completed request
 *		urb->context = hardware specific controller state structure
 */
static void check_pending(struct bas_cardstate *ucs)
{
	unsigned long flags;

	IFNULLRET(ucs);
	IFNULLRET(cardstate);

	spin_lock_irqsave(&ucs->lock, flags);
	switch (ucs->pending) {
	case 0:
		break;
	case HD_OPEN_ATCHANNEL:
		if (atomic_read(&ucs->basstate) & BS_ATOPEN)
			ucs->pending = 0;
		break;
	case HD_OPEN_B1CHANNEL:
		if (atomic_read(&ucs->basstate) & BS_B1OPEN)
			ucs->pending = 0;
		break;
	case HD_OPEN_B2CHANNEL:
		if (atomic_read(&ucs->basstate) & BS_B2OPEN)
			ucs->pending = 0;
		break;
	case HD_CLOSE_ATCHANNEL:
		if (!(atomic_read(&ucs->basstate) & BS_ATOPEN))
			ucs->pending = 0;
		//wake_up_interruptible(cs->initwait);
		//FIXME need own wait queue?
		break;
	case HD_CLOSE_B1CHANNEL:
		if (!(atomic_read(&ucs->basstate) & BS_B1OPEN))
			ucs->pending = 0;
		break;
	case HD_CLOSE_B2CHANNEL:
		if (!(atomic_read(&ucs->basstate) & BS_B2OPEN))
			ucs->pending = 0;
		break;
	case HD_DEVICE_INIT_ACK:		/* no reply expected */
		ucs->pending = 0;
		break;
	/* HD_READ_ATMESSAGE, HD_WRITE_ATMESSAGE, HD_RESET_INTERRUPTPIPE
	 * are handled separately and should never end up here
	 */
	default:
		warn("unknown pending request 0x%02x cleared", ucs->pending);
		ucs->pending = 0;
	}

	if (!ucs->pending)
		del_timer(&ucs->timer_ctrl);

	spin_unlock_irqrestore(&ucs->lock, flags);
}

/* cmd_in_timeout
 * timeout routine for command input request
 * argument:
 *	controller state structure
 */
static void cmd_in_timeout(unsigned long data)
{
	struct cardstate *cs = (struct cardstate *) data;
	struct bas_cardstate *ucs;
	unsigned long flags;

	IFNULLRET(cs);
	ucs = cs->hw.bas;
	IFNULLRET(ucs);

	spin_lock_irqsave(&cs->lock, flags);
	if (!atomic_read(&cs->connected)) {
		dbg(DEBUG_USBREQ, "%s: disconnected", __func__);
		spin_unlock_irqrestore(&cs->lock, flags);
		return;
	}
	if (!ucs->rcvbuf_size) {
		dbg(DEBUG_USBREQ, "%s: no receive in progress", __func__);
		spin_unlock_irqrestore(&cs->lock, flags);
		return;
	}
	spin_unlock_irqrestore(&cs->lock, flags);

	err("timeout reading AT response");
	error_reset(cs);	//FIXME retry?
}


static void read_ctrl_callback(struct urb *urb, struct pt_regs *regs);

/* atread_submit
 * submit an HD_READ_ATMESSAGE command URB
 * parameters:
 *	cs	controller state structure
 *	timeout	timeout in 1/10 sec., 0: none
 * return value:
 *	0 on success
 *	-EINVAL if a NULL pointer is encountered somewhere
 *	-EBUSY if another request is pending
 *	any URB submission error code
 */
static int atread_submit(struct cardstate *cs, int timeout)
{
	struct bas_cardstate *ucs;
	int ret;

	IFNULLRETVAL(cs, -EINVAL);
	ucs = cs->hw.bas;
	IFNULLRETVAL(ucs, -EINVAL);
	IFNULLRETVAL(ucs->urb_cmd_in, -EINVAL);

	dbg(DEBUG_USBREQ, "-------> HD_READ_ATMESSAGE (%d)", ucs->rcvbuf_size);

	if (ucs->urb_cmd_in->status == -EINPROGRESS) {
		err("could not submit HD_READ_ATMESSAGE: URB busy");
		return -EBUSY;
	}

	ucs->dr_cmd_in.bRequestType = IN_VENDOR_REQ;
	ucs->dr_cmd_in.bRequest = HD_READ_ATMESSAGE;
	ucs->dr_cmd_in.wValue = 0;
	ucs->dr_cmd_in.wIndex = 0;
	ucs->dr_cmd_in.wLength = cpu_to_le16(ucs->rcvbuf_size);
	usb_fill_control_urb(ucs->urb_cmd_in, ucs->udev,
	                     usb_rcvctrlpipe(ucs->udev, 0),
	                     (unsigned char*) & ucs->dr_cmd_in,
	                     ucs->rcvbuf, ucs->rcvbuf_size,
	                     read_ctrl_callback, cs->inbuf);

	if ((ret = usb_submit_urb(ucs->urb_cmd_in, SLAB_ATOMIC)) != 0) {
		err("could not submit HD_READ_ATMESSAGE: %s",
		    get_usb_statmsg(ret));
		return ret;
	}

	if (timeout > 0) {
		dbg(DEBUG_USBREQ, "setting timeout of %d/10 secs", timeout);
		ucs->timer_cmd_in.expires = jiffies + timeout * HZ / 10;
		ucs->timer_cmd_in.data = (unsigned long) cs;
		ucs->timer_cmd_in.function = cmd_in_timeout;
		add_timer(&ucs->timer_cmd_in);
	}
	return 0;
}

static void stopurbs(struct bas_bc_state *);
static int start_cbsend(struct cardstate *);

/* set/clear bits in base connection state
 */
inline static void update_basstate(struct bas_cardstate *ucs,
				   int set, int clear)
{
	unsigned long flags;
	int state;

	spin_lock_irqsave(&ucs->lock, flags);
	state = atomic_read(&ucs->basstate);
	state &= ~clear;
	state |= set;
	atomic_set(&ucs->basstate, state);
	spin_unlock_irqrestore(&ucs->lock, flags);
}


/* read_int_callback
 * USB completion handler for interrupt pipe input
 * called by the USB subsystem in interrupt context
 * parameter:
 *	urb	USB request block
 *		urb->context = controller state structure
 */
static void read_int_callback(struct urb *urb, struct pt_regs *regs)
{
	struct cardstate *cs;
	struct bas_cardstate *ucs;
	struct bc_state *bcs;
	unsigned long flags;
	int status;
	unsigned l;
	int channel;

	IFNULLRET(urb);
	cs = (struct cardstate *) urb->context;
	IFNULLRET(cs);
	ucs = cs->hw.bas;
	IFNULLRET(ucs);

	if (unlikely(!atomic_read(&cs->connected))) {
		warn("%s: disconnected", __func__);
		return;
	}

	switch (urb->status) {
	case 0:			/* success */
		break;
	case -ENOENT:			/* canceled */
	case -ECONNRESET:		/* canceled (async) */
	case -EINPROGRESS:		/* pending */
		/* ignore silently */
		dbg(DEBUG_USBREQ,
		    "%s: %s", __func__, get_usb_statmsg(urb->status));
		return;
	default:		/* severe trouble */
		warn("interrupt read: %s", get_usb_statmsg(urb->status));
		//FIXME corrective action? resubmission always ok?
		goto resubmit;
	}

	l = (unsigned) ucs->int_in_buf[1] +
	    (((unsigned) ucs->int_in_buf[2]) << 8);

	dbg(DEBUG_USBREQ,
	    "<-------%d: 0x%02x (%u [0x%02x 0x%02x])", urb->actual_length,
	    (int)ucs->int_in_buf[0], l,
	    (int)ucs->int_in_buf[1], (int)ucs->int_in_buf[2]);

	channel = 0;

	switch (ucs->int_in_buf[0]) {
	case HD_DEVICE_INIT_OK:
		update_basstate(ucs, BS_INIT, 0);
		break;

	case HD_READY_SEND_ATDATA:
		del_timer(&ucs->timer_atrdy);
		update_basstate(ucs, BS_ATREADY, BS_ATTIMER);
		start_cbsend(cs);
		break;

	case HD_OPEN_B2CHANNEL_ACK:
		++channel;
	case HD_OPEN_B1CHANNEL_ACK:
		bcs = cs->bcs + channel;
		update_basstate(ucs, BS_B1OPEN << channel, 0);
		gigaset_bchannel_up(bcs);
		break;

	case HD_OPEN_ATCHANNEL_ACK:
		update_basstate(ucs, BS_ATOPEN, 0);
		start_cbsend(cs);
		break;

	case HD_CLOSE_B2CHANNEL_ACK:
		++channel;
	case HD_CLOSE_B1CHANNEL_ACK:
		bcs = cs->bcs + channel;
		update_basstate(ucs, 0, BS_B1OPEN << channel);
		stopurbs(bcs->hw.bas);
		gigaset_bchannel_down(bcs);
		break;

	case HD_CLOSE_ATCHANNEL_ACK:
		update_basstate(ucs, 0, BS_ATOPEN);
		break;

	case HD_B2_FLOW_CONTROL:
		++channel;
	case HD_B1_FLOW_CONTROL:
		bcs = cs->bcs + channel;
		atomic_add((l - BAS_NORMFRAME) * BAS_CORRFRAMES,
		           &bcs->hw.bas->corrbytes);
		dbg(DEBUG_ISO,
		    "Flow control (channel %d, sub %d): 0x%02x => %d",
		    channel, bcs->hw.bas->numsub, l,
		    atomic_read(&bcs->hw.bas->corrbytes));
		break;

	case HD_RECEIVEATDATA_ACK:	/* AT response ready to be received */
		if (!l) {
			warn("HD_RECEIVEATDATA_ACK with length 0 ignored");
			break;
		}
		spin_lock_irqsave(&cs->lock, flags);
		if (ucs->rcvbuf_size) {
			spin_unlock_irqrestore(&cs->lock, flags);
			err("receive AT data overrun, %d bytes lost", l);
			error_reset(cs);	//FIXME reschedule
			break;
		}
		if ((ucs->rcvbuf = kmalloc(l, GFP_ATOMIC)) == NULL) {
			spin_unlock_irqrestore(&cs->lock, flags);
			err("%s: out of memory, %d bytes lost", __func__, l);
			error_reset(cs);	//FIXME reschedule
			break;
		}
		ucs->rcvbuf_size = l;
		ucs->retry_cmd_in = 0;
		if ((status = atread_submit(cs, BAS_TIMEOUT)) < 0) {
			kfree(ucs->rcvbuf);
			ucs->rcvbuf = NULL;
			ucs->rcvbuf_size = 0;
			error_reset(cs);	//FIXME reschedule
		}
		spin_unlock_irqrestore(&cs->lock, flags);
		break;

	case HD_RESET_INTERRUPT_PIPE_ACK:
		dbg(DEBUG_USBREQ, "HD_RESET_INTERRUPT_PIPE_ACK");
		break;

	case HD_SUSPEND_END:
		dbg(DEBUG_USBREQ, "HD_SUSPEND_END");
		break;

	default:
		warn("unknown Gigaset signal 0x%02x (%u) ignored",
		     (int) ucs->int_in_buf[0], l);
	}

	check_pending(ucs);

resubmit:
	status = usb_submit_urb(urb, SLAB_ATOMIC);
	if (unlikely(status)) {
		err("could not resubmit interrupt URB: %s",
		    get_usb_statmsg(status));
		error_reset(cs);
	}
}

/* read_ctrl_callback
 * USB completion handler for control pipe input
 * called by the USB subsystem in interrupt context
 * parameter:
 *	urb	USB request block
 *		urb->context = inbuf structure for controller state
 */
static void read_ctrl_callback(struct urb *urb, struct pt_regs *regs)
{
	struct cardstate *cs;
	struct bas_cardstate *ucs;
	unsigned numbytes;
	unsigned long flags;
	struct inbuf_t *inbuf;
	int have_data = 0;

	IFNULLRET(urb);
	inbuf = (struct inbuf_t *) urb->context;
	IFNULLRET(inbuf);
	cs = inbuf->cs;
	IFNULLRET(cs);
	ucs = cs->hw.bas;
	IFNULLRET(ucs);

	spin_lock_irqsave(&cs->lock, flags);
	if (!atomic_read(&cs->connected)) {
		warn("%s: disconnected", __func__);
		spin_unlock_irqrestore(&cs->lock, flags);
		return;
	}

	if (!ucs->rcvbuf_size) {
		warn("%s: no receive in progress", __func__);
		spin_unlock_irqrestore(&cs->lock, flags);
		return;
	}

	del_timer(&ucs->timer_cmd_in);

	switch (urb->status) {
	case 0:				/* normal completion */
		numbytes = urb->actual_length;
		if (unlikely(numbytes == 0)) {
			warn("control read: empty block received");
			goto retry;
		}
		if (unlikely(numbytes != ucs->rcvbuf_size)) {
			warn("control read: received %d chars, expected %d",
			     numbytes, ucs->rcvbuf_size);
			if (numbytes > ucs->rcvbuf_size)
				numbytes = ucs->rcvbuf_size;
		}

		/* copy received bytes to inbuf */
		have_data = gigaset_fill_inbuf(inbuf, ucs->rcvbuf, numbytes);

		if (unlikely(numbytes < ucs->rcvbuf_size)) {
			/* incomplete - resubmit for remaining bytes */
			ucs->rcvbuf_size -= numbytes;
			ucs->retry_cmd_in = 0;
			goto retry;
		}
		break;

	case -ENOENT:			/* canceled */
	case -ECONNRESET:		/* canceled (async) */
	case -EINPROGRESS:		/* pending */
		/* no action necessary */
		dbg(DEBUG_USBREQ,
		    "%s: %s", __func__, get_usb_statmsg(urb->status));
		break;

	default:			/* severe trouble */
		warn("control read: %s", get_usb_statmsg(urb->status));
	retry:
		if (ucs->retry_cmd_in++ < BAS_RETRY) {
			notice("control read: retry %d", ucs->retry_cmd_in);
			if (atread_submit(cs, BAS_TIMEOUT) >= 0) {
				/* resubmitted - bypass regular exit block */
				spin_unlock_irqrestore(&cs->lock, flags);
				return;
			}
		} else {
			err("control read: giving up after %d tries",
			    ucs->retry_cmd_in);
		}
		error_reset(cs);
	}

	kfree(ucs->rcvbuf);
	ucs->rcvbuf = NULL;
	ucs->rcvbuf_size = 0;
	spin_unlock_irqrestore(&cs->lock, flags);
	if (have_data) {
		dbg(DEBUG_INTR, "%s-->BH", __func__);
		gigaset_schedule_event(cs);
	}
}

/* read_iso_callback
 * USB completion handler for B channel isochronous input
 * called by the USB subsystem in interrupt context
 * parameter:
 *	urb	USB request block of completed request
 *		urb->context = bc_state structure
 */
static void read_iso_callback(struct urb *urb, struct pt_regs *regs)
{
	struct bc_state *bcs;
	struct bas_bc_state *ubc;
	unsigned long flags;
	int i, rc;

	IFNULLRET(urb);
	IFNULLRET(urb->context);
	IFNULLRET(cardstate);

	/* status codes not worth bothering the tasklet with */
	if (unlikely(urb->status == -ENOENT || urb->status == -ECONNRESET ||
	             urb->status == -EINPROGRESS)) {
		dbg(DEBUG_ISO,
		    "%s: %s", __func__, get_usb_statmsg(urb->status));
		return;
	}

	bcs = (struct bc_state *) urb->context;
	ubc = bcs->hw.bas;
	IFNULLRET(ubc);

	spin_lock_irqsave(&ubc->isoinlock, flags);
	if (likely(ubc->isoindone == NULL)) {
		/* pass URB to tasklet */
		ubc->isoindone = urb;
		tasklet_schedule(&ubc->rcvd_tasklet);
	} else {
		/* tasklet still busy, drop data and resubmit URB */
		ubc->loststatus = urb->status;
		for (i = 0; i < BAS_NUMFRAMES; i++) {
			ubc->isoinlost += urb->iso_frame_desc[i].actual_length;
			if (unlikely(urb->iso_frame_desc[i].status != 0 &&
				     urb->iso_frame_desc[i].status != -EINPROGRESS)) {
				ubc->loststatus = urb->iso_frame_desc[i].status;
			}
			urb->iso_frame_desc[i].status = 0;
			urb->iso_frame_desc[i].actual_length = 0;
		}
		if (likely(atomic_read(&ubc->running))) {
			urb->dev = bcs->cs->hw.bas->udev;	/* clobbered by USB subsystem */
			urb->transfer_flags = URB_ISO_ASAP;
			urb->number_of_packets = BAS_NUMFRAMES;
			dbg(DEBUG_ISO, "%s: isoc read overrun/resubmit", __func__);
			rc = usb_submit_urb(urb, SLAB_ATOMIC);
			if (unlikely(rc != 0)) {
				err("could not resubmit isochronous read URB: %s",
				    get_usb_statmsg(rc));
				dump_urb(DEBUG_ISO, "isoc read", urb);
				error_hangup(bcs);
			}
		}
	}
	spin_unlock_irqrestore(&ubc->isoinlock, flags);
}

/* write_iso_callback
 * USB completion handler for B channel isochronous output
 * called by the USB subsystem in interrupt context
 * parameter:
 *	urb	USB request block of completed request
 *		urb->context = isow_urbctx_t structure
 */
static void write_iso_callback(struct urb *urb, struct pt_regs *regs)
{
	struct isow_urbctx_t *ucx;
	struct bas_bc_state *ubc;
	unsigned long flags;

	IFNULLRET(urb);
	IFNULLRET(urb->context);
	IFNULLRET(cardstate);

	/* status codes not worth bothering the tasklet with */
	if (unlikely(urb->status == -ENOENT || urb->status == -ECONNRESET ||
	             urb->status == -EINPROGRESS)) {
		dbg(DEBUG_ISO,
		    "%s: %s", __func__, get_usb_statmsg(urb->status));
		return;
	}

	/* pass URB context to tasklet */
	ucx = (struct isow_urbctx_t *) urb->context;
	IFNULLRET(ucx->bcs);
	ubc = ucx->bcs->hw.bas;
	IFNULLRET(ubc);

	spin_lock_irqsave(&ubc->isooutlock, flags);
	ubc->isooutovfl = ubc->isooutdone;
	ubc->isooutdone = ucx;
	spin_unlock_irqrestore(&ubc->isooutlock, flags);
	tasklet_schedule(&ubc->sent_tasklet);
}

/* starturbs
 * prepare and submit USB request blocks for isochronous input and output
 * argument:
 *	B channel control structure
 * return value:
 *	0 on success
 *	< 0 on error (no URBs submitted)
 */
static int starturbs(struct bc_state *bcs)
{
	struct urb *urb;
	struct bas_bc_state *ubc;
	int j, k;
	int rc;

	IFNULLRETVAL(bcs, -EFAULT);
	ubc = bcs->hw.bas;
	IFNULLRETVAL(ubc, -EFAULT);

	/* initialize L2 reception */
	if (bcs->proto2 == ISDN_PROTO_L2_HDLC)
		bcs->inputstate |= INS_flag_hunt;

	/* submit all isochronous input URBs */
	atomic_set(&ubc->running, 1);
	for (k = 0; k < BAS_INURBS; k++) {
		urb = ubc->isoinurbs[k];
		if (!urb) {
			err("isoinurbs[%d]==NULL", k);
			rc = -EFAULT;
			goto error;
		}

		urb->dev = bcs->cs->hw.bas->udev;
		urb->pipe = usb_rcvisocpipe(urb->dev, 3 + 2 * bcs->channel);
		urb->transfer_flags = URB_ISO_ASAP;
		urb->transfer_buffer = ubc->isoinbuf + k * BAS_INBUFSIZE;
		urb->transfer_buffer_length = BAS_INBUFSIZE;
		urb->number_of_packets = BAS_NUMFRAMES;
		urb->interval = BAS_FRAMETIME;
		urb->complete = read_iso_callback;
		urb->context = bcs;
		for (j = 0; j < BAS_NUMFRAMES; j++) {
			urb->iso_frame_desc[j].offset = j * BAS_MAXFRAME;
			urb->iso_frame_desc[j].length = BAS_MAXFRAME;
			urb->iso_frame_desc[j].status = 0;
			urb->iso_frame_desc[j].actual_length = 0;
		}

		dump_urb(DEBUG_ISO, "Initial isoc read", urb);
		if ((rc = usb_submit_urb(urb, SLAB_ATOMIC)) != 0) {
			err("could not submit isochronous read URB %d: %s",
			    k, get_usb_statmsg(rc));
			goto error;
		}
	}

	/* initialize L2 transmission */
	gigaset_isowbuf_init(ubc->isooutbuf, PPP_FLAG);

	/* set up isochronous output URBs for flag idling */
	for (k = 0; k < BAS_OUTURBS; ++k) {
		urb = ubc->isoouturbs[k].urb;
		if (!urb) {
			err("isoouturbs[%d].urb==NULL", k);
			rc = -EFAULT;
			goto error;
		}
		urb->dev = bcs->cs->hw.bas->udev;
		urb->pipe = usb_sndisocpipe(urb->dev, 4 + 2 * bcs->channel);
		urb->transfer_flags = URB_ISO_ASAP;
		urb->transfer_buffer = ubc->isooutbuf->data;
		urb->transfer_buffer_length = sizeof(ubc->isooutbuf->data);
		urb->number_of_packets = BAS_NUMFRAMES;
		urb->interval = BAS_FRAMETIME;
		urb->complete = write_iso_callback;
		urb->context = &ubc->isoouturbs[k];
		for (j = 0; j < BAS_NUMFRAMES; ++j) {
			urb->iso_frame_desc[j].offset = BAS_OUTBUFSIZE;
			urb->iso_frame_desc[j].length = BAS_NORMFRAME;
			urb->iso_frame_desc[j].status = 0;
			urb->iso_frame_desc[j].actual_length = 0;
		}
		ubc->isoouturbs[k].limit = -1;
	}

	/* submit two URBs, keep third one */
	for (k = 0; k < 2; ++k) {
		dump_urb(DEBUG_ISO, "Initial isoc write", urb);
		rc = usb_submit_urb(ubc->isoouturbs[k].urb, SLAB_ATOMIC);
		if (rc != 0) {
			err("could not submit isochronous write URB %d: %s",
			    k, get_usb_statmsg(rc));
			goto error;
		}
	}
	dump_urb(DEBUG_ISO, "Initial isoc write (free)", urb);
	ubc->isooutfree = &ubc->isoouturbs[2];
	ubc->isooutdone = ubc->isooutovfl = NULL;
	return 0;
 error:
	stopurbs(ubc);
	return rc;
}

/* stopurbs
 * cancel the USB request blocks for isochronous input and output
 * errors are silently ignored
 * argument:
 *	B channel control structure
 */
static void stopurbs(struct bas_bc_state *ubc)
{
	int k, rc;

	IFNULLRET(ubc);

	atomic_set(&ubc->running, 0);

	for (k = 0; k < BAS_INURBS; ++k) {
		rc = usb_unlink_urb(ubc->isoinurbs[k]);
		dbg(DEBUG_ISO, "%s: isoc input URB %d unlinked, result = %d",
		    __func__, k, rc);
	}

	for (k = 0; k < BAS_OUTURBS; ++k) {
		rc = usb_unlink_urb(ubc->isoouturbs[k].urb);
		dbg(DEBUG_ISO, "%s: isoc output URB %d unlinked, result = %d",
		    __func__, k, rc);
	}
}

/* Isochronous Write - Bottom Half */
/* =============================== */

/* submit_iso_write_urb
 * fill and submit the next isochronous write URB
 * parameters:
 *	bcs	B channel state structure
 * return value:
 *	number of frames submitted in URB
 *	0 if URB not submitted because no data available (isooutbuf busy)
 *	error code < 0 on error
 */
static int submit_iso_write_urb(struct isow_urbctx_t *ucx)
{
	struct urb *urb;
	struct bas_bc_state *ubc;
	struct usb_iso_packet_descriptor *ifd;
	int corrbytes, nframe, rc;

	IFNULLRETVAL(ucx, -EFAULT);
	urb = ucx->urb;
	IFNULLRETVAL(urb, -EFAULT);
	IFNULLRETVAL(ucx->bcs, -EFAULT);
	ubc = ucx->bcs->hw.bas;
	IFNULLRETVAL(ubc, -EFAULT);

	urb->dev = ucx->bcs->cs->hw.bas->udev;	/* clobbered by USB subsystem */
	urb->transfer_flags = URB_ISO_ASAP;
	urb->transfer_buffer = ubc->isooutbuf->data;
	urb->transfer_buffer_length = sizeof(ubc->isooutbuf->data);

	for (nframe = 0; nframe < BAS_NUMFRAMES; nframe++) {
		ifd = &urb->iso_frame_desc[nframe];

		/* compute frame length according to flow control */
		ifd->length = BAS_NORMFRAME;
		if ((corrbytes = atomic_read(&ubc->corrbytes)) != 0) {
			dbg(DEBUG_ISO, "%s: corrbytes=%d", __func__, corrbytes);
			if (corrbytes > BAS_HIGHFRAME - BAS_NORMFRAME)
				corrbytes = BAS_HIGHFRAME - BAS_NORMFRAME;
			else if (corrbytes < BAS_LOWFRAME - BAS_NORMFRAME)
				corrbytes = BAS_LOWFRAME - BAS_NORMFRAME;
			ifd->length += corrbytes;
			atomic_add(-corrbytes, &ubc->corrbytes);
		}
		//dbg(DEBUG_ISO, "%s: frame %d length=%d", __func__, nframe, ifd->length);

		/* retrieve block of data to send */
		ifd->offset = gigaset_isowbuf_getbytes(ubc->isooutbuf, ifd->length);
		if (ifd->offset < 0) {
			if (ifd->offset == -EBUSY) {
				dbg(DEBUG_ISO, "%s: buffer busy at frame %d",
				    __func__, nframe);
				/* tasklet will be restarted from gigaset_send_skb() */
			} else {
				err("%s: buffer error %d at frame %d",
				    __func__, ifd->offset, nframe);
				return ifd->offset;
			}
			break;
		}
		ucx->limit = atomic_read(&ubc->isooutbuf->nextread);
		ifd->status = 0;
		ifd->actual_length = 0;
	}
	if ((urb->number_of_packets = nframe) > 0) {
		if ((rc = usb_submit_urb(urb, SLAB_ATOMIC)) != 0) {
			err("could not submit isochronous write URB: %s",
			    get_usb_statmsg(rc));
			dump_urb(DEBUG_ISO, "isoc write", urb);
			return rc;
		}
		++ubc->numsub;
	}
	return nframe;
}

/* write_iso_tasklet
 * tasklet scheduled when an isochronous output URB from the Gigaset device
 * has completed
 * parameter:
 *	data	B channel state structure
 */
static void write_iso_tasklet(unsigned long data)
{
	struct bc_state *bcs;
	struct bas_bc_state *ubc;
	struct cardstate *cs;
	struct isow_urbctx_t *done, *next, *ovfl;
	struct urb *urb;
	struct usb_iso_packet_descriptor *ifd;
	int offset;
	unsigned long flags;
	int i;
	struct sk_buff *skb;
	int len;

	bcs = (struct bc_state *) data;
	IFNULLRET(bcs);
	ubc = bcs->hw.bas;
	IFNULLRET(ubc);
	cs = bcs->cs;
	IFNULLRET(cs);

	/* loop while completed URBs arrive in time */
	for (;;) {
		if (unlikely(!atomic_read(&cs->connected))) {
			warn("%s: disconnected", __func__);
			return;
		}

		if (unlikely(!(atomic_read(&ubc->running)))) {
			dbg(DEBUG_ISO, "%s: not running", __func__);
			return;
		}

		/* retrieve completed URBs */
		spin_lock_irqsave(&ubc->isooutlock, flags);
		done = ubc->isooutdone;
		ubc->isooutdone = NULL;
		ovfl = ubc->isooutovfl;
		ubc->isooutovfl = NULL;
		spin_unlock_irqrestore(&ubc->isooutlock, flags);
		if (ovfl) {
			err("isochronous write buffer underrun - buy a faster machine :-)");
			error_hangup(bcs);
			break;
		}
		if (!done)
			break;

		/* submit free URB if available */
		spin_lock_irqsave(&ubc->isooutlock, flags);
		next = ubc->isooutfree;
		ubc->isooutfree = NULL;
		spin_unlock_irqrestore(&ubc->isooutlock, flags);
		if (next) {
			if (submit_iso_write_urb(next) <= 0) {
				/* could not submit URB, put it back */
				spin_lock_irqsave(&ubc->isooutlock, flags);
				if (ubc->isooutfree == NULL) {
					ubc->isooutfree = next;
					next = NULL;
				}
				spin_unlock_irqrestore(&ubc->isooutlock, flags);
				if (next) {
					/* couldn't put it back */
					err("losing isochronous write URB");
					error_hangup(bcs);
				}
			}
		}

		/* process completed URB */
		urb = done->urb;
		switch (urb->status) {
		case 0:				/* normal completion */
			break;
		case -EXDEV:			/* inspect individual frames */
			/* assumptions (for lack of documentation):
			 * - actual_length bytes of the frame in error are successfully sent
			 * - all following frames are not sent at all
			 */
			dbg(DEBUG_ISO, "%s: URB partially completed", __func__);
			offset = done->limit;	/* just in case */
			for (i = 0; i < BAS_NUMFRAMES; i++) {
				ifd = &urb->iso_frame_desc[i];
				if (ifd->status ||
				    ifd->actual_length != ifd->length) {
					warn("isochronous write: frame %d: %s, "
					     "only %d of %d bytes sent",
					     i, get_usb_statmsg(ifd->status),
					     ifd->actual_length, ifd->length);
					offset = (ifd->offset +
					          ifd->actual_length)
					         % BAS_OUTBUFSIZE;
					break;
				}
			}
#ifdef CONFIG_GIGASET_DEBUG
			/* check assumption on remaining frames */
			for (; i < BAS_NUMFRAMES; i++) {
				ifd = &urb->iso_frame_desc[i];
				if (ifd->status != -EINPROGRESS
				    || ifd->actual_length != 0) {
					warn("isochronous write: frame %d: %s, "
					     "%d of %d bytes sent",
					     i, get_usb_statmsg(ifd->status),
					     ifd->actual_length, ifd->length);
					offset = (ifd->offset +
					          ifd->actual_length)
					         % BAS_OUTBUFSIZE;
					break;
				}
			}
#endif
			break;
		case -EPIPE:			//FIXME is this the code for "underrun"?
			err("isochronous write stalled");
			error_hangup(bcs);
			break;
		default:			/* severe trouble */
			warn("isochronous write: %s",
			     get_usb_statmsg(urb->status));
		}

		/* mark the write buffer area covered by this URB as free */
		if (done->limit >= 0)
			atomic_set(&ubc->isooutbuf->read, done->limit);

		/* mark URB as free */
		spin_lock_irqsave(&ubc->isooutlock, flags);
		next = ubc->isooutfree;
		ubc->isooutfree = done;
		spin_unlock_irqrestore(&ubc->isooutlock, flags);
		if (next) {
			/* only one URB still active - resubmit one */
			if (submit_iso_write_urb(next) <= 0) {
				/* couldn't submit */
				error_hangup(bcs);
			}
		}
	}

	/* process queued SKBs */
	while ((skb = skb_dequeue(&bcs->squeue))) {
		/* copy to output buffer, doing L2 encapsulation */
		len = skb->len;
		if (gigaset_isoc_buildframe(bcs, skb->data, len) == -EAGAIN) {
			/* insufficient buffer space, push back onto queue */
			skb_queue_head(&bcs->squeue, skb);
			dbg(DEBUG_ISO, "%s: skb requeued, qlen=%d",
			    __func__, skb_queue_len(&bcs->squeue));
			break;
		}
		skb_pull(skb, len);
		gigaset_skb_sent(bcs, skb);
		dev_kfree_skb_any(skb);
	}
}

/* Isochronous Read - Bottom Half */
/* ============================== */

/* read_iso_tasklet
 * tasklet scheduled when an isochronous input URB from the Gigaset device
 * has completed
 * parameter:
 *	data	B channel state structure
 */
static void read_iso_tasklet(unsigned long data)
{
	struct bc_state *bcs;
	struct bas_bc_state *ubc;
	struct cardstate *cs;
	struct urb *urb;
	char *rcvbuf;
	unsigned long flags;
	int totleft, numbytes, offset, frame, rc;

	bcs = (struct bc_state *) data;
	IFNULLRET(bcs);
	ubc = bcs->hw.bas;
	IFNULLRET(ubc);
	cs = bcs->cs;
	IFNULLRET(cs);

	/* loop while more completed URBs arrive in the meantime */
	for (;;) {
		if (!atomic_read(&cs->connected)) {
			warn("%s: disconnected", __func__);
			return;
		}

		/* retrieve URB */
		spin_lock_irqsave(&ubc->isoinlock, flags);
		if (!(urb = ubc->isoindone)) {
			spin_unlock_irqrestore(&ubc->isoinlock, flags);
			return;
		}
		ubc->isoindone = NULL;
		if (unlikely(ubc->loststatus != -EINPROGRESS)) {
			warn("isochronous read overrun, dropped URB with status: %s, %d bytes lost",
			     get_usb_statmsg(ubc->loststatus), ubc->isoinlost);
			ubc->loststatus = -EINPROGRESS;
		}
		spin_unlock_irqrestore(&ubc->isoinlock, flags);

		if (unlikely(!(atomic_read(&ubc->running)))) {
			dbg(DEBUG_ISO, "%s: channel not running, dropped URB with status: %s",
			    __func__, get_usb_statmsg(urb->status));
			return;
		}

		switch (urb->status) {
		case 0:				/* normal completion */
			break;
		case -EXDEV:			/* inspect individual frames (we do that anyway) */
			dbg(DEBUG_ISO, "%s: URB partially completed", __func__);
			break;
		case -ENOENT:
		case -ECONNRESET:
			dbg(DEBUG_ISO, "%s: URB canceled", __func__);
			continue;		/* -> skip */
		case -EINPROGRESS:		/* huh? */
			dbg(DEBUG_ISO, "%s: URB still pending", __func__);
			continue;		/* -> skip */
		case -EPIPE:
			err("isochronous read stalled");
			error_hangup(bcs);
			continue;		/* -> skip */
		default:			/* severe trouble */
			warn("isochronous read: %s",
			     get_usb_statmsg(urb->status));
			goto error;
		}

		rcvbuf = urb->transfer_buffer;
		totleft = urb->actual_length;
		for (frame = 0; totleft > 0 && frame < BAS_NUMFRAMES; frame++) {
			if (unlikely(urb->iso_frame_desc[frame].status)) {
				warn("isochronous read: frame %d: %s",
				     frame, get_usb_statmsg(urb->iso_frame_desc[frame].status));
				break;
			}
			numbytes = urb->iso_frame_desc[frame].actual_length;
			if (unlikely(numbytes > BAS_MAXFRAME)) {
				warn("isochronous read: frame %d: numbytes (%d) > BAS_MAXFRAME",
				     frame, numbytes);
				break;
			}
			if (unlikely(numbytes > totleft)) {
				warn("isochronous read: frame %d: numbytes (%d) > totleft (%d)",
				     frame, numbytes, totleft);
				break;
			}
			offset = urb->iso_frame_desc[frame].offset;
			if (unlikely(offset + numbytes > BAS_INBUFSIZE)) {
				warn("isochronous read: frame %d: offset (%d) + numbytes (%d) > BAS_INBUFSIZE",
				     frame, offset, numbytes);
				break;
			}
			gigaset_isoc_receive(rcvbuf + offset, numbytes, bcs);
			totleft -= numbytes;
		}
		if (unlikely(totleft > 0))
			warn("isochronous read: %d data bytes missing",
			     totleft);

	error:
		/* URB processed, resubmit */
		for (frame = 0; frame < BAS_NUMFRAMES; frame++) {
			urb->iso_frame_desc[frame].status = 0;
			urb->iso_frame_desc[frame].actual_length = 0;
		}
		urb->dev = bcs->cs->hw.bas->udev;	/* clobbered by USB subsystem */
		urb->transfer_flags = URB_ISO_ASAP;
		urb->number_of_packets = BAS_NUMFRAMES;
		if ((rc = usb_submit_urb(urb, SLAB_ATOMIC)) != 0) {
			err("could not resubmit isochronous read URB: %s",
			    get_usb_statmsg(rc));
			dump_urb(DEBUG_ISO, "resubmit iso read", urb);
			error_hangup(bcs);
		}
	}
}

/* Channel Operations */
/* ================== */

/* req_timeout
 * timeout routine for control output request
 * argument:
 *	B channel control structure
 */
static void req_timeout(unsigned long data)
{
	struct bc_state *bcs = (struct bc_state *) data;
	struct bas_cardstate *ucs;
	int pending;
	unsigned long flags;

	IFNULLRET(bcs);
	IFNULLRET(bcs->cs);
	ucs = bcs->cs->hw.bas;
	IFNULLRET(ucs);

	check_pending(ucs);

	spin_lock_irqsave(&ucs->lock, flags);
	pending = ucs->pending;
	ucs->pending = 0;
	spin_unlock_irqrestore(&ucs->lock, flags);

	switch (pending) {
	case 0:					/* no pending request */
		dbg(DEBUG_USBREQ, "%s: no request pending", __func__);
		break;

	case HD_OPEN_ATCHANNEL:
		err("timeout opening AT channel");
		error_reset(bcs->cs);
		break;

	case HD_OPEN_B2CHANNEL:
	case HD_OPEN_B1CHANNEL:
		err("timeout opening channel %d", bcs->channel + 1);
		error_hangup(bcs);
		break;

	case HD_CLOSE_ATCHANNEL:
		err("timeout closing AT channel");
		//wake_up_interruptible(cs->initwait);
		//FIXME need own wait queue?
		break;

	case HD_CLOSE_B2CHANNEL:
	case HD_CLOSE_B1CHANNEL:
		err("timeout closing channel %d", bcs->channel + 1);
		break;

	default:
		warn("request 0x%02x timed out, clearing", pending);
	}
}

/* write_ctrl_callback
 * USB completion handler for control pipe output
 * called by the USB subsystem in interrupt context
 * parameter:
 *	urb	USB request block of completed request
 *		urb->context = hardware specific controller state structure
 */
static void write_ctrl_callback(struct urb *urb, struct pt_regs *regs)
{
	struct bas_cardstate *ucs;
	unsigned long flags;

	IFNULLRET(urb);
	IFNULLRET(urb->context);
	IFNULLRET(cardstate);

	ucs = (struct bas_cardstate *) urb->context;
	spin_lock_irqsave(&ucs->lock, flags);
	if (urb->status && ucs->pending) {
		err("control request 0x%02x failed: %s",
		    ucs->pending, get_usb_statmsg(urb->status));
		del_timer(&ucs->timer_ctrl);
		ucs->pending = 0;
	}
	/* individual handling of specific request types */
	switch (ucs->pending) {
	case HD_DEVICE_INIT_ACK:		/* no reply expected */
		ucs->pending = 0;
		break;
	}
	spin_unlock_irqrestore(&ucs->lock, flags);
}

/* req_submit
 * submit a control output request without message buffer to the Gigaset base
 * and optionally start a timeout
 * parameters:
 *	bcs	B channel control structure
 *	req	control request code (HD_*)
 *	val	control request parameter value (set to 0 if unused)
 *	timeout	timeout in seconds (0: no timeout)
 * return value:
 *	0 on success
 *	-EINVAL if a NULL pointer is encountered somewhere
 *	-EBUSY if another request is pending
 *	any URB submission error code
 */
static int req_submit(struct bc_state *bcs, int req, int val, int timeout)
{
	struct bas_cardstate *ucs;
	int ret;
	unsigned long flags;

	IFNULLRETVAL(bcs, -EINVAL);
	IFNULLRETVAL(bcs->cs, -EINVAL);
	ucs = bcs->cs->hw.bas;
	IFNULLRETVAL(ucs, -EINVAL);
	IFNULLRETVAL(ucs->urb_ctrl, -EINVAL);

	dbg(DEBUG_USBREQ, "-------> 0x%02x (%d)", req, val);

	spin_lock_irqsave(&ucs->lock, flags);
	if (ucs->pending) {
		spin_unlock_irqrestore(&ucs->lock, flags);
		err("submission of request 0x%02x failed: request 0x%02x still pending",
		    req, ucs->pending);
		return -EBUSY;
	}
	if (ucs->urb_ctrl->status == -EINPROGRESS) {
		spin_unlock_irqrestore(&ucs->lock, flags);
		err("could not submit request 0x%02x: URB busy", req);
		return -EBUSY;
	}

	ucs->dr_ctrl.bRequestType = OUT_VENDOR_REQ;
	ucs->dr_ctrl.bRequest = req;
	ucs->dr_ctrl.wValue = cpu_to_le16(val);
	ucs->dr_ctrl.wIndex = 0;
	ucs->dr_ctrl.wLength = 0;
	usb_fill_control_urb(ucs->urb_ctrl, ucs->udev,
                             usb_sndctrlpipe(ucs->udev, 0),
                             (unsigned char*) &ucs->dr_ctrl, NULL, 0,
                             write_ctrl_callback, ucs);
	if ((ret = usb_submit_urb(ucs->urb_ctrl, SLAB_ATOMIC)) != 0) {
		err("could not submit request 0x%02x: %s",
		    req, get_usb_statmsg(ret));
		spin_unlock_irqrestore(&ucs->lock, flags);
		return ret;
	}
	ucs->pending = req;

	if (timeout > 0) {
		dbg(DEBUG_USBREQ, "setting timeout of %d/10 secs", timeout);
		ucs->timer_ctrl.expires = jiffies + timeout * HZ / 10;
		ucs->timer_ctrl.data = (unsigned long) bcs;
		ucs->timer_ctrl.function = req_timeout;
		add_timer(&ucs->timer_ctrl);
	}

	spin_unlock_irqrestore(&ucs->lock, flags);
	return 0;
}

/* gigaset_init_bchannel
 * called by common.c to connect a B channel
 * initialize isochronous I/O and tell the Gigaset base to open the channel
 * argument:
 *	B channel control structure
 * return value:
 *	0 on success, error code < 0 on error
 */
static int gigaset_init_bchannel(struct bc_state *bcs)
{
	int req, ret;

	IFNULLRETVAL(bcs, -EINVAL);

	if ((ret = starturbs(bcs)) < 0) {
		err("could not start isochronous I/O for channel %d",
		    bcs->channel + 1);
		error_hangup(bcs);
		return ret;
	}

	req = bcs->channel ? HD_OPEN_B2CHANNEL : HD_OPEN_B1CHANNEL;
	if ((ret = req_submit(bcs, req, 0, BAS_TIMEOUT)) < 0) {
		err("could not open channel %d: %s",
		    bcs->channel + 1, get_usb_statmsg(ret));
		stopurbs(bcs->hw.bas);
		error_hangup(bcs);
	}
	return ret;
}

/* gigaset_close_bchannel
 * called by common.c to disconnect a B channel
 * tell the Gigaset base to close the channel
 * stopping isochronous I/O and LL notification will be done when the
 * acknowledgement for the close arrives
 * argument:
 *	B channel control structure
 * return value:
 *	0 on success, error code < 0 on error
 */
static int gigaset_close_bchannel(struct bc_state *bcs)
{
	int req, ret;

	IFNULLRETVAL(bcs, -EINVAL);

	if (!(atomic_read(&bcs->cs->hw.bas->basstate) &
	      (bcs->channel ? BS_B2OPEN : BS_B1OPEN))) {
		/* channel not running: just signal common.c */
		gigaset_bchannel_down(bcs);
		return 0;
	}

	req = bcs->channel ? HD_CLOSE_B2CHANNEL : HD_CLOSE_B1CHANNEL;
	if ((ret = req_submit(bcs, req, 0, BAS_TIMEOUT)) < 0)
		err("could not submit HD_CLOSE_BxCHANNEL request: %s",
		    get_usb_statmsg(ret));
	return ret;
}

/* Device Operations */
/* ================= */

/* complete_cb
 * unqueue first command buffer from queue, waking any sleepers
 * must be called with cs->cmdlock held
 * parameter:
 *	cs	controller state structure
 */
static void complete_cb(struct cardstate *cs)
{
	struct cmdbuf_t *cb;

	IFNULLRET(cs);
	cb = cs->cmdbuf;
	IFNULLRET(cb);

	/* unqueue completed buffer */
	cs->cmdbytes -= cs->curlen;
	dbg(DEBUG_TRANSCMD | DEBUG_LOCKCMD,
	    "write_command: sent %u bytes, %u left",
	    cs->curlen, cs->cmdbytes);
	if ((cs->cmdbuf = cb->next) != NULL) {
		cs->cmdbuf->prev = NULL;
		cs->curlen = cs->cmdbuf->len;
	} else {
		cs->lastcmdbuf = NULL;
		cs->curlen = 0;
	}

	if (cb->wake_tasklet)
		tasklet_schedule(cb->wake_tasklet);

	kfree(cb);
}

static int atwrite_submit(struct cardstate *cs, unsigned char *buf, int len);

/* write_command_callback
 * USB completion handler for AT command transmission
 * called by the USB subsystem in interrupt context
 * parameter:
 *	urb	USB request block of completed request
 *		urb->context = controller state structure
 */
static void write_command_callback(struct urb *urb, struct pt_regs *regs)
{
	struct cardstate *cs;
	unsigned long flags;
	struct bas_cardstate *ucs;

	IFNULLRET(urb);
	cs = (struct cardstate *) urb->context;
	IFNULLRET(cs);
	ucs = cs->hw.bas;
	IFNULLRET(ucs);

	/* check status */
	switch (urb->status) {
	case 0:					/* normal completion */
		break;
	case -ENOENT:			/* canceled */
	case -ECONNRESET:		/* canceled (async) */
	case -EINPROGRESS:		/* pending */
		/* ignore silently */
		dbg(DEBUG_USBREQ,
		    "%s: %s", __func__, get_usb_statmsg(urb->status));
		return;
	default:				/* any failure */
		if (++ucs->retry_cmd_out > BAS_RETRY) {
			warn("command write: %s, giving up after %d retries",
			     get_usb_statmsg(urb->status), ucs->retry_cmd_out);
			break;
		}
		if (cs->cmdbuf == NULL) {
			warn("command write: %s, cannot retry - cmdbuf gone",
			     get_usb_statmsg(urb->status));
			break;
		}
		notice("command write: %s, retry %d",
		       get_usb_statmsg(urb->status), ucs->retry_cmd_out);
		if (atwrite_submit(cs, cs->cmdbuf->buf, cs->cmdbuf->len) >= 0)
			/* resubmitted - bypass regular exit block */
			return;
		/* command send failed, assume base still waiting */
		update_basstate(ucs, BS_ATREADY, 0);
	}

	spin_lock_irqsave(&cs->cmdlock, flags);
	if (cs->cmdbuf != NULL)
		complete_cb(cs);
	spin_unlock_irqrestore(&cs->cmdlock, flags);
}

/* atrdy_timeout
 * timeout routine for AT command transmission
 * argument:
 *	controller state structure
 */
static void atrdy_timeout(unsigned long data)
{
	struct cardstate *cs = (struct cardstate *) data;
	struct bas_cardstate *ucs;

	IFNULLRET(cs);
	ucs = cs->hw.bas;
	IFNULLRET(ucs);

	warn("timeout waiting for HD_READY_SEND_ATDATA");

	/* fake the missing signal - what else can I do? */
	update_basstate(ucs, BS_ATREADY, BS_ATTIMER);
	start_cbsend(cs);
}

/* atwrite_submit
 * submit an HD_WRITE_ATMESSAGE command URB
 * parameters:
 *	cs	controller state structure
 *	buf	buffer containing command to send
 *	len	length of command to send
 * return value:
 *	0 on success
 *	-EFAULT if a NULL pointer is encountered somewhere
 *	-EBUSY if another request is pending
 *	any URB submission error code
 */
static int atwrite_submit(struct cardstate *cs, unsigned char *buf, int len)
{
	struct bas_cardstate *ucs;
	int ret;

	IFNULLRETVAL(cs, -EFAULT);
	ucs = cs->hw.bas;
	IFNULLRETVAL(ucs, -EFAULT);
	IFNULLRETVAL(ucs->urb_cmd_out, -EFAULT);

	dbg(DEBUG_USBREQ, "-------> HD_WRITE_ATMESSAGE (%d)", len);

	if (ucs->urb_cmd_out->status == -EINPROGRESS) {
		err("could not submit HD_WRITE_ATMESSAGE: URB busy");
		return -EBUSY;
	}

	ucs->dr_cmd_out.bRequestType = OUT_VENDOR_REQ;
	ucs->dr_cmd_out.bRequest = HD_WRITE_ATMESSAGE;
	ucs->dr_cmd_out.wValue = 0;
	ucs->dr_cmd_out.wIndex = 0;
	ucs->dr_cmd_out.wLength = cpu_to_le16(len);
	usb_fill_control_urb(ucs->urb_cmd_out, ucs->udev,
			     usb_sndctrlpipe(ucs->udev, 0),
			     (unsigned char*) &ucs->dr_cmd_out, buf, len,
			     write_command_callback, cs);

	if ((ret = usb_submit_urb(ucs->urb_cmd_out, SLAB_ATOMIC)) != 0) {
		err("could not submit HD_WRITE_ATMESSAGE: %s",
		    get_usb_statmsg(ret));
		return ret;
	}

	/* submitted successfully */
	update_basstate(ucs, 0, BS_ATREADY);

	/* start timeout if necessary */
	if (!(atomic_read(&ucs->basstate) & BS_ATTIMER)) {
		dbg(DEBUG_OUTPUT,
		    "setting ATREADY timeout of %d/10 secs", ATRDY_TIMEOUT);
		ucs->timer_atrdy.expires = jiffies + ATRDY_TIMEOUT * HZ / 10;
		ucs->timer_atrdy.data = (unsigned long) cs;
		ucs->timer_atrdy.function = atrdy_timeout;
		add_timer(&ucs->timer_atrdy);
		update_basstate(ucs, BS_ATTIMER, 0);
	}
	return 0;
}

/* start_cbsend
 * start transmission of AT command queue if necessary
 * parameter:
 *	cs		controller state structure
 * return value:
 *	0 on success
 *	error code < 0 on error
 */
static int start_cbsend(struct cardstate *cs)
{
	struct cmdbuf_t *cb;
	struct bas_cardstate *ucs;
	unsigned long flags;
	int rc;
	int retval = 0;

	IFNULLRETVAL(cs, -EFAULT);
	ucs = cs->hw.bas;
	IFNULLRETVAL(ucs, -EFAULT);

	/* check if AT channel is open */
	if (!(atomic_read(&ucs->basstate) & BS_ATOPEN)) {
		dbg(DEBUG_TRANSCMD | DEBUG_LOCKCMD, "AT channel not open");
		rc = req_submit(cs->bcs, HD_OPEN_ATCHANNEL, 0, BAS_TIMEOUT);
		if (rc < 0) {
			err("could not open AT channel");
			/* flush command queue */
			spin_lock_irqsave(&cs->cmdlock, flags);
			while (cs->cmdbuf != NULL)
				complete_cb(cs);
			spin_unlock_irqrestore(&cs->cmdlock, flags);
		}
		return rc;
	}

	/* try to send first command in queue */
	spin_lock_irqsave(&cs->cmdlock, flags);

	while ((cb = cs->cmdbuf) != NULL &&
	       atomic_read(&ucs->basstate) & BS_ATREADY) {
		ucs->retry_cmd_out = 0;
		rc = atwrite_submit(cs, cb->buf, cb->len);
		if (unlikely(rc)) {
			retval = rc;
			complete_cb(cs);
		}
	}

	spin_unlock_irqrestore(&cs->cmdlock, flags);
	return retval;
}

/* gigaset_write_cmd
 * This function is called by the device independent part of the driver
 * to transmit an AT command string to the Gigaset device.
 * It encapsulates the device specific method for transmission over the
 * direct USB connection to the base.
 * The command string is added to the queue of commands to send, and
 * USB transmission is started if necessary.
 * parameters:
 *	cs		controller state structure
 *	buf		command string to send
 *	len		number of bytes to send (max. IF_WRITEBUF)
 *	wake_tasklet	tasklet to run when transmission is completed (NULL if none)
 * return value:
 *	number of bytes queued on success
 *	error code < 0 on error
 */
static int gigaset_write_cmd(struct cardstate *cs,
                             const unsigned char *buf, int len,
                             struct tasklet_struct *wake_tasklet)
{
	struct cmdbuf_t *cb;
	unsigned long flags;
	int status;

	gigaset_dbg_buffer(atomic_read(&cs->mstate) != MS_LOCKED ?
	                     DEBUG_TRANSCMD : DEBUG_LOCKCMD,
	                   "CMD Transmit", len, buf, 0);

	if (!atomic_read(&cs->connected)) {
		err("%s: not connected", __func__);
		return -ENODEV;
	}

	if (len <= 0)
		return 0;			/* nothing to do */

	if (len > IF_WRITEBUF)
		len = IF_WRITEBUF;
	if (!(cb = kmalloc(sizeof(struct cmdbuf_t) + len, GFP_ATOMIC))) {
		err("%s: out of memory", __func__);
		return -ENOMEM;
	}

	memcpy(cb->buf, buf, len);
	cb->len = len;
	cb->offset = 0;
	cb->next = NULL;
	cb->wake_tasklet = wake_tasklet;

	spin_lock_irqsave(&cs->cmdlock, flags);
	cb->prev = cs->lastcmdbuf;
	if (cs->lastcmdbuf)
		cs->lastcmdbuf->next = cb;
	else {
		cs->cmdbuf = cb;
		cs->curlen = len;
	}
	cs->cmdbytes += len;
	cs->lastcmdbuf = cb;
	spin_unlock_irqrestore(&cs->cmdlock, flags);

	status = start_cbsend(cs);

	return status < 0 ? status : len;
}

/* gigaset_write_room
 * tty_driver.write_room interface routine
 * return number of characters the driver will accept to be written via gigaset_write_cmd
 * parameter:
 *	controller state structure
 * return value:
 *	number of characters
 */
static int gigaset_write_room(struct cardstate *cs)
{
	return IF_WRITEBUF;
}

/* gigaset_chars_in_buffer
 * tty_driver.chars_in_buffer interface routine
 * return number of characters waiting to be sent
 * parameter:
 *	controller state structure
 * return value:
 *	number of characters
 */
static int gigaset_chars_in_buffer(struct cardstate *cs)
{
	unsigned long flags;
	unsigned bytes;

	spin_lock_irqsave(&cs->cmdlock, flags);
	bytes = cs->cmdbytes;
	spin_unlock_irqrestore(&cs->cmdlock, flags);

	return bytes;
}

/* gigaset_brkchars
 * implementation of ioctl(GIGASET_BRKCHARS)
 * parameter:
 *	controller state structure
 * return value:
 *	-EINVAL (unimplemented function)
 */
static int gigaset_brkchars(struct cardstate *cs, const unsigned char buf[6])
{
	return -EINVAL;
}


/* Device Initialization/Shutdown */
/* ============================== */

/* Free hardware dependent part of the B channel structure
 * parameter:
 *	bcs	B channel structure
 * return value:
 *	!=0 on success
 */
static int gigaset_freebcshw(struct bc_state *bcs)
{
	if (!bcs->hw.bas)
		return 0;

	if (bcs->hw.bas->isooutbuf)
		kfree(bcs->hw.bas->isooutbuf);
	kfree(bcs->hw.bas);
	bcs->hw.bas = NULL;
	return 1;
}

/* Initialize hardware dependent part of the B channel structure
 * parameter:
 *	bcs	B channel structure
 * return value:
 *	!=0 on success
 */
static int gigaset_initbcshw(struct bc_state *bcs)
{
	int i;
	struct bas_bc_state *ubc;

	bcs->hw.bas = ubc = kmalloc(sizeof(struct bas_bc_state), GFP_KERNEL);
	if (!ubc) {
		err("could not allocate bas_bc_state");
		return 0;
	}

	atomic_set(&ubc->running, 0);
	atomic_set(&ubc->corrbytes, 0);
	spin_lock_init(&ubc->isooutlock);
	for (i = 0; i < BAS_OUTURBS; ++i) {
		ubc->isoouturbs[i].urb = NULL;
		ubc->isoouturbs[i].bcs = bcs;
	}
	ubc->isooutdone = ubc->isooutfree = ubc->isooutovfl = NULL;
	ubc->numsub = 0;
	if (!(ubc->isooutbuf = kmalloc(sizeof(struct isowbuf_t), GFP_KERNEL))) {
		err("could not allocate isochronous output buffer");
		kfree(ubc);
		bcs->hw.bas = NULL;
		return 0;
	}
	tasklet_init(&ubc->sent_tasklet,
	             &write_iso_tasklet, (unsigned long) bcs);

	spin_lock_init(&ubc->isoinlock);
	for (i = 0; i < BAS_INURBS; ++i)
		ubc->isoinurbs[i] = NULL;
	ubc->isoindone = NULL;
	ubc->loststatus = -EINPROGRESS;
	ubc->isoinlost = 0;
	ubc->seqlen = 0;
	ubc->inbyte = 0;
	ubc->inbits = 0;
	ubc->goodbytes = 0;
	ubc->alignerrs = 0;
	ubc->fcserrs = 0;
	ubc->frameerrs = 0;
	ubc->giants = 0;
	ubc->runts = 0;
	ubc->aborts = 0;
	ubc->shared0s = 0;
	ubc->stolen0s = 0;
	tasklet_init(&ubc->rcvd_tasklet,
	             &read_iso_tasklet, (unsigned long) bcs);
	return 1;
}

static void gigaset_reinitbcshw(struct bc_state *bcs)
{
	struct bas_bc_state *ubc = bcs->hw.bas;

	atomic_set(&bcs->hw.bas->running, 0);
	atomic_set(&bcs->hw.bas->corrbytes, 0);
	bcs->hw.bas->numsub = 0;
	spin_lock_init(&ubc->isooutlock);
	spin_lock_init(&ubc->isoinlock);
	ubc->loststatus = -EINPROGRESS;
}

static void gigaset_freecshw(struct cardstate *cs)
{
	struct bas_cardstate *ucs = cs->hw.bas;

	del_timer(&ucs->timer_ctrl);
	del_timer(&ucs->timer_atrdy);
	del_timer(&ucs->timer_cmd_in);

	kfree(cs->hw.bas);
}

static int gigaset_initcshw(struct cardstate *cs)
{
	struct bas_cardstate *ucs;

	cs->hw.bas = ucs = kmalloc(sizeof *ucs, GFP_KERNEL);
	if (!ucs)
		return 0;

	ucs->urb_cmd_in = NULL;
	ucs->urb_cmd_out = NULL;
	ucs->rcvbuf = NULL;
	ucs->rcvbuf_size = 0;

	spin_lock_init(&ucs->lock);
	ucs->pending = 0;

	atomic_set(&ucs->basstate, 0);
	init_timer(&ucs->timer_ctrl);
	init_timer(&ucs->timer_atrdy);
	init_timer(&ucs->timer_cmd_in);

	return 1;
}

/* freeurbs
 * unlink and deallocate all URBs unconditionally
 * caller must make sure that no commands are still in progress
 * parameter:
 *	cs	controller state structure
 */
static void freeurbs(struct cardstate *cs)
{
	struct bas_cardstate *ucs;
	struct bas_bc_state *ubc;
	int i, j;

	IFNULLRET(cs);
	ucs = cs->hw.bas;
	IFNULLRET(ucs);

	for (j = 0; j < 2; ++j) {
		ubc = cs->bcs[j].hw.bas;
		IFNULLCONT(ubc);
		for (i = 0; i < BAS_OUTURBS; ++i)
			if (ubc->isoouturbs[i].urb) {
				usb_kill_urb(ubc->isoouturbs[i].urb);
				dbg(DEBUG_INIT,
				    "%s: isoc output URB %d/%d unlinked",
				    __func__, j, i);
				usb_free_urb(ubc->isoouturbs[i].urb);
				ubc->isoouturbs[i].urb = NULL;
			}
		for (i = 0; i < BAS_INURBS; ++i)
			if (ubc->isoinurbs[i]) {
				usb_kill_urb(ubc->isoinurbs[i]);
				dbg(DEBUG_INIT,
				    "%s: isoc input URB %d/%d unlinked",
				    __func__, j, i);
				usb_free_urb(ubc->isoinurbs[i]);
				ubc->isoinurbs[i] = NULL;
			}
	}
	if (ucs->urb_int_in) {
		usb_kill_urb(ucs->urb_int_in);
		dbg(DEBUG_INIT, "%s: interrupt input URB unlinked", __func__);
		usb_free_urb(ucs->urb_int_in);
		ucs->urb_int_in = NULL;
	}
	if (ucs->urb_cmd_out) {
		usb_kill_urb(ucs->urb_cmd_out);
		dbg(DEBUG_INIT, "%s: command output URB unlinked", __func__);
		usb_free_urb(ucs->urb_cmd_out);
		ucs->urb_cmd_out = NULL;
	}
	if (ucs->urb_cmd_in) {
		usb_kill_urb(ucs->urb_cmd_in);
		dbg(DEBUG_INIT, "%s: command input URB unlinked", __func__);
		usb_free_urb(ucs->urb_cmd_in);
		ucs->urb_cmd_in = NULL;
	}
	if (ucs->urb_ctrl) {
		usb_kill_urb(ucs->urb_ctrl);
		dbg(DEBUG_INIT, "%s: control output URB unlinked", __func__);
		usb_free_urb(ucs->urb_ctrl);
		ucs->urb_ctrl = NULL;
	}
}

/* gigaset_probe
 * This function is called when a new USB device is connected.
 * It checks whether the new device is handled by this driver.
 */
static int gigaset_probe(struct usb_interface *interface,
			 const struct usb_device_id *id)
{
	struct usb_host_interface *hostif;
	struct usb_device *udev = interface_to_usbdev(interface);
	struct cardstate *cs = NULL;
	struct bas_cardstate *ucs = NULL;
	struct bas_bc_state *ubc;
	struct usb_endpoint_descriptor *endpoint;
	int i, j;
	int ret;

	IFNULLRETVAL(udev, -ENODEV);

	dbg(DEBUG_ANY,
	    "%s: Check if device matches .. (Vendor: 0x%x, Product: 0x%x)",
	    __func__, le16_to_cpu(udev->descriptor.idVendor),
	    le16_to_cpu(udev->descriptor.idProduct));

	/* See if the device offered us matches what we can accept */
	if ((le16_to_cpu(udev->descriptor.idVendor)  != USB_GIGA_VENDOR_ID) ||
	    (le16_to_cpu(udev->descriptor.idProduct) != USB_GIGA_PRODUCT_ID &&
	     le16_to_cpu(udev->descriptor.idProduct) != USB_4175_PRODUCT_ID &&
	     le16_to_cpu(udev->descriptor.idProduct) != USB_SX303_PRODUCT_ID &&
	     le16_to_cpu(udev->descriptor.idProduct) != USB_SX353_PRODUCT_ID)) {
		dbg(DEBUG_ANY, "%s: unmatched ID - exiting", __func__);
		return -ENODEV;
	}

	/* set required alternate setting */
	hostif = interface->cur_altsetting;
	if (hostif->desc.bAlternateSetting != 3) {
		dbg(DEBUG_ANY,
		    "%s: wrong alternate setting %d - trying to switch",
		    __func__, hostif->desc.bAlternateSetting);
		if (usb_set_interface(udev, hostif->desc.bInterfaceNumber, 3) < 0) {
			warn("usb_set_interface failed, device %d interface %d altsetting %d",
			     udev->devnum, hostif->desc.bInterfaceNumber,
			     hostif->desc.bAlternateSetting);
			return -ENODEV;
		}
		hostif = interface->cur_altsetting;
	}

	/* Reject application specific interfaces
	 */
	if (hostif->desc.bInterfaceClass != 255) {
		warn("%s: bInterfaceClass == %d",
		     __func__, hostif->desc.bInterfaceClass);
		return -ENODEV;
	}

	info("%s: Device matched (Vendor: 0x%x, Product: 0x%x)",
	     __func__, le16_to_cpu(udev->descriptor.idVendor),
	     le16_to_cpu(udev->descriptor.idProduct));

	cs = gigaset_getunassignedcs(driver);
	if (!cs) {
		err("%s: no free cardstate", __func__);
		return -ENODEV;
	}
	ucs = cs->hw.bas;
	ucs->udev = udev;
	ucs->interface = interface;

	/* allocate URBs:
	 * - one for the interrupt pipe
	 * - three for the different uses of the default control pipe
	 * - three for each isochronous pipe
	 */
	ucs->urb_int_in = usb_alloc_urb(0, SLAB_KERNEL);
	if (!ucs->urb_int_in) {
		err("No free urbs available");
		goto error;
	}
	ucs->urb_cmd_in = usb_alloc_urb(0, SLAB_KERNEL);
	if (!ucs->urb_cmd_in) {
		err("No free urbs available");
		goto error;
	}
	ucs->urb_cmd_out = usb_alloc_urb(0, SLAB_KERNEL);
	if (!ucs->urb_cmd_out) {
		err("No free urbs available");
		goto error;
	}
	ucs->urb_ctrl = usb_alloc_urb(0, SLAB_KERNEL);
	if (!ucs->urb_ctrl) {
		err("No free urbs available");
		goto error;
	}

	for (j = 0; j < 2; ++j) {
		ubc = cs->bcs[j].hw.bas;
		for (i = 0; i < BAS_OUTURBS; ++i) {
			ubc->isoouturbs[i].urb =
				usb_alloc_urb(BAS_NUMFRAMES, SLAB_KERNEL);
			if (!ubc->isoouturbs[i].urb) {
				err("No free urbs available");
				goto error;
			}
		}
		for (i = 0; i < BAS_INURBS; ++i) {
			ubc->isoinurbs[i] =
				usb_alloc_urb(BAS_NUMFRAMES, SLAB_KERNEL);
			if (!ubc->isoinurbs[i]) {
				err("No free urbs available");
				goto error;
			}
		}
	}

	ucs->rcvbuf = NULL;
	ucs->rcvbuf_size = 0;

	/* Fill the interrupt urb and send it to the core */
	endpoint = &hostif->endpoint[0].desc;
	usb_fill_int_urb(ucs->urb_int_in, udev,
	                 usb_rcvintpipe(udev,
	                                (endpoint->bEndpointAddress) & 0x0f),
	                 ucs->int_in_buf, 3, read_int_callback, cs,
	                 endpoint->bInterval);
	ret = usb_submit_urb(ucs->urb_int_in, SLAB_KERNEL);
	if (ret) {
		err("could not submit interrupt URB: %s", get_usb_statmsg(ret));
		goto error;
	}

	/* tell the device that the driver is ready */
	if ((ret = req_submit(cs->bcs, HD_DEVICE_INIT_ACK, 0, 0)) != 0)
		goto error;

	/* tell common part that the device is ready */
	if (startmode == SM_LOCKED)
		atomic_set(&cs->mstate, MS_LOCKED);
	if (!gigaset_start(cs))
		goto error;

	/* save address of controller structure */
	usb_set_intfdata(interface, cs);

	/* set up device sysfs */
	gigaset_init_dev_sysfs(interface);
	return 0;

error:
	freeurbs(cs);
	gigaset_unassign(cs);
	return -ENODEV;
}

/* gigaset_disconnect
 * This function is called when the Gigaset base is unplugged.
 */
static void gigaset_disconnect(struct usb_interface *interface)
{
	struct cardstate *cs;
	struct bas_cardstate *ucs;

	/* clear device sysfs */
	gigaset_free_dev_sysfs(interface);

	cs = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	IFNULLRET(cs);
	ucs = cs->hw.bas;
	IFNULLRET(ucs);

	info("disconnecting GigaSet base");
	gigaset_stop(cs);
	freeurbs(cs);
	kfree(ucs->rcvbuf);
	ucs->rcvbuf = NULL;
	ucs->rcvbuf_size = 0;
	atomic_set(&ucs->basstate, 0);
	gigaset_unassign(cs);
}

static struct gigaset_ops gigops = {
	gigaset_write_cmd,
	gigaset_write_room,
	gigaset_chars_in_buffer,
	gigaset_brkchars,
	gigaset_init_bchannel,
	gigaset_close_bchannel,
	gigaset_initbcshw,
	gigaset_freebcshw,
	gigaset_reinitbcshw,
	gigaset_initcshw,
	gigaset_freecshw,
	gigaset_set_modem_ctrl,
	gigaset_baud_rate,
	gigaset_set_line_ctrl,
	gigaset_isoc_send_skb,
	gigaset_isoc_input,
};

/* bas_gigaset_init
 * This function is called after the kernel module is loaded.
 */
static int __init bas_gigaset_init(void)
{
	int result;

	/* allocate memory for our driver state and intialize it */
	if ((driver = gigaset_initdriver(GIGASET_MINOR, GIGASET_MINORS,
	                               GIGASET_MODULENAME, GIGASET_DEVNAME,
	                               GIGASET_DEVFSNAME, &gigops,
	                               THIS_MODULE)) == NULL)
		goto error;

	/* allocate memory for our device state and intialize it */
	cardstate = gigaset_initcs(driver, 2, 0, 0, cidmode, GIGASET_MODULENAME);
	if (!cardstate)
		goto error;

	/* register this driver with the USB subsystem */
	result = usb_register(&gigaset_usb_driver);
	if (result < 0) {
		err("usb_register failed (error %d)", -result);
		goto error;
	}

	info(DRIVER_AUTHOR);
	info(DRIVER_DESC);
	return 0;

error:	if (cardstate)
		gigaset_freecs(cardstate);
	cardstate = NULL;
	if (driver)
		gigaset_freedriver(driver);
	driver = NULL;
	return -1;
}

/* bas_gigaset_exit
 * This function is called before the kernel module is unloaded.
 */
static void __exit bas_gigaset_exit(void)
{
	gigaset_blockdriver(driver); /* => probe will fail
	                              * => no gigaset_start any more
	                              */

	gigaset_shutdown(cardstate);
	/* from now on, no isdn callback should be possible */

	if (atomic_read(&cardstate->hw.bas->basstate) & BS_ATOPEN) {
		dbg(DEBUG_ANY, "closing AT channel");
		if (req_submit(cardstate->bcs,
		               HD_CLOSE_ATCHANNEL, 0, BAS_TIMEOUT) >= 0) {
			/* successfully submitted - wait for completion */
			//wait_event_interruptible(cs->initwait, !cs->hw.bas->pending);
			//FIXME need own wait queue? wakeup?
		}
	}

	/* deregister this driver with the USB subsystem */
	usb_deregister(&gigaset_usb_driver);
	/* this will call the disconnect-callback */
	/* from now on, no disconnect/probe callback should be running */

	gigaset_freecs(cardstate);
	cardstate = NULL;
	gigaset_freedriver(driver);
	driver = NULL;
}


module_init(bas_gigaset_init);
module_exit(bas_gigaset_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
