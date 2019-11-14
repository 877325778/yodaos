/*
 * (C) Copyright 2016
 * Stefano Babic, DENX Software Engineering, sbabic@denx.de.
 *
 * Code mostly taken from openssl examples
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "swupdate.h"
#include "sslapi.h"
#include "util.h"

#define BUFSIZE	(1024 * 8)

static int dgst_init(struct swupdate_digest *dgst)
{
	int rc;

	ERR_clear_error();
	rc = EVP_DigestInit_ex(dgst->ctx, EVP_sha256(), NULL);
	if (rc != 1) {
		ERROR("EVP_DigestInit_ex failed: %s\n", ERR_error_string(ERR_get_error(), NULL));
		return -EINVAL; /* failed */
	}

	return 0;
}

/*
 * depending on the algorithm, load a RSA public key
 * or a certificate
 */
#if defined(CONFIG_SIGALG_RAWRSA)
static EVP_PKEY *load_pubkey(const char *file)
{
	BIO *key=NULL;
	EVP_PKEY *pkey=NULL;

	if (file == NULL)
	{
		ERROR("no keyfile specified\n");
		goto end;
	}

	key=BIO_new(BIO_s_file());
	if (key == NULL)
	{
		goto end;
	}

	if (BIO_read_filename(key, file) <= 0)
	{
		printf("Error opening %s \n", file);
		goto end;
	}

	pkey=PEM_read_bio_PUBKEY(key, NULL, NULL, NULL);
end:
	if (key != NULL) BIO_free(key);
	if (pkey == NULL)
		ERROR("unable to load key filename %s\n", file);
	return(pkey);
}

static int dgst_verify_init(struct swupdate_digest *dgst)
{
	int rc;

	rc = EVP_DigestVerifyInit(dgst->ctx, NULL, EVP_sha256(), NULL, dgst->pkey);
	if (rc != 1) {
		ERROR("EVP_DigestVerifyInit failed, error 0x%lx\n", ERR_get_error());
		return -EFAULT; /* failed */
	}

	return 0;
}

static int verify_update(struct swupdate_digest *dgst, char *msg, unsigned int mlen)
{
	int rc;

	rc = EVP_DigestVerifyUpdate(dgst->ctx, msg, mlen);
	if(rc != 1) {
		ERROR("EVP_DigestVerifyUpdate failed, error 0x%lx\n", ERR_get_error());
		return -EFAULT;
	}

	return 0;
}

static int verify_final(struct swupdate_digest *dgst, unsigned char *sig, unsigned int slen)
{
	unsigned int rc;

	/* Clear any errors for the call below */
	ERR_clear_error();
	rc = EVP_DigestVerifyFinal(dgst->ctx, sig, slen);
	if(rc != 1) {
		ERROR("EVP_DigestVerifyFinal failed, error 0x%lx %d\n", ERR_get_error(), rc);
		return -1;
	}

	return rc;
}

