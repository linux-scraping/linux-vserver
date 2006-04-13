/*
 * USB driver for Gigaset 307x directly or using M105 Data.
 *
 * Copyright (c) 2001 by Stefan Eilers <Eilers.Stefan@epost.de>
 *                   and Hansjoerg Lipp <hjlipp@web.de>.
 *
 * This driver was derived from the USB skeleton driver by
 * Greg Kroah-Hartman <greg@kroah.com>
 *
 * =====================================================================
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation; either version 2 of
 *	the License, or (at your option) any later version.
 * =====================================================================
 * ToDo: ...
 * =====================================================================
 * Version: $Id: usb-gigaset.c,v 1.85.4.18 2006/02/04 18:28:16 hjlipp Exp $
 * =====================================================================
 */

#include "gigaset.h"

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

/* Version Information */
#define DRIVER_AUTHOR "Hansjoerg Lipp <hjlipp@web.de>, Stefan Eilers <Eilers.Stefan@epost.de>"
#define DRIVER_DESC "USB Driver for Gigaset 307x using M105"

/* Module parameters */

static int startmode = SM_ISDN;
static int cidmode = 1;

module_param(startmode, int, S_IRUGO);
module_param(cidmode, int, S_IRUGO);
MODULE_PARM_DESC(startmode, "start in isdn4linux mode");
MODULE_PARM_DESC(cidmode, "Call-ID mode");

#define GIGASET_MINORS     1
#define GIGASET_MINOR      8
#define GIGASET_MODULENAME "usb_gigaset"
#define GIGASET_DEVFSNAME  "gig/usb/"
#define GIGASET_DEVNAME    "ttyGU"

#define IF_WRITEBUF 2000 //FIXME  // WAKEUP_CHARS: 256

/* Values for the Gigaset M105 Data */
#define USB_M105_VENDOR_ID	0x0681
#define USB_M105_PRODUCT_ID	0x0009

/* table of devices that work with this driver */
static struct usb_device_id gigaset_table [] = {
	{ USB_DEVICE(USB_M105_VENDOR_ID, USB_M105_PRODUCT_ID) },
	{ }					/* Terminating entry */
};

MODULE_DEVICE_TABLE(usb, gigaset_table);

/* Get a minor range for your devices from the usb maintainer */
#define USB_SKEL_MINOR_BASE	200


/*
 * Control requests (empty fields: 00)
 *
 *       RT|RQ|VALUE|INDEX|LEN  |DATA
 * In:
 *       C1 08             01
 *            Get flags (1 byte). Bits: 0=dtr,1=rts,3-7:?
 *       C1 0F             ll ll
 *            Get device information/status (llll: 0x200 and 0x40 seen).
 *            Real size: I only saw MIN(llll,0x64).
 *            Contents: seems to be always the same...
 *              offset 0x00: Length of this structure (0x64) (len: 1,2,3 bytes)
 *              offset 0x3c: String (16 bit chars): "MCCI USB Serial V2.0"
 *              rest:        ?
 * Out:
 *       41 11
 *            Initialize/reset device ?
 *       41 00 xx 00
 *            ? (xx=00 or 01; 01 on start, 00 on close)
 *       41 07 vv mm
 *            Set/clear flags vv=value, mm=mask (see RQ 08)
 *       41 12 xx
 *            Used before the following configuration requests are issued
 *            (with xx=0x0f). I've seen other values<0xf, though.
 *       41 01 xx xx
 *            Set baud rate. xxxx=ceil(0x384000/rate)=trunc(0x383fff/rate)+1.
 *       41 03 ps bb
 *            Set byte size and parity. p:  0x20=even,0x10=odd,0x00=no parity
 *                                     [    0x30: m, 0x40: s           ]
 *                                     [s:  0: 1 stop bit; 1: 1.5; 2: 2]
 *                                      bb: bits/byte (seen 7 and 8)
 *       41 13 -- -- -- -- 10 00 ww 00 00 00 xx 00 00 00 yy 00 00 00 zz 00 00 00
 *            ??
 *            Initialization: 01, 40, 00, 00
 *            Open device:    00  40, 00, 00
 *            yy and zz seem to be equal, either 0x00 or 0x0a
 *            (ww,xx) pairs seen: (00,00), (00,40), (01,40), (09,80), (19,80)
 *       41 19 -- -- -- -- 06 00 00 00 00 xx 11 13
 *            Used after every "configuration sequence" (RQ 12, RQs 01/03/13).
 *            xx is usually 0x00 but was 0x7e before starting data transfer
 *            in unimodem mode. So, this might be an array of characters that need
 *            special treatment ("commit all bufferd data"?), 11=^Q, 13=^S.
 *
 * Unimodem mode: use "modprobe ppp_async flag_time=0" as the device _needs_ two
 * flags per packet.
 */

