# $OpenBSD: Makefile.inc,v 1.26 2025/02/14 12:01:58 jsing Exp $

# hppa-specific libcrypto build rules

# aes
CFLAGS+= -DAES_ASM
SSLASM+= aes aes-parisc aes-parisc
# bn
SSLASM+= bn parisc-mont parisc-mont
CFLAGS+= -DOPENSSL_BN_ASM_MONT -DBN_DIV2W
# modes
CFLAGS+= -DGHASH_ASM
SSLASM+= modes ghash-parisc ghash-parisc
# sha
SSLASM+= sha sha1-parisc sha1-parisc
SSLASM+= sha sha512-parisc sha256-parisc

.for dir src dst in ${SSLASM}
SRCS+=	${dst}.S
GENERATED+=${dst}.S
${dst}.S: ${LCRYPTO_SRC}/${dir}/asm/${src}.pl
	/usr/bin/perl \
		${LCRYPTO_SRC}/${dir}/asm/${src}.pl 32 ${.TARGET} > ${.TARGET}
.endfor