int swupdate_verify_file(struct swupdate_digest *dgst, const char *sigfile,
		const char *file)
{
	FILE *fp = NULL;
	BIO *sigbio;
	int siglen = 0;
	int i;
	unsigned char *sigbuf = NULL;
	char *msg = NULL;
	int size;
	size_t rbytes;
	int status = 0;

	if (!dgst) {
		ERROR("Wrong crypto initialization: did you pass the key ?");
		status = -ENOKEY;
		goto out;
	}

	msg = malloc(BUFSIZE);
	if (!msg) {
		status = -ENOMEM;
		goto out;
	}

	sigbio = BIO_new_file(sigfile, "rb");
	siglen = EVP_PKEY_size(dgst->pkey);
	sigbuf = OPENSSL_malloc(siglen);

	siglen = BIO_read(sigbio, sigbuf, siglen);
	BIO_free(sigbio);

	if(siglen <= 0) {
		ERROR("Error reading signature file %s\n", sigfile);
		status = -ENOKEY;
		goto out;
	}

	if ((dgst_init(dgst) < 0) || (dgst_verify_init(dgst) < 0)) {
		status = -ENOKEY;
		goto out;
	}

	fp = fopen(file, "r");
	if (!fp) {
		ERROR("%s cannot be opened\n", file);
		status = -EBADF;
		goto out;
	}

	size = 0;
	for (;;) {
		rbytes = fread(msg, 1, BUFSIZE, fp);
		if (rbytes > 0) {
			size += rbytes;
			if (verify_update(dgst, msg, rbytes) < 0)
				break;
		}
		if (feof(fp))
			break;
	}

	TRACE("Verify signed image: Read %d bytes\n", size);
	i = verify_final(dgst, sigbuf, (unsigned int)siglen);
	if(i > 0) {
		TRACE("Verified OK\n");
		status = 0;
	} else if(i == 0) {
		TRACE("Verification Failure\n");
		status = -EBADMSG;
	} else {
		TRACE("Error Verifying Data\n");
		status = -EFAULT;
	}

out:
	if (fp)
		fclose(fp);
	if (msg)
		free(msg);
	if (sigbuf)
		OPENSSL_free(sigbuf);

	return status;
}

#elif defined(CONFIG_SIGALG_CMS)
static int cms_verify_callback(int ok, X509_STORE_CTX *ctx) {
	int cert_error = X509_STORE_CTX_get_error(ctx);

	if (!ok) {
		switch (cert_error) {
#if defined(CONFIG_CMS_IGNORE_CERTIFICATE_PURPOSE)
		case X509_V_ERR_CERT_HAS_EXPIRED:
		case X509_V_ERR_CERT_NOT_YET_VALID:
			ok = 1;
			break;
#endif
#if defined(CONFIG_CMS_IGNORE_CERTIFICATE_PURPOSE)
		case X509_V_ERR_INVALID_PURPOSE:
			ok = 1;
			break;
#endif
		default:
			break;
		}
	}

	return ok;
}
static X509_STORE *load_cert_chain(const char *file)
{
	X509_STORE *castore = X509_STORE_new();
	if (!castore) {
		return NULL;
	}

	/*
	 * Set error callback function for verification of CRTs and CRLs in order
	 * to ignore some errors depending on configuration
	 */
	X509_STORE_set_verify_cb(castore, cms_verify_callback);

	BIO *castore_bio = BIO_new_file(file, "r");
	if (!castore_bio) {
		TRACE("failed: BIO_new_file(%s)", file);
		return NULL;
	}

	int crt_count = 0;
	X509 *crt = NULL;
	do {
		crt = PEM_read_bio_X509(castore_bio, NULL, 0, NULL);
		if (crt) {
			crt_count++;
			TRACE("Read PEM #%d: %s", crt_count, crt->name);
			if (X509_STORE_add_cert(castore, crt) == 0) {
				TRACE("Adding certificate to X509_STORE failed");
				BIO_free(castore_bio);
				X509_STORE_free(castore);
				return NULL;
			}
		}
	} while (crt);
	BIO_free(castore_bio);

	if (crt_count == 0) {
		X509_STORE_free(castore);
		return NULL;
	}

	return castore;
}

int swupdate_verify_file(struct swupdate_digest *dgst, const char *sigfile,
		const char *file)
{
	int status = -EFAULT;
	CMS_ContentInfo *cms = NULL;
	BIO *content_bio = NULL;

	/* Open CMS blob that needs to be checked */
	BIO *sigfile_bio = BIO_new_file(sigfile, "rb");
	if (!sigfile_bio) {
		ERROR("%s cannot be opened", sigfile);
		status = -EBADF;
		goto out;
	}

	/* Parse the DER-encoded CMS message */
	cms = d2i_CMS_bio(sigfile_bio, NULL);
	if (!cms) {
		ERROR("%s cannot be parsed as DER-encoded CMS signature blob", sigfile);
		status = -EFAULT;
		goto out;
	}

	/* Open the content file (data which was signed) */
	content_bio = BIO_new_file(file, "rb");
	if (!content_bio) {
		ERROR("%s cannot be opened", file);
		status = -EBADF;
		goto out;
	}

	/* Then try to verify signature */
	if (!CMS_verify(cms, NULL, dgst->certs, content_bio, NULL, 0)) {
		ERR_print_errors_fp(stderr);
		ERROR("Signature verification failed");
		status = -EBADMSG;
		goto out;
	}

	TRACE("Verified OK\n");

	/* Signature is valid */
	status = 0;
out:

	if (cms) {
		CMS_ContentInfo_free(cms);
	}
	if (content_bio) {
		BIO_free(content_bio);
	}
	if (sigfile_bio) {
		BIO_free(sigfile_bio);
	}
	return status;
}
#endif
struct swupdate_digest *swupdate_HASH_init(void)
{
	struct swupdate_digest *dgst;
	int ret;

