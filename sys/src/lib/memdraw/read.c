/*
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

Memimage*
readmemimage(int fd)
{
	char hdr[5*12+1];
	int dy;
	uint32_t chan;
	uint32_t l, n;
	int m, j;
	int new, miny, maxy;
	Rectangle r;
	uint8_t *tmp;
	int ldepth, chunk;
	Memimage *i;

	if(jehanne_readn(fd, hdr, 11) != 11){
		jehanne_werrstr("readimage: short header");
		return nil;
	}
	if(jehanne_memcmp(hdr, "compressed\n", 11) == 0)
		return creadmemimage(fd);
	if(jehanne_readn(fd, hdr+11, 5*12-11) != 5*12-11){
		jehanne_werrstr("readimage: short header (2)");
		return nil;
	}

	/*
	 * distinguish new channel descriptor from old ldepth.
	 * channel descriptors have letters as well as numbers,
	 * while ldepths are a single digit formatted as %-11d.
	 */
	new = 0;
	for(m=0; m<10; m++){
		if(hdr[m] != ' '){
			new = 1;
			break;
		}
	}
	if(hdr[11] != ' '){
		jehanne_werrstr("readimage: bad format");
		return nil;
	}
	if(new){
		hdr[11] = '\0';
		if((chan = strtochan(hdr)) == 0){
			jehanne_werrstr("readimage: bad channel string %s", hdr);
			return nil;
		}
	}else{
		ldepth = ((int)hdr[10])-'0';
		if(ldepth<0 || ldepth>3){
			jehanne_werrstr("readimage: bad ldepth %d", ldepth);
			return nil;
		}
		chan = drawld2chan[ldepth];
	}

	r.min.x = jehanne_atoi(hdr+1*12);
	r.min.y = jehanne_atoi(hdr+2*12);
	r.max.x = jehanne_atoi(hdr+3*12);
	r.max.y = jehanne_atoi(hdr+4*12);
	if(r.min.x>r.max.x || r.min.y>r.max.y){
		jehanne_werrstr("readimage: bad rectangle");
		return nil;
	}

	miny = r.min.y;
	maxy = r.max.y;

	l = bytesperline(r, chantodepth(chan));
	i = allocmemimage(r, chan);
	if(i == nil)
		return nil;
	chunk = 32*1024;
	if(chunk < l)
		chunk = l;
	tmp = jehanne_malloc(chunk);
	if(tmp == nil)
		goto Err;
	while(maxy > miny){
		dy = maxy - miny;
		if(dy*l > chunk)
			dy = chunk/l;
		if(dy <= 0){
			jehanne_werrstr("readmemimage: image too wide for buffer");
			goto Err;
		}
		n = dy*l;
		m = jehanne_readn(fd, tmp, n);
		if(m != n){
			jehanne_werrstr("readmemimage: read count %d not %d: %r", m, n);
   Err:
 			freememimage(i);
			jehanne_free(tmp);
			return nil;
		}
		if(!new)	/* an old image: must flip all the bits */
			for(j=0; j<chunk; j++)
				tmp[j] ^= 0xFF;

		if(loadmemimage(i, Rect(r.min.x, miny, r.max.x, miny+dy), tmp, chunk) <= 0)
			goto Err;
		miny += dy;
	}
	jehanne_free(tmp);
	return i;
}