static int gigaset_probe(struct usb_interface *interface,
                         const struct usb_device_id *id);
static void gigaset_disconnect(struct usb_interface *interface);

static struct gigaset_driver *driver = NULL;
static struct cardstate *cardstate = NULL;

/* usb specific object needed to register this driver with the usb subsystem */
static struct usb_driver gigaset_usb_driver = {
	.name =         GIGASET_MODULENAME,
	.probe =        gigaset_probe,
	.disconnect =   gigaset_disconnect,
	.id_table =     gigaset_table,
};

struct usb_cardstate {
	struct usb_device       *udev;                  /* save off the usb device pointer */
	struct usb_interface    *interface;             /* the interface for this device */
	atomic_t                busy;                   /* bulk output in progress */

	/* Output buffer for commands (M105: and data)*/
	unsigned char           *bulk_out_buffer;       /* the buffer to send data */
	int                     bulk_out_size;          /* the size of the send buffer */
	__u8                    bulk_out_endpointAddr;  /* the address of the bulk out endpoint */
	struct urb              *bulk_out_urb;          /* the urb used to transmit data */

	/* Input buffer for command responses (M105: and data)*/
	int                     rcvbuf_size;            /* the size of the receive buffer */
	struct urb              *read_urb;              /* the urb used to receive data */
	__u8                    int_in_endpointAddr;    /* the address of the bulk in endpoint */

	char                    bchars[6];              /* req. 0x19 */
};

struct usb_bc_state {};

static inline unsigned tiocm_to_gigaset(unsigned state)
{
	return ((state & TIOCM_DTR) ? 1 : 0) | ((state & TIOCM_RTS) ? 2 : 0);
}

#ifdef CONFIG_GIGASET_UNDOCREQ
/* WARNING: EXPERIMENTAL! */
static int gigaset_set_modem_ctrl(struct cardstate *cs, unsigned old_state,
                                  unsigned new_state)
{
	unsigned mask, val;
	int r;

	mask = tiocm_to_gigaset(old_state ^ new_state);
	val = tiocm_to_gigaset(new_state);

	dbg(DEBUG_USBREQ, "set flags 0x%02x with mask 0x%02x", val, mask);
	r = usb_control_msg(cs->hw.usb->udev,
	                    usb_sndctrlpipe(cs->hw.usb->udev, 0), 7, 0x41,
	                    (val & 0xff) | ((mask & 0xff) << 8), 0,
	                    NULL, 0, 2000 /*timeout??*/); // don't use this in an interrupt/BH
	if (r < 0)
		return r;
	//..
	return 0;
}

static int set_value(struct cardstate *cs, u8 req, u16 val)
{
	int r, r2;

	dbg(DEBUG_USBREQ, "request %02x (%04x)", (unsigned)req, (unsigned)val);
	r = usb_control_msg(cs->hw.usb->udev,
	                    usb_sndctrlpipe(cs->hw.usb->udev, 0), 0x12, 0x41,
	                    0xf /*?*/, 0,
	                    NULL, 0, 2000 /*?*/); /* no idea, what this does */
	if (r < 0) {
		err("error %d on request 0x12", -r);
		return r;
	}

	r = usb_control_msg(cs->hw.usb->udev,
	                    usb_sndctrlpipe(cs->hw.usb->udev, 0), req, 0x41,
	                    val, 0,
	                    NULL, 0, 2000 /*?*/);
	if (r < 0)
		err("error %d on request 0x%02x", -r, (unsigned)req);

	r2 = usb_control_msg(cs->hw.usb->udev,
	                     usb_sndctrlpipe(cs->hw.usb->udev, 0), 0x19, 0x41,
	                     0, 0, cs->hw.usb->bchars, 6, 2000 /*?*/);
	if (r2 < 0)
		err("error %d on request 0x19", -r2);

	return r < 0 ? r : (r2 < 0 ? r2 : 0);
}

