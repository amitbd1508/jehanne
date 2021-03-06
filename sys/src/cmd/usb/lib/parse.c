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
#include <u.h>
#include <lib9.h>
#include <thread.h>
#include "usb.h"

int
parsedev(Dev *xd, uint8_t *b, int n)
{
	Usbdev *d;
	DDev *dd;
	char *hd;

	d = xd->usb;
	assert(d != nil);
	dd = (DDev*)b;
	if(usbdebug>1){
		hd = hexstr(b, Ddevlen);
		fprint(2, "%s: parsedev %s: %s\n", argv0, xd->dir, hd);
		free(hd);
	}
	if(dd->bLength < Ddevlen){
		werrstr("short dev descr. (%d < %d)", dd->bLength, Ddevlen);
		return -1;
	}
	if(dd->bDescriptorType != Ddev){
		werrstr("%d is not a dev descriptor", dd->bDescriptorType);
		return -1;
	}
	d->csp = CSP(dd->bDevClass, dd->bDevSubClass, dd->bDevProtocol);
	d->ep[0]->maxpkt = xd->maxpkt = dd->bMaxPacketSize0;
	d->class = dd->bDevClass;
	d->nconf = dd->bNumConfigurations;
	if(d->nconf == 0)
		dprint(2, "%s: %s: no configurations\n", argv0, xd->dir);
	d->vid = GET2(dd->idVendor);
	d->did = GET2(dd->idProduct);
	d->dno = GET2(dd->bcdDev);
	d->vsid = dd->iManufacturer;
	d->psid = dd->iProduct;
	d->ssid = dd->iSerialNumber;
	if(n > Ddevlen && usbdebug>1)
		fprint(2, "%s: %s: parsedev: %d bytes left",
			argv0, xd->dir, n - Ddevlen);
	return Ddevlen;
}

static int
parseiface(Usbdev *d, Conf *c, uint8_t *b, int n, Iface **ipp, Altc **app)
{
	int class, subclass, proto;
	int ifid, altid;
	DIface *dip;
	Iface *ip;

	assert(d != nil && c != nil);
	if(n < Difacelen){
		werrstr("short interface descriptor");
		return -1;
	}
	dip = (DIface *)b;
	ifid = dip->bInterfaceNumber;
	if(ifid < 0 || ifid >= nelem(c->iface)){
		werrstr("bad interface number %d", ifid);
		return -1;
	}
	if(c->iface[ifid] == nil)
		c->iface[ifid] = emallocz(sizeof(Iface), 1);
	ip = c->iface[ifid];
	class = dip->bInterfaceClass;
	subclass = dip->bInterfaceSubClass;
	proto = dip->bInterfaceProtocol;
	ip->csp = CSP(class, subclass, proto);
	if(ip->csp == 0)
		ip->csp = d->csp;
	if(d->csp == 0)				/* use csp from 1st iface */
		d->csp = ip->csp;		/* if device has none */
	if(d->class == 0)
		d->class = class;
	ip->id = ifid;
	if(c == d->conf[0] && ifid == 0)	/* ep0 was already there */
		d->ep[0]->iface = ip;
	altid = dip->bAlternateSetting;
	if(altid < 0 || altid >= nelem(ip->altc)){
		werrstr("bad alternate conf. number %d", altid);
		return -1;
	}
	if(ip->altc[altid] == nil)
		ip->altc[altid] = emallocz(sizeof(Altc), 1);
	*ipp = ip;
	*app = ip->altc[altid];
	return Difacelen;
}

extern Ep* mkep(Usbdev *, int);

static int
parseendpt(Usbdev *d, Conf *c, Iface *ip, Altc *altc, uint8_t *b, int n, Ep **epp)
{
	int i, dir, epid, type, addr;
	Ep *ep;
	DEp *dep;

	assert(d != nil && c != nil && ip != nil && altc != nil);
	if(n < Deplen){
		werrstr("short endpoint descriptor");
		return -1;
	}
	dep = (DEp *)b;
	altc->attrib = dep->bmAttributes;	/* here? */
	altc->interval = dep->bInterval;

	type = dep->bmAttributes & 0x03;
	addr = dep->bEndpointAddress;
	if(addr & 0x80)
		dir = Ein;
	else
		dir = Eout;
	epid = addr & 0xF;	/* default map to 0..15 */
	assert(epid < nelem(d->ep));
	ep = d->ep[epid];
	if(ep == nil){
		ep = mkep(d, epid);
		ep->dir = dir;
	}else if((ep->addr & 0x80) != (addr & 0x80)){
		if(ep->type == type)
			ep->dir = Eboth;
		else {
			/*
			 * resolve conflict when same endpoint number
			 * is used for different input and output types.
			 * map input endpoint to 16..31 and output to 0..15.
			 */
			ep->id = ((ep->addr & 0x80) != 0)<<4 | (ep->addr & 0xF);
			d->ep[ep->id] = ep;
			epid = ep->id ^ 0x10;
			ep = mkep(d, epid);
			ep->dir = dir;
		}
	}
	ep->maxpkt = GET2(dep->wMaxPacketSize);
	ep->ntds = 1 + ((ep->maxpkt >> 11) & 3);
	ep->maxpkt &= 0x7FF;
	ep->addr = addr;
	ep->type = type;
	ep->isotype = (dep->bmAttributes>>2) & 0x03;
	ep->conf = c;
	ep->iface = ip;
	for(i = 0; i < nelem(ip->ep); i++)
		if(ip->ep[i] == nil)
			break;
	if(i == nelem(ip->ep)){
		werrstr("parseendpt: bug: too many end points on interface "
			"with csp %#lux", ip->csp);
		fprint(2, "%s: %r\n", argv0);
		return -1;
	}
	*epp = ip->ep[i] = ep;
	return Dep;
}

