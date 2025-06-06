/* 	$OpenBSD: test_sshkey.c,v 1.30 2025/05/24 06:50:28 dtucker Exp $ */
/*
 * Regress test for sshkey.h key management API
 *
 * Placed in the public domain
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/rsa.h>

#include "test_helper.h"

#include "ssherr.h"
#include "sshbuf.h"
#define SSHBUF_INTERNAL 1	/* access internals for testing */
#include "sshkey.h"

#include "authfile.h"
#include "common.h"
#include "ssh2.h"

void sshkey_tests(void);
void sshkey_benchmarks(void);

static void
put_opt(struct sshbuf *b, const char *name, const char *value)
{
	struct sshbuf *sect;

	sect = sshbuf_new();
	ASSERT_PTR_NE(sect, NULL);
	ASSERT_INT_EQ(sshbuf_put_cstring(b, name), 0);
	if (value != NULL)
		ASSERT_INT_EQ(sshbuf_put_cstring(sect, value), 0);
	ASSERT_INT_EQ(sshbuf_put_stringb(b, sect), 0);
	sshbuf_free(sect);
}

static void
build_cert(struct sshbuf *b, struct sshkey *k, const char *type,
    struct sshkey *sign_key, struct sshkey *ca_key,
    const char *sig_alg)
{
	struct sshbuf *ca_buf, *pk, *principals, *critopts, *exts;
	u_char *sigblob;
	size_t siglen;

	ca_buf = sshbuf_new();
	ASSERT_PTR_NE(ca_buf, NULL);
	ASSERT_INT_EQ(sshkey_putb(ca_key, ca_buf), 0);

	/*
	 * Get the public key serialisation by rendering the key and skipping
	 * the type string. This is a bit of a hack :/
	 */
	pk = sshbuf_new();
	ASSERT_PTR_NE(pk, NULL);
	ASSERT_INT_EQ(sshkey_putb_plain(k, pk), 0);
	ASSERT_INT_EQ(sshbuf_skip_string(pk), 0);

	principals = sshbuf_new();
	ASSERT_PTR_NE(principals, NULL);
	ASSERT_INT_EQ(sshbuf_put_cstring(principals, "gsamsa"), 0);
	ASSERT_INT_EQ(sshbuf_put_cstring(principals, "gregor"), 0);

	critopts = sshbuf_new();
	ASSERT_PTR_NE(critopts, NULL);
	put_opt(critopts, "force-command", "/usr/local/bin/nethack");
	put_opt(critopts, "source-address", "192.168.0.0/24,127.0.0.1,::1");

	exts = sshbuf_new();
	ASSERT_PTR_NE(exts, NULL);
	put_opt(critopts, "permit-X11-forwarding", NULL);

	ASSERT_INT_EQ(sshbuf_put_cstring(b, type), 0);
	ASSERT_INT_EQ(sshbuf_put_cstring(b, "noncenoncenonce!"), 0); /* nonce */
	ASSERT_INT_EQ(sshbuf_putb(b, pk), 0); /* public key serialisation */
	ASSERT_INT_EQ(sshbuf_put_u64(b, 1234), 0); /* serial */
	ASSERT_INT_EQ(sshbuf_put_u32(b, SSH2_CERT_TYPE_USER), 0); /* type */
	ASSERT_INT_EQ(sshbuf_put_cstring(b, "gregor"), 0); /* key ID */
	ASSERT_INT_EQ(sshbuf_put_stringb(b, principals), 0); /* principals */
	ASSERT_INT_EQ(sshbuf_put_u64(b, 0), 0); /* start */
	ASSERT_INT_EQ(sshbuf_put_u64(b, 0xffffffffffffffffULL), 0); /* end */
	ASSERT_INT_EQ(sshbuf_put_stringb(b, critopts), 0); /* options */
	ASSERT_INT_EQ(sshbuf_put_stringb(b, exts), 0); /* extensions */
	ASSERT_INT_EQ(sshbuf_put_string(b, NULL, 0), 0); /* reserved */
	ASSERT_INT_EQ(sshbuf_put_stringb(b, ca_buf), 0); /* signature key */
	ASSERT_INT_EQ(sshkey_sign(sign_key, &sigblob, &siglen,
	    sshbuf_ptr(b), sshbuf_len(b), sig_alg, NULL, NULL, 0), 0);
	ASSERT_INT_EQ(sshbuf_put_string(b, sigblob, siglen), 0); /* signature */

	free(sigblob);
	sshbuf_free(ca_buf);
	sshbuf_free(exts);
	sshbuf_free(critopts);
	sshbuf_free(principals);
	sshbuf_free(pk);
}

