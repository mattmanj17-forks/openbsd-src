/*	$OpenBSD: param.h,v 1.7 2025/07/07 18:33:37 kettenis Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	_MACHINE_PARAM_H_
#define	_MACHINE_PARAM_H_

#ifdef _KERNEL
#ifndef _LOCORE
#include <machine/cpu.h>
#endif
#endif

#define	_MACHINE	riscv64
#define	MACHINE		"riscv64"
#define	_MACHINE_ARCH	riscv64
#define	MACHINE_ARCH	"riscv64"
#define	MID_MACHINE	MID_RISCV64

#define	PAGE_SHIFT	12
#define	PAGE_SIZE	(1 << PAGE_SHIFT)
#define	PAGE_MASK	(PAGE_SIZE - 1)

#ifdef _KERNEL

#define	NBPG		PAGE_SIZE		/* bytes/page */
#define	PGSHIFT		PAGE_SHIFT		/* LOG2(PAGE_SIZE) */
#define	PGOFSET		PAGE_MASK		/* byte offset into page */

#define	UPAGES		6			/* pages of u-area */
#define	USPACE		(UPAGES * PAGE_SIZE)	/* total size of u-area */
#define	USPACE_ALIGN	0			/* u-area alignment 0-none */
#define __HAVE_USPACE_GUARD

#define	NMBCLUSTERS	(64 * 1024)		/* max cluster allocation */

#ifndef	MSGBUFSIZE
#define	MSGBUFSIZE	(16 * PAGE_SIZE)	/* default message buffer size */
#endif

#define	STACKALIGNBYTES		(16 - 1)
#define	STACKALIGN(p)		((u_long)(p) &~ STACKALIGNBYTES)

#define __HAVE_FDT

#endif /* _KERNEL */

#endif /* _MACHINE_PARAM_H_ */