static char*
dname(int dtype)
{
	switch(dtype){
	case Ddev:	return "device";
	case Dconf: 	return "config";
	case Dstr: 	return "string";
	case Diface:	return "interface";
	case Dep:	return "endpoint";
	case Dreport:	return "report";
	case Dphysical:	return "phys";
	default:	return "desc";
	}
}

int
parsedesc(Usbdev *d, Conf *c, uint8_t *b, int n)
{
	int	len, nd, tot;
	Iface	*ip;
	Ep 	*ep;
	Altc	*altc;
	char	*hd;

	assert(d != nil && c != nil);
	tot = 0;
	ip = nil;
	ep = nil;
	altc = nil;
	for(nd = 0; nd < nelem(d->ddesc); nd++)
		if(d->ddesc[nd] == nil)
			break;

	while(n > 2 && b[0] != 0 && b[0] <= n){
		len = b[0];
		if(usbdebug>1){
			hd = hexstr(b, len);
			fprint(2, "%s:\t\tparsedesc %s %x[%d] %s\n",
				argv0, dname(b[1]), b[1], b[0], hd);
			free(hd);
		}
		switch(b[1]){
		case Ddev:
		case Dconf:
			werrstr("unexpected descriptor %d", b[1]);
			ddprint(2, "%s\tparsedesc: %r", argv0);
			break;
		case Diface:
			if(parseiface(d, c, b, n, &ip, &altc) < 0){
				ddprint(2, "%s\tparsedesc: %r\n", argv0);
				return -1;
			}
			break;
		case Dep:
			if(ip == nil || altc == nil){
				werrstr("unexpected endpoint descriptor");
				break;
			}
			if(parseendpt(d, c, ip, altc, b, n, &ep) < 0){
				ddprint(2, "%s\tparsedesc: %r\n", argv0);
				return -1;
			}
			break;
		default:
			if(nd == nelem(d->ddesc)){
				fprint(2, "%s: parsedesc: too many "
					"device-specific descriptors for device"
					" %s %s\n",
					argv0, d->vendor, d->product);
				break;
			}
			d->ddesc[nd] = emallocz(sizeof(Desc)+b[0], 0);
			d->ddesc[nd]->iface = ip;
			d->ddesc[nd]->ep = ep;
			d->ddesc[nd]->altc = altc;
			d->ddesc[nd]->conf = c;
			memmove(&d->ddesc[nd]->data, b, len);
			++nd;
		}
		n -= len;
		b += len;
		tot += len;
	}
	return tot;
}

int
parseconf(Usbdev *d, Conf *c, uint8_t *b, int n)
{
	DConf* dc;
	int	l;
	int	nr;
	char	*hd;

	assert(d != nil && c != nil);
	dc = (DConf*)b;
	if(usbdebug>1){
		hd = hexstr(b, Dconflen);
		fprint(2, "%s:\tparseconf  %s\n", argv0, hd);
		free(hd);
	}
	if(dc->bLength < Dconflen){
		werrstr("short configuration descriptor");
		return -1;
	}
	if(dc->bDescriptorType != Dconf){
		werrstr("not a configuration descriptor");
		return -1;
	}
	c->cval = dc->bConfigurationValue;
	c->attrib = dc->bmAttributes;
	c->milliamps = dc->MaxPower*2;
	l = GET2(dc->wTotalLength);
	if(n < l){
		werrstr("truncated configuration info");
		return -1;
	}
	n -= Dconflen;
	b += Dconflen;
	nr = 0;
	if(n > 0 && (nr=parsedesc(d, c, b, n)) < 0)
		return -1;
	n -= nr;
	if(n > 0 && usbdebug>1)
		fprint(2, "%s:\tparseconf: %d bytes left\n", argv0, n);
	return l;
}