static void
signature_test(struct sshkey *k, struct sshkey *bad, const char *sig_alg,
    const u_char *d, size_t l)
{
	size_t len;
	u_char *sig;

	ASSERT_INT_EQ(sshkey_sign(k, &sig, &len, d, l, sig_alg,
	    NULL, NULL, 0), 0);
	ASSERT_SIZE_T_GT(len, 8);
	ASSERT_PTR_NE(sig, NULL);
	ASSERT_INT_EQ(sshkey_verify(k, sig, len, d, l, NULL, 0, NULL), 0);
	ASSERT_INT_NE(sshkey_verify(bad, sig, len, d, l, NULL, 0, NULL), 0);
	/* Fuzz test is more comprehensive, this is just a smoke test */
	sig[len - 5] ^= 0x10;
	ASSERT_INT_NE(sshkey_verify(k, sig, len, d, l, NULL, 0, NULL), 0);
	free(sig);
}

static void
signature_bench(const char *name, int ktype, int bits, const char *sig_alg,
    const u_char *d, size_t l)
{
	struct sshkey *k;
	size_t len;
	u_char *sig;
	char testname[256];

	snprintf(testname, sizeof(testname), "sign %s", name);
	TEST_START(testname);
	ASSERT_INT_EQ(sshkey_generate(ktype, bits, &k), 0);
	ASSERT_PTR_NE(k, NULL);

	BENCH_START(testname);
	ASSERT_INT_EQ(sshkey_sign(k, &sig, &len, d, l, sig_alg,
	    NULL, NULL, 0), 0);
	free(sig);
	BENCH_FINISH("sign");

	sshkey_free(k);
	TEST_DONE();
}

static void
verify_bench(const char *name, int ktype, int bits, const char *sig_alg,
    const u_char *d, size_t l)
{
	struct sshkey *k;
	size_t len;
	u_char *sig;
	char testname[256];

	snprintf(testname, sizeof(testname), "verify %s", name);
	TEST_START(testname);
	ASSERT_INT_EQ(sshkey_generate(ktype, bits, &k), 0);
	ASSERT_PTR_NE(k, NULL);

	ASSERT_INT_EQ(sshkey_sign(k, &sig, &len, d, l, sig_alg,
	    NULL, NULL, 0), 0);
	BENCH_START(testname);
	ASSERT_INT_EQ(sshkey_verify(k, sig, len, d, l, NULL, 0, NULL), 0);
	BENCH_FINISH("verify");

	free(sig);
	sshkey_free(k);
	TEST_DONE();
}

static void
banana(u_char *s, size_t l)
{
	size_t o;
	const u_char the_banana[] = { 'b', 'a', 'n', 'a', 'n', 'a' };

	for (o = 0; o < l; o += sizeof(the_banana)) {
		if (l - o < sizeof(the_banana)) {
			memcpy(s + o, "nanananana", l - o);
			break;
		}
		memcpy(s + o, the_banana, sizeof(the_banana));
	}
}