	dgst = calloc(1, sizeof(*dgst));
	if (!dgst) {
		return NULL;
	}

 	dgst->ctx = EVP_MD_CTX_create();
	if(dgst->ctx == NULL) {
		ERROR("EVP_MD_CTX_create failed, error 0x%lx\n", ERR_get_error());
		free(dgst);
		return NULL;
	}

	ret = dgst_init(dgst);
	if (ret) {
		free(dgst);
		return NULL;
	}

	return dgst;
}

int swupdate_HASH_update(struct swupdate_digest *dgst, unsigned char *buf,
				size_t len)
{
	if (!dgst)
		return -EFAULT;

	EVP_DigestUpdate (dgst->ctx, buf, len);

	return 0;
}

int swupdate_HASH_final(struct swupdate_digest *dgst, unsigned char *md_value,
		unsigned int *md_len)
{
	if (!dgst)
		return -EFAULT;

	return EVP_DigestFinal_ex (dgst->ctx, md_value, md_len);

}

void swupdate_HASH_cleanup(struct swupdate_digest *dgst)
{
	if (dgst) {
		EVP_MD_CTX_destroy(dgst->ctx);
		free(dgst);
		dgst = NULL;
	}
}

/*
 * Just a wrap function to memcmp
 */
int swupdate_HASH_compare(unsigned char *hash1, unsigned char *hash2)
{
	int i;

	for (i = 0; i < SHA256_HASH_LENGTH; i++)
		if (hash1[i] != hash2[i])
			return -1;

	return 0;
}

int swupdate_dgst_init(struct swupdate_cfg *sw, const char *keyfile)
{
	struct swupdate_digest *dgst;
	int ret;

	/*
	 * Check that it was not called before
	 */
	if (sw->dgst) {
		return -EBUSY;
	}

	dgst = calloc(1, sizeof(*dgst));
	if (!dgst) {
		ret = -ENOMEM;
		goto dgst_init_error;
	}

#if defined(CONFIG_SIGALG_RAWRSA)
	/*
	 * Load public key
	 */
	dgst->pkey = load_pubkey(keyfile);
	if (!dgst->pkey) {
		ERROR("Error loading pub key from %s", keyfile);
		ret = -EINVAL;
		goto dgst_init_error;
	}
#elif defined(CONFIG_SIGALG_CMS)
	/*
	 * Load certificate chain
	 */
	dgst->certs = load_cert_chain(keyfile);
	if (!dgst->certs) {
		ERROR("Error loading certificate chain from %s", keyfile);
		ret = -EINVAL;
		goto dgst_init_error;
	}
#else
	TRACE("public key / cert %s ignored, you need to set SIGALG", keyfile);
#endif

	/*
	 * Create context
	 */
	dgst->ctx = EVP_MD_CTX_create();
	if(dgst->ctx == NULL) {
		ERROR("EVP_MD_CTX_create failed, error 0x%lx\n", ERR_get_error());
		ret = -ENOMEM;
		goto dgst_init_error;
	}

	sw->dgst = dgst;

	return 0;

dgst_init_error:
	if (dgst)
		free(dgst);

	return ret;
}
