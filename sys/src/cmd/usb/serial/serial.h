/* Copyright (c) 20XX 9front
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
typedef struct Serial Serial;
typedef struct Serialops Serialops;
typedef struct Serialport Serialport;

struct Serialops {
	int	(*seteps)(Serialport*);
	int	(*init)(Serialport*);
	int	(*getparam)(Serialport*);
	int	(*setparam)(Serialport*);
	int	(*clearpipes)(Serialport*);
	int	(*reset)(Serial*, Serialport*);
	int	(*sendlines)(Serialport*);
	int	(*modemctl)(Serialport*, int);
	int	(*setbreak)(Serialport*, int);
	int	(*readstatus)(Serialport*);
	int	(*wait4data)(Serialport*, uint8_t *, int);
	int	(*wait4write)(Serialport*, uint8_t *, int);
};

enum {
	DataBufSz = 8*1024,
	Maxifc = 16,
};

struct Serialport {
	char name[32];
	Serial	*s;		/* device we belong to */
	int	isjtag;

	Dev	*epintr;	/* may not exist */

	Dev	*epin;
	Dev	*epout;

	uint8_t	ctlstate;

	/* serial parameters */
	uint32_t	baud;
	int	stop;
	int	mctl;
	int	parity;
	int	bits;
	int	fifo;
	int	limit;
	int	rts;
	int	cts;
	int	dsr;
	int	dcd;
	int	dtr;
	int	rlsd;

	int64_t	timer;
	int	blocked;	/* for sw flow ctl. BUG: not implemented yet */
	int	nbreakerr;
	int	ring;
	int	nframeerr;
	int	nparityerr;
	int	novererr;
	int	enabled;

	int	interfc;	/* interfc on the device for ftdi */

	Channel *w4data;
	Channel *gotdata;
	int	ndata;
	uint8_t	data[DataBufSz];

	Reqqueue *rq;
	Reqqueue *wq;
};

struct Serial {
	QLock	ql;
	Dev	*dev;		/* usb device*/

	int	type;		/* serial model subtype */
	int	recover;	/* # of non-fatal recovery tries */
	Serialops;

	int	hasepintr;

	int	jtag;		/* index of jtag interface, -1 none */
	int	nifcs;		/* # of serial interfaces, including JTAG */
	Serialport p[Maxifc];
	int	maxrtrans;
	int	maxwtrans;

	int	maxread;
	int	maxwrite;

	int	inhdrsz;
	int	outhdrsz;
	int	baudbase;	/* for special baud base settings, see ftdi */
};

enum {
	/* soft flow control chars */
	CTLS	= 023,
	CTLQ	= 021,
	CtlDTR	= 1,
	CtlRTS	= 2,
};

/*
 * !hget http://lxr.linux.no/source/drivers/usb/serial/pl2303.h|htmlfmt
 * !hget http://lxr.linux.no/source/drivers/usb/serial/pl2303.c|htmlfmt
 */

typedef struct Cinfo Cinfo;
struct Cinfo {
	int	vid;		/* usb vendor id */
	int	did;		/* usb device/product id */

	int	cid;		/* assigned for us */
};

extern int serialdebug;

#define	dsprint	if(serialdebug)fprint

int	serialrecover(Serial *ser, Serialport *p, Dev *ep, char *err);
int	serialreset(Serial *ser);
char	*serdumpst(Serialport *p, char *buf, int bufsz);
Cinfo	*matchid(Cinfo *tab, int vid, int did);