/* WARNING: HIGHLY EXPERIMENTAL! */
// don't use this in an interrupt/BH
static int gigaset_baud_rate(struct cardstate *cs, unsigned cflag)
{
	u16 val;
	u32 rate;

	cflag &= CBAUD;

	switch (cflag) {
	//FIXME more values?
	case    B300: rate =     300; break;
	case    B600: rate =     600; break;
	case   B1200: rate =    1200; break;
	case   B2400: rate =    2400; break;
	case   B4800: rate =    4800; break;
	case   B9600: rate =    9600; break;
	case  B19200: rate =   19200; break;
	case  B38400: rate =   38400; break;
	case  B57600: rate =   57600; break;
	case B115200: rate =  115200; break;
	default:
		rate =  9600;
		err("unsupported baudrate request 0x%x,"
		    " using default of B9600", cflag);
	}

	val = 0x383fff / rate + 1;

	return set_value(cs, 1, val);
}

/* WARNING: HIGHLY EXPERIMENTAL! */
// don't use this in an interrupt/BH
static int gigaset_set_line_ctrl(struct cardstate *cs, unsigned cflag)
{
	u16 val = 0;

	/* set the parity */
	if (cflag & PARENB)
		val |= (cflag & PARODD) ? 0x10 : 0x20;

	/* set the number of data bits */
	switch (cflag & CSIZE) {
	case CS5:
		val |= 5 << 8; break;
	case CS6:
		val |= 6 << 8; break;
	case CS7:
		val |= 7 << 8; break;
	case CS8:
		val |= 8 << 8; break;
	default:
		err("CSIZE was not CS5-CS8, using default of 8");
		val |= 8 << 8;
		break;
	}

	/* set the number of stop bits */
	if (cflag & CSTOPB) {
		if ((cflag & CSIZE) == CS5)
			val |= 1; /* 1.5 stop bits */ //FIXME is this okay?
		else
			val |= 2; /* 2 stop bits */
	}

	return set_value(cs, 3, val);
}

#else
static int gigaset_set_modem_ctrl(struct cardstate *cs, unsigned old_state,
                                  unsigned new_state)
{
	return -EINVAL;
}

static int gigaset_set_line_ctrl(struct cardstate *cs, unsigned cflag)
{
	return -EINVAL;
}

static int gigaset_baud_rate(struct cardstate *cs, unsigned cflag)
{
	return -EINVAL;
}
#endif


 /*================================================================================================================*/
static int gigaset_init_bchannel(struct bc_state *bcs)
{
	/* nothing to do for M10x */
	gigaset_bchannel_up(bcs);
	return 0;
}

static int gigaset_close_bchannel(struct bc_state *bcs)
{
	/* nothing to do for M10x */
	gigaset_bchannel_down(bcs);
	return 0;
}

//void send_ack_to_LL(void *data);
static int write_modem(struct cardstate *cs);
static int send_cb(struct cardstate *cs, struct cmdbuf_t *cb);


/* Handling of send queue. If there is already a skb opened, put data to
 * the transfer buffer by calling "write_modem". Otherwise take a new skb out of the queue.
 * This function will be called by the ISR via "transmit_chars" (USB: B-Channel Bulk callback handler
 * via immediate task queue) or by writebuf_from_LL if the LL wants to transmit data.
 */
