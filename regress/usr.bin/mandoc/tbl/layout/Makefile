# $OpenBSD: Makefile,v 1.9 2025/07/24 17:16:34 schwarze Exp $

REGRESS_TARGETS	 = badspan center complex empty emptycol emptyline font
REGRESS_TARGETS	+= lines lines-nogroff numbers
REGRESS_TARGETS	+= shortlines spacing spacing-nogroff span
LINT_TARGETS	 = badspan complex empty spacing-nogroff
HTML_TARGETS	 = font

# groff-1.23.0 defects:
# - When the layout is completely empty,
#   GNU eqn produces no output whatsoever for the whole table.
# - When there is no horizontal line at the top,
#   vertical lines extend beyond the top of the table.
# - Excessive spacing modifiers in the layout are honoured.

SKIP_GROFF	 = empty lines-nogroff spacing-nogroff

.include <bsd.regress.mk>
