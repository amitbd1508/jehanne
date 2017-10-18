/*
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

#include "rc.h"
#include "exec.h"
#include "io.h"
#include "fns.h"

void *
emalloc(int n)
{
	void *p = malloc(n);
	if(p==0)
		panic("Can't malloc %d bytes", n);
	return p;
}

void*
erealloc(void *p, int n)
{
	p = realloc(p, n);
	if(p==0 && n!=0)
		panic("Can't realloc %d bytes\n", n);
	return p;
}

char*
estrdup(char *s)
{
	char *d;
	int n;

	n = strlen(s)+1;
	d = emalloc(n);
	memmove(d, s, n);
	return d;
}

extern int lastword, lastdol;

void
yyerror(char *m)
{
	pfmt(err, "rc: ");
	if(runq->cmdfile && !runq->iflag)
		pfmt(err, "%s:%d: ", runq->cmdfile, runq->lineno);
	else if(runq->cmdfile)
		pfmt(err, "%s: ", runq->cmdfile);
	else if(!runq->iflag)
		pfmt(err, "line %d: ", runq->lineno);
	if(tok[0] && tok[0]!='\n')
		pfmt(err, "token %q: ", tok);
	pfmt(err, "%s\n", m);
	flush(err);
	lastword = 0;
	lastdol = 0;
	while(lastc!='\n' && lastc!=EOF) advance();
	nerror++;
	setvar(ENV_STATUS, newword(m, (word *)0));
}
char *bp;

static void
iacvt(int n)
{
	if(n<0){
		*bp++='-';
		n=-n;	/* doesn't work for n==-inf */
	}
	if(n/10)
		iacvt(n/10);
	*bp++=n%10+'0';
}

void
inttoascii(char *s, int n)
{
	bp = s;
	iacvt(n);
	*bp='\0';
}

void
panic(char *s, int n)
{
	pfmt(err, "rc: ");
	pfmt(err, s, n);
	pchr(err, '\n');
	flush(err);
	Abort();
}