static void gigaset_modem_fill(unsigned long data)
{
	struct cardstate *cs = (struct cardstate *) data;
	struct bc_state *bcs = &cs->bcs[0]; /* only one channel */
	struct cmdbuf_t *cb;
	unsigned long flags;
	int again;

	dbg(DEBUG_OUTPUT, "modem_fill");

	if (atomic_read(&cs->hw.usb->busy)) {
		dbg(DEBUG_OUTPUT, "modem_fill: busy");
		return;
	}

	do {
		again = 0;
		if (!bcs->tx_skb) { /* no skb is being sent */
			spin_lock_irqsave(&cs->cmdlock, flags);
			cb = cs->cmdbuf;
			spin_unlock_irqrestore(&cs->cmdlock, flags);
			if (cb) { /* commands to send? */
				dbg(DEBUG_OUTPUT, "modem_fill: cb");
				if (send_cb(cs, cb) < 0) {
					dbg(DEBUG_OUTPUT,
					    "modem_fill: send_cb failed");
					again = 1; /* no callback will be called! */
				}
			} else { /* skbs to send? */
				bcs->tx_skb = skb_dequeue(&bcs->squeue);
				if (bcs->tx_skb)
					dbg(DEBUG_INTR,
					    "Dequeued skb (Adr: %lx)!",
					    (unsigned long) bcs->tx_skb);
			}
		}

		if (bcs->tx_skb) {
			dbg(DEBUG_OUTPUT, "modem_fill: tx_skb");
			if (write_modem(cs) < 0) {
				dbg(DEBUG_OUTPUT,
				    "modem_fill: write_modem failed");
				// FIXME should we tell the LL?
				again = 1; /* no callback will be called! */
			}
		}
	} while (again);
}

/**
 *	gigaset_read_int_callback
 *
 *      It is called if the data was received from the device. This is almost similiar to
 *      the interrupt service routine in the serial device.
 */
static void gigaset_read_int_callback(struct urb *urb, struct pt_regs *regs)
{
	int resubmit = 0;
	int r;
	struct cardstate *cs;
	unsigned numbytes;
	unsigned char *src;
	//unsigned long flags;
	struct inbuf_t *inbuf;

	IFNULLRET(urb);
	inbuf = (struct inbuf_t *) urb->context;
	IFNULLRET(inbuf);
	//spin_lock_irqsave(&inbuf->lock, flags);
	cs = inbuf->cs;
	IFNULLGOTO(cs, exit);
	IFNULLGOTO(cardstate, exit);

	if (!atomic_read(&cs->connected)) {
		err("%s: disconnected", __func__);
		goto exit;
	}

	if (!urb->status) {
		numbytes = urb->actual_length;

		if (numbytes) {
			src = inbuf->rcvbuf;
			if (unlikely(*src))
				warn("%s: There was no leading 0, but 0x%02x!",
				     __func__, (unsigned) *src);
			++src; /* skip leading 0x00 */
			--numbytes;
			if (gigaset_fill_inbuf(inbuf, src, numbytes)) {
				dbg(DEBUG_INTR, "%s-->BH", __func__);
				gigaset_schedule_event(inbuf->cs);
			}
		} else
			dbg(DEBUG_INTR, "Received zero block length");
		resubmit = 1;
	} else {
		/* The urb might have been killed. */
		dbg(DEBUG_ANY, "%s - nonzero read bulk status received: %d",
		    __func__, urb->status);
		if (urb->status != -ENOENT) /* not killed */
			resubmit = 1;
	}
exit:
	//spin_unlock_irqrestore(&inbuf->lock, flags);
	if (resubmit) {
		r = usb_submit_urb(urb, SLAB_ATOMIC);
		if (r)
			err("error %d when resubmitting urb.", -r);
	}
}


/* This callback routine is called when data was transmitted to a B-Channel.
 * Therefore it has to check if there is still data to transmit. This
 * happens by calling modem_fill via task queue.
 *
 */
static void gigaset_write_bulk_callback(struct urb *urb, struct pt_regs *regs)
{
	struct cardstate *cs = (struct cardstate *) urb->context;

	IFNULLRET(cs);
#ifdef CONFIG_GIGASET_DEBUG
	if (!atomic_read(&cs->connected)) {
		err("%s:not connected", __func__);
		return;
	}
#endif
	if (urb->status)
		err("bulk transfer failed (status %d)", -urb->status); /* That's all we can do. Communication problems
		                                                           are handeled by timeouts or network protocols */

	atomic_set(&cs->hw.usb->busy, 0);
	tasklet_schedule(&cs->write_tasklet);
}

