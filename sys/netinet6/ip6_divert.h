/*      $OpenBSD: ip6_divert.h,v 1.25 2025/06/18 17:45:07 bluhm Exp $ */

/*
 * Copyright (c) 2009 Michele Marchetto <michele@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _IP6_DIVERT_H_
#define _IP6_DIVERT_H_

/*
 * Names for divert sysctl objects
 */
#define	DIVERT6CTL_RECVSPACE	1	/* receive buffer space */
#define	DIVERT6CTL_SENDSPACE	2	/* send buffer space */
#define	DIVERT6CTL_STATS	3	/* divert statistics */
#define	DIVERT6CTL_MAXID	4

#define	DIVERT6CTL_NAMES { \
	{ 0, 0 }, \
	{ NULL,	0 }, \
	{ NULL,	0 }, \
	{ "stats",	CTLTYPE_STRUCT } \
}

#ifdef _KERNEL

#include <sys/percpu.h>
#include <netinet/ip_divert.h>

extern struct cpumem *div6counters;

static inline void
div6stat_inc(enum divstat_counters c)
{
	counters_inc(div6counters, c);
}

extern struct	inpcbtable	divb6table;

extern const struct pr_usrreqs divert6_usrreqs;

void	 divert6_init(void);
void	 divert6_packet(struct mbuf *, int, u_int16_t);
int	 divert6_sysctl(int *, u_int, void *, size_t *, void *, size_t);
int	 divert6_attach(struct socket *, int, int);
int	 divert6_send(struct socket *, struct mbuf *, struct mbuf *,
	     struct mbuf *);
#endif /* _KERNEL */

#endif /* _IP6_DIVERT_H_ */