static void
signature_tests(struct sshkey *k, struct sshkey *bad, const char *sig_alg)
{
	u_char i, buf[2049];
	size_t lens[] = {
		1, 2, 7, 8, 9, 15, 16, 17, 31, 32, 33, 127, 128, 129,
		255, 256, 257, 1023, 1024, 1025, 2047, 2048, 2049
	};

	for (i = 0; i < (sizeof(lens)/sizeof(lens[0])); i++) {
		test_subtest_info("%s key, banana length %zu",
		    sshkey_type(k), lens[i]);
		banana(buf, lens[i]);
		signature_test(k, bad, sig_alg, buf, lens[i]);
	}
}

static void
signature_benchmark(const char *name, int ktype, int bits,
    const char *sig_alg, int bench_verify)
{
	u_char buf[256];

	banana(buf, sizeof(buf));
	if (bench_verify)
		verify_bench(name, ktype, bits, sig_alg, buf, sizeof(buf));
	else
		signature_bench(name, ktype, bits, sig_alg, buf, sizeof(buf));
}

static struct sshkey *
get_private(const char *n)
{
	struct sshbuf *b;
	struct sshkey *ret;

	b = load_file(n);
	ASSERT_INT_EQ(sshkey_parse_private_fileblob(b, "", &ret, NULL), 0);
	sshbuf_free(b);
	return ret;
}