static int send_cb(struct cardstate *cs, struct cmdbuf_t *cb)
{
	struct cmdbuf_t *tcb;
	unsigned long flags;
	int count;
	int status = -ENOENT; // FIXME
	struct usb_cardstate *ucs = cs->hw.usb;

	do {
		if (!cb->len) {
			tcb = cb;

			spin_lock_irqsave(&cs->cmdlock, flags);
			cs->cmdbytes -= cs->curlen;
			dbg(DEBUG_OUTPUT, "send_cb: sent %u bytes, %u left",
			    cs->curlen, cs->cmdbytes);
			cs->cmdbuf = cb = cb->next;
			if (cb) {
				cb->prev = NULL;
				cs->curlen = cb->len;
			} else {
				cs->lastcmdbuf = NULL;
				cs->curlen = 0;
			}
			spin_unlock_irqrestore(&cs->cmdlock, flags);

			if (tcb->wake_tasklet)
				tasklet_schedule(tcb->wake_tasklet);
			kfree(tcb);
		}
		if (cb) {
			count = min(cb->len, ucs->bulk_out_size);
			usb_fill_bulk_urb(ucs->bulk_out_urb, ucs->udev,
			                  usb_sndbulkpipe(ucs->udev,
			                     ucs->bulk_out_endpointAddr & 0x0f),
			                  cb->buf + cb->offset, count,
			                  gigaset_write_bulk_callback, cs);

			cb->offset += count;
			cb->len -= count;
			atomic_set(&ucs->busy, 1);
			dbg(DEBUG_OUTPUT, "send_cb: send %d bytes", count);

			status = usb_submit_urb(ucs->bulk_out_urb, SLAB_ATOMIC);
			if (status) {
				atomic_set(&ucs->busy, 0);
				err("could not submit urb (error %d).",
				    -status);
				cb->len = 0; /* skip urb => remove cb+wakeup in next loop cycle */
			}
		}
	} while (cb && status); /* bei Fehler naechster Befehl //FIXME: ist das OK? */

	return status;
}

/* Write string into transbuf and send it to modem.
 */
static int gigaset_write_cmd(struct cardstate *cs, const unsigned char *buf,
                             int len, struct tasklet_struct *wake_tasklet)
{
	struct cmdbuf_t *cb;
	unsigned long flags;

	gigaset_dbg_buffer(atomic_read(&cs->mstate) != MS_LOCKED ?
	                     DEBUG_TRANSCMD : DEBUG_LOCKCMD,
	                   "CMD Transmit", len, buf, 0);

	if (!atomic_read(&cs->connected)) {
		err("%s: not connected", __func__);
		return -ENODEV;
	}

	if (len <= 0)
		return 0;

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

	tasklet_schedule(&cs->write_tasklet);
	return len;
}

static int gigaset_write_room(struct cardstate *cs)
{
	unsigned long flags;
	unsigned bytes;

	spin_lock_irqsave(&cs->cmdlock, flags);
	bytes = cs->cmdbytes;
	spin_unlock_irqrestore(&cs->cmdlock, flags);

	return bytes < IF_WRITEBUF ? IF_WRITEBUF - bytes : 0;
}

static int gigaset_chars_in_buffer(struct cardstate *cs)
{
	return cs->cmdbytes;
}

static int gigaset_brkchars(struct cardstate *cs, const unsigned char buf[6])
{
#ifdef CONFIG_GIGASET_UNDOCREQ
	gigaset_dbg_buffer(DEBUG_USBREQ, "brkchars", 6, buf, 0);
	memcpy(cs->hw.usb->bchars, buf, 6);
	return usb_control_msg(cs->hw.usb->udev,
	                       usb_sndctrlpipe(cs->hw.usb->udev, 0), 0x19, 0x41,
	                       0, 0, &buf, 6, 2000);
#else
	return -EINVAL;
#endif
}

static int gigaset_freebcshw(struct bc_state *bcs)
{
	if (!bcs->hw.usb)
		return 0;
	//FIXME
	kfree(bcs->hw.usb);
	return 1;
}

/* Initialize the b-channel structure */
static int gigaset_initbcshw(struct bc_state *bcs)
{
	bcs->hw.usb = kmalloc(sizeof(struct usb_bc_state), GFP_KERNEL);
	if (!bcs->hw.usb)
		return 0;

	//bcs->hw.usb->trans_flg = READY_TO_TRNSMIT; /* B-Channel ready to transmit */
	return 1;
}

static void gigaset_reinitbcshw(struct bc_state *bcs)
{
}

static void gigaset_freecshw(struct cardstate *cs)
{
	//FIXME
	tasklet_kill(&cs->write_tasklet);
	kfree(cs->hw.usb);
}

