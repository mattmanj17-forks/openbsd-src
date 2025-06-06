/*	$OpenBSD: softintr.c,v 1.11 2025/05/10 09:54:17 visa Exp $	*/
/*	$NetBSD: softintr.c,v 1.2 2003/07/15 00:24:39 lukem Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/atomic.h>
#include <machine/intr.h>

/*
 * Schedule a software interrupt.
 */
void
softintr(int si)
{
	atomic_setbits_int(&curcpu()->ci_ipending, SI_TO_IRQBIT(si));
}

void
dosoftint(int pcpl)
{
	struct cpu_info *ci = curcpu();
	int sir, q, mask;

	ppc_intr_enable(1);
	KERNEL_LOCK();

	while ((sir = (ci->ci_ipending & ppc_smask[pcpl])) != 0) {
		atomic_clearbits_int(&ci->ci_ipending, sir);

		for (q = NSOFTINTR - 1; q >= 0; q--) {
			mask = SI_TO_IRQBIT(q);
			if (sir & mask)
				softintr_dispatch(q);
		}
	}

	KERNEL_UNLOCK();
	(void)ppc_intr_disable();
}