void
sshkey_tests(void)
{
	struct sshkey *k1 = NULL, *k2 = NULL, *k3 = NULL, *k4 = NULL;
	struct sshkey *kr = NULL, *kd = NULL, *ke = NULL, *kf = NULL;
	struct sshbuf *b = NULL;

	TEST_START("new invalid");
	k1 = sshkey_new(-42);
	ASSERT_PTR_EQ(k1, NULL);
	TEST_DONE();

	TEST_START("new/free KEY_UNSPEC");
	k1 = sshkey_new(KEY_UNSPEC);
	ASSERT_PTR_NE(k1, NULL);
	sshkey_free(k1);
	TEST_DONE();

	TEST_START("new/free KEY_RSA");
	k1 = sshkey_new(KEY_RSA);
	ASSERT_PTR_NE(k1, NULL);
	ASSERT_PTR_NE(k1->pkey, NULL);
	sshkey_free(k1);
	TEST_DONE();

	TEST_START("new/free KEY_ECDSA");
	k1 = sshkey_new(KEY_ECDSA);
	ASSERT_PTR_NE(k1, NULL);
	ASSERT_PTR_EQ(k1->pkey, NULL);  /* Can't allocate without NID */
	sshkey_free(k1);
	TEST_DONE();

	TEST_START("new/free KEY_ED25519");
	k1 = sshkey_new(KEY_ED25519);
	ASSERT_PTR_NE(k1, NULL);
	/* These should be blank until key loaded or generated */
	ASSERT_PTR_EQ(k1->ed25519_sk, NULL);
	ASSERT_PTR_EQ(k1->ed25519_pk, NULL);
	sshkey_free(k1);
	TEST_DONE();

	TEST_START("generate KEY_RSA too small modulus");
	ASSERT_INT_EQ(sshkey_generate(KEY_RSA, 128, &k1),
	    SSH_ERR_KEY_LENGTH);
	ASSERT_PTR_EQ(k1, NULL);
	TEST_DONE();

	TEST_START("generate KEY_RSA too large modulus");
	ASSERT_INT_EQ(sshkey_generate(KEY_RSA, 1 << 20, &k1),
	    SSH_ERR_KEY_LENGTH);
	ASSERT_PTR_EQ(k1, NULL);
	TEST_DONE();


	TEST_START("generate KEY_ECDSA wrong bits");
	ASSERT_INT_EQ(sshkey_generate(KEY_ECDSA, 42, &k1),
	    SSH_ERR_KEY_LENGTH);
	ASSERT_PTR_EQ(k1, NULL);
	sshkey_free(k1);
	TEST_DONE();

	TEST_START("generate KEY_RSA");
	ASSERT_INT_EQ(sshkey_generate(KEY_RSA, 767, &kr),
	    SSH_ERR_KEY_LENGTH);
	ASSERT_INT_EQ(sshkey_generate(KEY_RSA, 1024, &kr), 0);
	ASSERT_PTR_NE(kr, NULL);
	ASSERT_PTR_NE(EVP_PKEY_get0_RSA(kr->pkey), NULL);
	ASSERT_PTR_NE(rsa_n(kr), NULL);
	ASSERT_PTR_NE(rsa_e(kr), NULL);
	ASSERT_PTR_NE(rsa_p(kr), NULL);
	ASSERT_INT_EQ(BN_num_bits(rsa_n(kr)), 1024);
	TEST_DONE();


	TEST_START("generate KEY_ECDSA");
	ASSERT_INT_EQ(sshkey_generate(KEY_ECDSA, 256, &ke), 0);
	ASSERT_PTR_NE(ke, NULL);
	ASSERT_PTR_NE(EVP_PKEY_get0_EC_KEY(ke->pkey), NULL);
	ASSERT_PTR_NE(EC_KEY_get0_public_key(EVP_PKEY_get0_EC_KEY(ke->pkey)),
	    NULL);
	ASSERT_PTR_NE(EC_KEY_get0_private_key(EVP_PKEY_get0_EC_KEY(ke->pkey)),
	    NULL);
	TEST_DONE();

	TEST_START("generate KEY_ED25519");
	ASSERT_INT_EQ(sshkey_generate(KEY_ED25519, 256, &kf), 0);
	ASSERT_PTR_NE(kf, NULL);
	ASSERT_INT_EQ(kf->type, KEY_ED25519);
	ASSERT_PTR_NE(kf->ed25519_pk, NULL);
	ASSERT_PTR_NE(kf->ed25519_sk, NULL);
	TEST_DONE();

	TEST_START("demote KEY_RSA");
	ASSERT_INT_EQ(sshkey_from_private(kr, &k1), 0);
	ASSERT_PTR_NE(k1, NULL);
	ASSERT_PTR_NE(kr, k1);
	ASSERT_INT_EQ(k1->type, KEY_RSA);
	ASSERT_PTR_NE(EVP_PKEY_get0_RSA(k1->pkey), NULL);
	ASSERT_PTR_NE(rsa_n(k1), NULL);
	ASSERT_PTR_NE(rsa_e(k1), NULL);
	ASSERT_PTR_EQ(rsa_p(k1), NULL);
	TEST_DONE();

	TEST_START("equal KEY_RSA/demoted KEY_RSA");
	ASSERT_INT_EQ(sshkey_equal(kr, k1), 1);
	sshkey_free(k1);
	TEST_DONE();


	TEST_START("demote KEY_ECDSA");
	ASSERT_INT_EQ(sshkey_from_private(ke, &k1), 0);
	ASSERT_PTR_NE(k1, NULL);
	ASSERT_PTR_NE(ke, k1);
	ASSERT_INT_EQ(k1->type, KEY_ECDSA);
	ASSERT_PTR_NE(EVP_PKEY_get0_EC_KEY(k1->pkey), NULL);
	ASSERT_INT_EQ(k1->ecdsa_nid, ke->ecdsa_nid);
	ASSERT_PTR_NE(EC_KEY_get0_public_key(EVP_PKEY_get0_EC_KEY(ke->pkey)),
	   NULL);
	ASSERT_PTR_EQ(EC_KEY_get0_private_key(EVP_PKEY_get0_EC_KEY(k1->pkey)),
	   NULL);
	TEST_DONE();

	TEST_START("equal KEY_ECDSA/demoted KEY_ECDSA");
	ASSERT_INT_EQ(sshkey_equal(ke, k1), 1);
	sshkey_free(k1);
	TEST_DONE();

	TEST_START("demote KEY_ED25519");
	ASSERT_INT_EQ(sshkey_from_private(kf, &k1), 0);
	ASSERT_PTR_NE(k1, NULL);
	ASSERT_PTR_NE(kf, k1);
	ASSERT_INT_EQ(k1->type, KEY_ED25519);
	ASSERT_PTR_NE(k1->ed25519_pk, NULL);
	ASSERT_PTR_EQ(k1->ed25519_sk, NULL);
	TEST_DONE();

	TEST_START("equal KEY_ED25519/demoted KEY_ED25519");
	ASSERT_INT_EQ(sshkey_equal(kf, k1), 1);
	sshkey_free(k1);
	TEST_DONE();

	TEST_START("equal mismatched key types");
	ASSERT_INT_EQ(sshkey_equal(kd, kr), 0);
	ASSERT_INT_EQ(sshkey_equal(kd, ke), 0);
	ASSERT_INT_EQ(sshkey_equal(kr, ke), 0);
	ASSERT_INT_EQ(sshkey_equal(ke, kf), 0);
	ASSERT_INT_EQ(sshkey_equal(kd, kf), 0);
	TEST_DONE();

	TEST_START("equal different keys");
	ASSERT_INT_EQ(sshkey_generate(KEY_RSA, 1024, &k1), 0);
	ASSERT_INT_EQ(sshkey_equal(kr, k1), 0);
	sshkey_free(k1);
	ASSERT_INT_EQ(sshkey_generate(KEY_ECDSA, 256, &k1), 0);
	ASSERT_INT_EQ(sshkey_equal(ke, k1), 0);
	sshkey_free(k1);
	ASSERT_INT_EQ(sshkey_generate(KEY_ED25519, 256, &k1), 0);
	ASSERT_INT_EQ(sshkey_equal(kf, k1), 0);
	sshkey_free(k1);
	TEST_DONE();

	sshkey_free(kr);
	sshkey_free(kd);
	sshkey_free(ke);
	sshkey_free(kf);

	TEST_START("certify key");
	ASSERT_INT_EQ(sshkey_load_public(test_data_file("ed25519_1.pub"),
	    &k1, NULL), 0);
	k2 = get_private("ed25519_2");
	ASSERT_INT_EQ(sshkey_to_certified(k1), 0);
	ASSERT_PTR_NE(k1->cert, NULL);
	k1->cert->type = SSH2_CERT_TYPE_USER;
	k1->cert->serial = 1234;
	k1->cert->key_id = strdup("estragon");
	ASSERT_PTR_NE(k1->cert->key_id, NULL);
	k1->cert->principals = calloc(4, sizeof(*k1->cert->principals));
	ASSERT_PTR_NE(k1->cert->principals, NULL);
	k1->cert->principals[0] = strdup("estragon");
	k1->cert->principals[1] = strdup("vladimir");
	k1->cert->principals[2] = strdup("pozzo");
	k1->cert->principals[3] = strdup("lucky");
	ASSERT_PTR_NE(k1->cert->principals[0], NULL);
	ASSERT_PTR_NE(k1->cert->principals[1], NULL);
	ASSERT_PTR_NE(k1->cert->principals[2], NULL);
	ASSERT_PTR_NE(k1->cert->principals[3], NULL);
	k1->cert->nprincipals = 4;
	k1->cert->valid_after = 0;
	k1->cert->valid_before = (u_int64_t)-1;
	sshbuf_free(k1->cert->critical);
	k1->cert->critical = sshbuf_new();
	ASSERT_PTR_NE(k1->cert->critical, NULL);
	sshbuf_free(k1->cert->extensions);
	k1->cert->extensions = sshbuf_new();
	ASSERT_PTR_NE(k1->cert->extensions, NULL);
	put_opt(k1->cert->critical, "force-command", "/usr/bin/true");
	put_opt(k1->cert->critical, "source-address", "127.0.0.1");
	put_opt(k1->cert->extensions, "permit-X11-forwarding", NULL);
	put_opt(k1->cert->extensions, "permit-agent-forwarding", NULL);
	ASSERT_INT_EQ(sshkey_from_private(k2, &k1->cert->signature_key), 0);
	ASSERT_INT_EQ(sshkey_certify(k1, k2, NULL, NULL, NULL), 0);
	b = sshbuf_new();
	ASSERT_PTR_NE(b, NULL);
	ASSERT_INT_EQ(sshkey_putb(k1, b), 0);
	ASSERT_INT_EQ(sshkey_from_blob(sshbuf_ptr(b), sshbuf_len(b), &k3), 0);

	sshkey_free(k1);
	sshkey_free(k2);
	sshkey_free(k3);
	sshbuf_reset(b);
	TEST_DONE();

	TEST_START("sign and verify RSA");
	k1 = get_private("rsa_1");
	ASSERT_INT_EQ(sshkey_load_public(test_data_file("rsa_2.pub"), &k2,
	    NULL), 0);
	signature_tests(k1, k2, "ssh-rsa");
	sshkey_free(k1);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("sign and verify RSA-SHA256");
	k1 = get_private("rsa_1");
	ASSERT_INT_EQ(sshkey_load_public(test_data_file("rsa_2.pub"), &k2,
	    NULL), 0);
	signature_tests(k1, k2, "rsa-sha2-256");
	sshkey_free(k1);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("sign and verify RSA-SHA512");
	k1 = get_private("rsa_1");
	ASSERT_INT_EQ(sshkey_load_public(test_data_file("rsa_2.pub"), &k2,
	    NULL), 0);
	signature_tests(k1, k2, "rsa-sha2-512");
	sshkey_free(k1);
	sshkey_free(k2);
	TEST_DONE();


	TEST_START("sign and verify ECDSA");
	k1 = get_private("ecdsa_1");
	ASSERT_INT_EQ(sshkey_load_public(test_data_file("ecdsa_2.pub"), &k2,
	    NULL), 0);
	signature_tests(k1, k2, NULL);
	sshkey_free(k1);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("sign and verify ED25519");
	k1 = get_private("ed25519_1");
	ASSERT_INT_EQ(sshkey_load_public(test_data_file("ed25519_2.pub"), &k2,
	    NULL), 0);
	signature_tests(k1, k2, NULL);
	sshkey_free(k1);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("nested certificate");
	ASSERT_INT_EQ(sshkey_load_cert(test_data_file("rsa_1"), &k1), 0);
	ASSERT_INT_EQ(sshkey_load_public(test_data_file("rsa_1.pub"), &k2,
	    NULL), 0);
	k3 = get_private("rsa_1");
	build_cert(b, k2, "ssh-rsa-cert-v01@openssh.com", k3, k1, NULL);
	ASSERT_INT_EQ(sshkey_from_blob(sshbuf_ptr(b), sshbuf_len(b), &k4),
	    SSH_ERR_KEY_CERT_INVALID_SIGN_KEY);
	ASSERT_PTR_EQ(k4, NULL);
	sshkey_free(k1);
	sshkey_free(k2);
	sshkey_free(k3);
	sshbuf_free(b);
	TEST_DONE();

}