static int gigaset_initcshw(struct cardstate *cs)
{
	struct usb_cardstate *ucs;

	cs->hw.usb = ucs =
		kmalloc(sizeof(struct usb_cardstate), GFP_KERNEL);
	if (!ucs)
		return 0;

	ucs->bchars[0] = 0;
	ucs->bchars[1] = 0;
	ucs->bchars[2] = 0;
	ucs->bchars[3] = 0;
	ucs->bchars[4] = 0x11;
	ucs->bchars[5] = 0x13;
	ucs->bulk_out_buffer = NULL;
	ucs->bulk_out_urb = NULL;
	//ucs->urb_cmd_out = NULL;
	ucs->read_urb = NULL;
	tasklet_init(&cs->write_tasklet,
	             &gigaset_modem_fill, (unsigned long) cs);

	return 1;
}

/* Writes the data of the current open skb into the modem.
 * We have to protect against multiple calls until the
 * callback handler () is called , due to the fact that we
 * are just allowed to send data once to an endpoint. Therefore
 * we using "trans_flg" to synchonize ...
 */
static int write_modem(struct cardstate *cs)
{
	int ret;
	int count;
	struct bc_state *bcs = &cs->bcs[0]; /* only one channel */
	struct usb_cardstate *ucs = cs->hw.usb;
	//unsigned long flags;

	IFNULLRETVAL(bcs->tx_skb, -EINVAL);

	dbg(DEBUG_WRITE, "len: %d...", bcs->tx_skb->len);

	ret = -ENODEV;
	IFNULLGOTO(ucs->bulk_out_buffer, error);
	IFNULLGOTO(ucs->bulk_out_urb, error);
	ret = 0;

	if (!bcs->tx_skb->len) {
		dev_kfree_skb_any(bcs->tx_skb);
		bcs->tx_skb = NULL;
		return -EINVAL;
	}

	/* Copy data to bulk out buffer and  // FIXME copying not necessary
	 * transmit data
	 */
	count = min(bcs->tx_skb->len, (unsigned) ucs->bulk_out_size);
	memcpy(ucs->bulk_out_buffer, bcs->tx_skb->data, count);
	skb_pull(bcs->tx_skb, count);

	usb_fill_bulk_urb(ucs->bulk_out_urb, ucs->udev,
	                  usb_sndbulkpipe(ucs->udev,
	                                  ucs->bulk_out_endpointAddr & 0x0f),
	                  ucs->bulk_out_buffer, count,
	                  gigaset_write_bulk_callback, cs);
	atomic_set(&ucs->busy, 1);
	dbg(DEBUG_OUTPUT, "write_modem: send %d bytes", count);

	ret = usb_submit_urb(ucs->bulk_out_urb, SLAB_ATOMIC);
	if (ret) {
		err("could not submit urb (error %d).", -ret);
		atomic_set(&ucs->busy, 0);
	}
	if (!bcs->tx_skb->len) {
		/* skb sent completely */
		gigaset_skb_sent(bcs, bcs->tx_skb); //FIXME also, when ret<0?

		dbg(DEBUG_INTR,
		    "kfree skb (Adr: %lx)!", (unsigned long) bcs->tx_skb);
		dev_kfree_skb_any(bcs->tx_skb);
		bcs->tx_skb = NULL;
	}

	return ret;
error:
	dev_kfree_skb_any(bcs->tx_skb);
	bcs->tx_skb = NULL;
	return ret;

}

static int gigaset_probe(struct usb_interface *interface,
                         const struct usb_device_id *id)
{
	int retval;
	struct usb_device *udev = interface_to_usbdev(interface);
	unsigned int ifnum;
	struct usb_host_interface *hostif;
	struct cardstate *cs = NULL;
	struct usb_cardstate *ucs = NULL;
	//struct usb_interface_descriptor *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	//isdn_ctrl command;
	int buffer_size;
	int alt;
	//unsigned long flags;

	info("%s: Check if device matches .. (Vendor: 0x%x, Product: 0x%x)",
	    __func__, le16_to_cpu(udev->descriptor.idVendor),
	    le16_to_cpu(udev->descriptor.idProduct));

	retval = -ENODEV; //FIXME

	/* See if the device offered us matches what we can accept */
	if ((le16_to_cpu(udev->descriptor.idVendor  != USB_M105_VENDOR_ID)) ||
	    (le16_to_cpu(udev->descriptor.idProduct != USB_M105_PRODUCT_ID)))
		return -ENODEV;

	/* this starts to become ascii art... */
	hostif = interface->cur_altsetting;
	alt = hostif->desc.bAlternateSetting;
	ifnum = hostif->desc.bInterfaceNumber; // FIXME ?

	if (alt != 0 || ifnum != 0) {
		warn("ifnum %d, alt %d", ifnum, alt);
		return -ENODEV;
	}

	/* Reject application specific intefaces
	 *
	 */
	if (hostif->desc.bInterfaceClass != 255) {
		info("%s: Device matched, but iface_desc[%d]->bInterfaceClass==%d !",
		       __func__, ifnum, hostif->desc.bInterfaceClass);
		return -ENODEV;
	}

	info("%s: Device matched ... !", __func__);

	cs = gigaset_getunassignedcs(driver);
	if (!cs) {
		warn("No free cardstate!");
		return -ENODEV;
	}
	ucs = cs->hw.usb;

#if 0
	if (usb_set_configuration(udev, udev->config[0].desc.bConfigurationValue) < 0) {
		warn("set_configuration failed");
		goto error;
	}


	if (usb_set_interface(udev, ifnum/*==0*/, alt/*==0*/) < 0) {
		warn("usb_set_interface failed, device %d interface %d altsetting %d",
		     udev->devnum, ifnum, alt);
		goto error;
	}
#endif

	/* set up the endpoint information */
	/* check out the endpoints */
	/* We will get 2 endpoints: One for sending commands to the device (bulk out) and one to
	 * poll messages from the device(int in).
	 * Therefore we will have an almost similiar situation as with our serial port handler.
	 * If an connection will be established, we will have to create data in/out pipes
	 * dynamically...
	 */

	endpoint = &hostif->endpoint[0].desc;

	buffer_size = le16_to_cpu(endpoint->wMaxPacketSize);
	ucs->bulk_out_size = buffer_size;
	ucs->bulk_out_endpointAddr = endpoint->bEndpointAddress;
	ucs->bulk_out_buffer = kmalloc(buffer_size, GFP_KERNEL);
	if (!ucs->bulk_out_buffer) {
		err("Couldn't allocate bulk_out_buffer");
		retval = -ENOMEM;
		goto error;
	}

	ucs->bulk_out_urb = usb_alloc_urb(0, SLAB_KERNEL);
	if (!ucs->bulk_out_urb) {
		err("Couldn't allocate bulk_out_buffer");
		retval = -ENOMEM;
		goto error;
	}

	endpoint = &hostif->endpoint[1].desc;

	atomic_set(&ucs->busy, 0);
	ucs->udev = udev;
	ucs->interface = interface;

	ucs->read_urb = usb_alloc_urb(0, SLAB_KERNEL);
	if (!ucs->read_urb) {
		err("No free urbs available");
		retval = -ENOMEM;
		goto error;
	}
	buffer_size = le16_to_cpu(endpoint->wMaxPacketSize);
	ucs->rcvbuf_size = buffer_size;
	ucs->int_in_endpointAddr = endpoint->bEndpointAddress;
	cs->inbuf[0].rcvbuf = kmalloc(buffer_size, GFP_KERNEL);
	if (!cs->inbuf[0].rcvbuf) {
		err("Couldn't allocate rcvbuf");
		retval = -ENOMEM;
		goto error;
	}
	/* Fill the interrupt urb and send it to the core */
	usb_fill_int_urb(ucs->read_urb, udev,
	                 usb_rcvintpipe(udev,
	                                endpoint->bEndpointAddress & 0x0f),
	                 cs->inbuf[0].rcvbuf, buffer_size,
	                 gigaset_read_int_callback,
	                 cs->inbuf + 0, endpoint->bInterval);

	retval = usb_submit_urb(ucs->read_urb, SLAB_KERNEL);
	if (retval) {
		err("Could not submit URB!");
		goto error;
	}

	/* tell common part that the device is ready */
	if (startmode == SM_LOCKED)
		atomic_set(&cs->mstate, MS_LOCKED);
	if (!gigaset_start(cs)) {
		tasklet_kill(&cs->write_tasklet);
		retval = -ENODEV; //FIXME
		goto error;
	}

	/* save address of controller structure */
	usb_set_intfdata(interface, cs);

	/* set up device sysfs */
	gigaset_init_dev_sysfs(interface);
	return 0;

error:
	if (ucs->read_urb)
		usb_kill_urb(ucs->read_urb);
	kfree(ucs->bulk_out_buffer);
	if (ucs->bulk_out_urb != NULL)
		usb_free_urb(ucs->bulk_out_urb);
	kfree(cs->inbuf[0].rcvbuf);
	if (ucs->read_urb != NULL)
		usb_free_urb(ucs->read_urb);
	ucs->read_urb = ucs->bulk_out_urb = NULL;
	cs->inbuf[0].rcvbuf = ucs->bulk_out_buffer = NULL;
	gigaset_unassign(cs);
	return retval;
}