void
sshkey_benchmarks(void)
{
	struct sshkey *k = NULL;

#ifdef WITH_OPENSSL
	BENCH_START("generate RSA-1024");
	TEST_START("generate KEY_RSA");
	ASSERT_INT_EQ(sshkey_generate(KEY_RSA, 1024, &k), 0);
	ASSERT_PTR_NE(k, NULL);
	sshkey_free(k);
	k = NULL;
	TEST_DONE();
	BENCH_FINISH("keys");

	BENCH_START("generate RSA-2048");
	TEST_START("generate KEY_RSA");
	ASSERT_INT_EQ(sshkey_generate(KEY_RSA, 2048, &k), 0);
	ASSERT_PTR_NE(k, NULL);
	sshkey_free(k);
	k = NULL;
	TEST_DONE();
	BENCH_FINISH("keys");

	BENCH_START("generate ECDSA-256");
	TEST_START("generate KEY_ECDSA");
	ASSERT_INT_EQ(sshkey_generate(KEY_ECDSA, 256, &k), 0);
	ASSERT_PTR_NE(k, NULL);
	sshkey_free(k);
	k = NULL;
	TEST_DONE();
	BENCH_FINISH("keys");

	BENCH_START("generate ECDSA-384");
	TEST_START("generate KEY_ECDSA");
	ASSERT_INT_EQ(sshkey_generate(KEY_ECDSA, 384, &k), 0);
	ASSERT_PTR_NE(k, NULL);
	sshkey_free(k);
	k = NULL;
	TEST_DONE();
	BENCH_FINISH("keys");

	BENCH_START("generate ECDSA-521");
	TEST_START("generate KEY_ECDSA");
	ASSERT_INT_EQ(sshkey_generate(KEY_ECDSA, 521, &k), 0);
	ASSERT_PTR_NE(k, NULL);
	sshkey_free(k);
	k = NULL;
	TEST_DONE();
	BENCH_FINISH("keys");
#endif /* WITH_OPENSSL */

	BENCH_START("generate ED25519");
	TEST_START("generate KEY_ED25519");
	ASSERT_INT_EQ(sshkey_generate(KEY_ED25519, 256, &k), 0);
	ASSERT_PTR_NE(k, NULL);
	sshkey_free(k);
	k = NULL;
	TEST_DONE();
	BENCH_FINISH("keys");

#ifdef WITH_OPENSSL
	/* sign */
	signature_benchmark("RSA-1024/SHA1", KEY_RSA, 1024, "ssh-rsa", 0);
	signature_benchmark("RSA-1024/SHA256", KEY_RSA, 1024, "rsa-sha2-256", 0);
	signature_benchmark("RSA-1024/SHA512", KEY_RSA, 1024, "rsa-sha2-512", 0);
	signature_benchmark("RSA-2048/SHA1", KEY_RSA, 2048, "ssh-rsa", 0);
	signature_benchmark("RSA-2048/SHA256", KEY_RSA, 2048, "rsa-sha2-256", 0);
	signature_benchmark("RSA-2048/SHA512", KEY_RSA, 2048, "rsa-sha2-512", 0);
	signature_benchmark("ECDSA-256", KEY_ECDSA, 256, NULL, 0);
	signature_benchmark("ECDSA-384", KEY_ECDSA, 384, NULL, 0);
	signature_benchmark("ECDSA-521", KEY_ECDSA, 521, NULL, 0);
	signature_benchmark("ED25519", KEY_ED25519, 0, NULL, 0);

	/* verify */
	signature_benchmark("RSA-1024/SHA1", KEY_RSA, 1024, "ssh-rsa", 1);
	signature_benchmark("RSA-1024/SHA256", KEY_RSA, 1024, "rsa-sha2-256", 1);
	signature_benchmark("RSA-1024/SHA512", KEY_RSA, 1024, "rsa-sha2-512", 1);
	signature_benchmark("RSA-2048/SHA1", KEY_RSA, 2048, "ssh-rsa", 1);
	signature_benchmark("RSA-2048/SHA256", KEY_RSA, 2048, "rsa-sha2-256", 1);
	signature_benchmark("RSA-2048/SHA512", KEY_RSA, 2048, "rsa-sha2-512", 1);
	signature_benchmark("ECDSA-256", KEY_ECDSA, 256, NULL, 1);
	signature_benchmark("ECDSA-384", KEY_ECDSA, 384, NULL, 1);
	signature_benchmark("ECDSA-521", KEY_ECDSA, 521, NULL, 1);
#endif /* WITH_OPENSSL */
	signature_benchmark("ED25519", KEY_ED25519, 0, NULL, 1);
}