/**
 *	skel_disconnect
 */
static void gigaset_disconnect(struct usb_interface *interface)
{
	struct cardstate *cs;
	struct usb_cardstate *ucs;

	cs = usb_get_intfdata(interface);

	/* clear device sysfs */
	gigaset_free_dev_sysfs(interface);

	usb_set_intfdata(interface, NULL);
	ucs = cs->hw.usb;
	usb_kill_urb(ucs->read_urb);
	//info("GigaSet USB device #%d will be disconnected", minor);

	gigaset_stop(cs);

	tasklet_kill(&cs->write_tasklet);

	usb_kill_urb(ucs->bulk_out_urb);  /* FIXME: nur, wenn noetig */
	//usb_kill_urb(ucs->urb_cmd_out);  /* FIXME: nur, wenn noetig */

	kfree(ucs->bulk_out_buffer);
	if (ucs->bulk_out_urb != NULL)
		usb_free_urb(ucs->bulk_out_urb);
	//if(ucs->urb_cmd_out != NULL)
	//	usb_free_urb(ucs->urb_cmd_out);
	kfree(cs->inbuf[0].rcvbuf);
	if (ucs->read_urb != NULL)
		usb_free_urb(ucs->read_urb);
	ucs->read_urb = ucs->bulk_out_urb/*=ucs->urb_cmd_out*/=NULL;
	cs->inbuf[0].rcvbuf = ucs->bulk_out_buffer = NULL;

	gigaset_unassign(cs);
}

static struct gigaset_ops ops = {
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
	gigaset_m10x_send_skb,
	gigaset_m10x_input,
};

/**
 *	usb_gigaset_init
 * This function is called while kernel-module is loaded
 */
static int __init usb_gigaset_init(void)
{
	int result;

	/* allocate memory for our driver state and intialize it */
	if ((driver = gigaset_initdriver(GIGASET_MINOR, GIGASET_MINORS,
	                               GIGASET_MODULENAME, GIGASET_DEVNAME,
	                               GIGASET_DEVFSNAME, &ops,
	                               THIS_MODULE)) == NULL)
		goto error;

	/* allocate memory for our device state and intialize it */
	cardstate = gigaset_initcs(driver, 1, 1, 0, cidmode, GIGASET_MODULENAME);
	if (!cardstate)
		goto error;

	/* register this driver with the USB subsystem */
	result = usb_register(&gigaset_usb_driver);
	if (result < 0) {
		err("usb_gigaset: usb_register failed (error %d)",
		    -result);
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


/**
 *	usb_gigaset_exit
 * This function is called while unloading the kernel-module
 */
static void __exit usb_gigaset_exit(void)
{
	gigaset_blockdriver(driver); /* => probe will fail
	                              * => no gigaset_start any more
	                              */

	gigaset_shutdown(cardstate);
	/* from now on, no isdn callback should be possible */

	/* deregister this driver with the USB subsystem */
	usb_deregister(&gigaset_usb_driver);
	/* this will call the disconnect-callback */
	/* from now on, no disconnect/probe callback should be running */

	gigaset_freecs(cardstate);
	cardstate = NULL;
	gigaset_freedriver(driver);
	driver = NULL;
}


module_init(usb_gigaset_init);
module_exit(usb_gigaset_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

MODULE_LICENSE("GPL");
