/* ssl/s3_clnt.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 1998-2002 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <stdio.h>
#include "ssl_locl.h"
#include "kssl_lcl.h"
#include <openssl/buffer.h>
#include <openssl/rand.h>
#include <openssl/objects.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/fips.h>

static SSL_METHOD *ssl3_get_client_method(int ver);
static int ssl3_client_hello(SSL *s);
static int ssl3_get_server_hello(SSL *s);
static int ssl3_get_certificate_request(SSL *s);
static int ca_dn_cmp(const X509_NAME * const *a,const X509_NAME * const *b);
static int ssl3_get_server_done(SSL *s);
static int ssl3_send_client_verify(SSL *s);
static int ssl3_send_client_certificate(SSL *s);
static int ssl3_send_client_key_exchange(SSL *s);
static int ssl3_get_key_exchange(SSL *s);
static int ssl3_get_server_certificate(SSL *s);
static int ssl3_check_cert_and_algorithm(SSL *s);
static SSL_METHOD *ssl3_get_client_method(int ver)
	{
	if (ver == SSL3_VERSION)
		return(SSLv3_client_method());
	else
		return(NULL);
	}

SSL_METHOD *SSLv3_client_method(void)
	{
	static int init=1;
	static SSL_METHOD SSLv3_client_data;

	if (init)
		{
		CRYPTO_w_lock(CRYPTO_LOCK_SSL_METHOD);

		if (init)
			{
			memcpy((char *)&SSLv3_client_data,(char *)sslv3_base_method(),
				sizeof(SSL_METHOD));
			SSLv3_client_data.ssl_connect=ssl3_connect;
			SSLv3_client_data.get_ssl_method=ssl3_get_client_method;
			init=0;
			}

		CRYPTO_w_unlock(CRYPTO_LOCK_SSL_METHOD);
		}
	return(&SSLv3_client_data);
	}

int ssl3_connect(SSL *s)
	{
	BUF_MEM *buf=NULL;
	unsigned long Time=(unsigned long)time(NULL),l;
	long num1;
	void (*cb)(const SSL *ssl,int type,int val)=NULL;
	int ret= -1;
	int new_state,state,skip=0;

	RAND_add(&Time,sizeof(Time),0);
	ERR_clear_error();
	clear_sys_error();

	if (s->info_callback != NULL)
		cb=s->info_callback;
	else if (s->ctx->info_callback != NULL)
		cb=s->ctx->info_callback;
	
	s->in_handshake++;
	if (!SSL_in_init(s) || SSL_in_before(s)) SSL_clear(s); 

	for (;;)
		{
		state=s->state;

		switch(s->state)
			{
		case SSL_ST_RENEGOTIATE:
			s->new_session=1;
			s->state=SSL_ST_CONNECT;
			s->ctx->stats.sess_connect_renegotiate++;
			/* break */
		case SSL_ST_BEFORE:
		case SSL_ST_CONNECT:
		case SSL_ST_BEFORE|SSL_ST_CONNECT:
		case SSL_ST_OK|SSL_ST_CONNECT:

			s->server=0;
			if (cb != NULL) cb(s,SSL_CB_HANDSHAKE_START,1);

			if ((s->version & 0xff00 ) != 0x0300)
				{
				SSLerr(SSL_F_SSL3_CONNECT, ERR_R_INTERNAL_ERROR);
				ret = -1;
				goto end;
				}
				
			/* s->version=SSL3_VERSION; */
			s->type=SSL_ST_CONNECT;

			if (s->init_buf == NULL)
				{
				if ((buf=BUF_MEM_new()) == NULL)
					{
					ret= -1;
					goto end;
					}
				if (!BUF_MEM_grow(buf,SSL3_RT_MAX_PLAIN_LENGTH))
					{
					ret= -1;
					goto end;
					}
				s->init_buf=buf;
				buf=NULL;
				}

			if (!ssl3_setup_buffers(s)) { ret= -1; goto end; }

			/* setup buffing BIO */
			if (!ssl_init_wbio_buffer(s,0)) { ret= -1; goto end; }

			/* don't push the buffering BIO quite yet */

			ssl3_init_finished_mac(s);

			s->state=SSL3_ST_CW_CLNT_HELLO_A;
			s->ctx->stats.sess_connect++;
			s->init_num=0;
			break;

		case SSL3_ST_CW_CLNT_HELLO_A:
		case SSL3_ST_CW_CLNT_HELLO_B:

			s->shutdown=0;
			ret=ssl3_client_hello(s);
			if (ret <= 0) goto end;
			s->state=SSL3_ST_CR_SRVR_HELLO_A;
			s->init_num=0;

			/* turn on buffering for the next lot of output */
			if (s->bbio != s->wbio)
				s->wbio=BIO_push(s->bbio,s->wbio);

			break;

		case SSL3_ST_CR_SRVR_HELLO_A:
		case SSL3_ST_CR_SRVR_HELLO_B:
			ret=ssl3_get_server_hello(s);
			if (ret <= 0) goto end;
			if (s->hit)
				s->state=SSL3_ST_CR_FINISHED_A;
			else
				s->state=SSL3_ST_CR_CERT_A;
			s->init_num=0;
			break;

		case SSL3_ST_CR_CERT_A:
		case SSL3_ST_CR_CERT_B:
			/* Check if it is anon DH */
			if (!(s->s3->tmp.new_cipher->algorithms & SSL_aNULL))
				{
				ret=ssl3_get_server_certificate(s);
				if (ret <= 0) goto end;
				}
			else
				skip=1;
			s->state=SSL3_ST_CR_KEY_EXCH_A;
			s->init_num=0;
			break;

		case SSL3_ST_CR_KEY_EXCH_A:
		case SSL3_ST_CR_KEY_EXCH_B:
			ret=ssl3_get_key_exchange(s);
			if (ret <= 0) goto end;
			s->state=SSL3_ST_CR_CERT_REQ_A;
			s->init_num=0;

			/* at this point we check that we have the
			 * required stuff from the server */
			if (!ssl3_check_cert_and_algorithm(s))
				{
				ret= -1;
				goto end;
				}
			break;

		case SSL3_ST_CR_CERT_REQ_A:
		case SSL3_ST_CR_CERT_REQ_B:
			ret=ssl3_get_certificate_request(s);
			if (ret <= 0) goto end;
			s->state=SSL3_ST_CR_SRVR_DONE_A;
			s->init_num=0;
			break;

		case SSL3_ST_CR_SRVR_DONE_A:
		case SSL3_ST_CR_SRVR_DONE_B:
			ret=ssl3_get_server_done(s);
			if (ret <= 0) goto end;
			if (s->s3->tmp.cert_req)
				s->state=SSL3_ST_CW_CERT_A;
			else
				s->state=SSL3_ST_CW_KEY_EXCH_A;
			s->init_num=0;

			break;

		case SSL3_ST_CW_CERT_A:
		case SSL3_ST_CW_CERT_B:
		case SSL3_ST_CW_CERT_C:
		case SSL3_ST_CW_CERT_D:
			ret=ssl3_send_client_certificate(s);
			if (ret <= 0) goto end;
			s->state=SSL3_ST_CW_KEY_EXCH_A;
			s->init_num=0;
			break;

		case SSL3_ST_CW_KEY_EXCH_A:
		case SSL3_ST_CW_KEY_EXCH_B:
			ret=ssl3_send_client_key_exchange(s);
			if (ret <= 0) goto end;
			l=s->s3->tmp.new_cipher->algorithms;
			/* EAY EAY EAY need to check for DH fix cert
			 * sent back */
			/* For TLS, cert_req is set to 2, so a cert chain
			 * of nothing is sent, but no verify packet is sent */
			if (s->s3->tmp.cert_req == 1)
				{
				s->state=SSL3_ST_CW_CERT_VRFY_A;
				}
			else
				{
				s->state=SSL3_ST_CW_CHANGE_A;
				s->s3->change_cipher_spec=0;
				}

			s->init_num=0;
			break;

		case SSL3_ST_CW_CERT_VRFY_A:
		case SSL3_ST_CW_CERT_VRFY_B:
			ret=ssl3_send_client_verify(s);
			if (ret <= 0) goto end;
			s->state=SSL3_ST_CW_CHANGE_A;
			s->init_num=0;
			s->s3->change_cipher_spec=0;
			break;

		case SSL3_ST_CW_CHANGE_A:
		case SSL3_ST_CW_CHANGE_B:
			ret=ssl3_send_change_cipher_spec(s,
				SSL3_ST_CW_CHANGE_A,SSL3_ST_CW_CHANGE_B);
			if (ret <= 0) goto end;
			s->state=SSL3_ST_CW_FINISHED_A;
			s->init_num=0;

			s->session->cipher=s->s3->tmp.new_cipher;
			if (s->s3->tmp.new_compression == NULL)
				s->session->compress_meth=0;
			else
				s->session->compress_meth=
					s->s3->tmp.new_compression->id;
			if (!s->method->ssl3_enc->setup_key_block(s))
				{
				ret= -1;
				goto end;
				}

			if (!s->method->ssl3_enc->change_cipher_state(s,
				SSL3_CHANGE_CIPHER_CLIENT_WRITE))
				{
				ret= -1;
				goto end;
				}

			break;

		case SSL3_ST_CW_FINISHED_A:
		case SSL3_ST_CW_FINISHED_B:
			ret=ssl3_send_finished(s,
				SSL3_ST_CW_FINISHED_A,SSL3_ST_CW_FINISHED_B,
				s->method->ssl3_enc->client_finished_label,
				s->method->ssl3_enc->client_finished_label_len);
			if (ret <= 0) goto end;
			s->state=SSL3_ST_CW_FLUSH;

			/* clear flags */
			s->s3->flags&= ~SSL3_FLAGS_POP_BUFFER;
			if (s->hit)
				{
				s->s3->tmp.next_state=SSL_ST_OK;
				if (s->s3->flags & SSL3_FLAGS_DELAY_CLIENT_FINISHED)
					{
					s->state=SSL_ST_OK;
					s->s3->flags|=SSL3_FLAGS_POP_BUFFER;
					s->s3->delay_buf_pop_ret=0;
					}
				}
			else
				{
				s->s3->tmp.next_state=SSL3_ST_CR_FINISHED_A;
				}
			s->init_num=0;
			break;

		case SSL3_ST_CR_FINISHED_A:
		case SSL3_ST_CR_FINISHED_B:

			ret=ssl3_get_finished(s,SSL3_ST_CR_FINISHED_A,
				SSL3_ST_CR_FINISHED_B);
			if (ret <= 0) goto end;

			if (s->hit)
				s->state=SSL3_ST_CW_CHANGE_A;
			else
				s->state=SSL_ST_OK;
			s->init_num=0;
			break;

		case SSL3_ST_CW_FLUSH:
			/* number of bytes to be flushed */
			num1=BIO_ctrl(s->wbio,BIO_CTRL_INFO,0,NULL);
			if (num1 > 0)
				{
				s->rwstate=SSL_WRITING;
				num1=BIO_flush(s->wbio);
				if (num1 <= 0) { ret= -1; goto end; }
				s->rwstate=SSL_NOTHING;
				}

			s->state=s->s3->tmp.next_state;
			break;

		case SSL_ST_OK:
			/* clean a few things up */
			ssl3_cleanup_key_block(s);

			if (s->init_buf != NULL)
				{
				BUF_MEM_free(s->init_buf);
				s->init_buf=NULL;
				}

			/* If we are not 'joining' the last two packets,
			 * remove the buffering now */
			if (!(s->s3->flags & SSL3_FLAGS_POP_BUFFER))
				ssl_free_wbio_buffer(s);
			/* else do it later in ssl3_write */

			s->init_num=0;
			s->new_session=0;

			ssl_update_cache(s,SSL_SESS_CACHE_CLIENT);
			if (s->hit) s->ctx->stats.sess_hit++;

			ret=1;
			/* s->server=0; */
			s->handshake_func=ssl3_connect;
			s->ctx->stats.sess_connect_good++;

			if (cb != NULL) cb(s,SSL_CB_HANDSHAKE_DONE,1);

			goto end;
			/* break; */
			
		default:
			SSLerr(SSL_F_SSL3_CONNECT,SSL_R_UNKNOWN_STATE);
			ret= -1;
			goto end;
			/* break; */
			}

		/* did we do anything */
		if (!s->s3->tmp.reuse_message && !skip)
			{
			if (s->debug)
				{
				if ((ret=BIO_flush(s->wbio)) <= 0)
					goto end;
				}

			if ((cb != NULL) && (s->state != state))
				{
				new_state=s->state;
				s->state=state;
				cb(s,SSL_CB_CONNECT_LOOP,1);
				s->state=new_state;
				}
			}
		skip=0;
		}
end:
	s->in_handshake--;
	if (buf != NULL)
		BUF_MEM_free(buf);
	if (cb != NULL)
		cb(s,SSL_CB_CONNECT_EXIT,ret);
	return(ret);
	}


static int ssl3_client_hello(SSL *s)
	{
	unsigned char *buf;
	unsigned char *p,*d;
	int i,j;
	unsigned long Time,l;
	SSL_COMP *comp;

	buf=(unsigned char *)s->init_buf->data;
	if (s->state == SSL3_ST_CW_CLNT_HELLO_A)
		{
		if ((s->session == NULL) ||
			(s->session->ssl_version != s->version) ||
			(s->session->not_resumable))
			{
			if (!ssl_get_new_session(s,0))
				goto err;
			}
		/* else use the pre-loaded session */

		p=s->s3->client_random;
		Time=(unsigned long)time(NULL);			/* Time */
		l2n(Time,p);
		if(RAND_pseudo_bytes(p,SSL3_RANDOM_SIZE-4) <= 0)
		    goto err;

		/* Do the message type and length last */
		d=p= &(buf[4]);

		*(p++)=s->version>>8;
		*(p++)=s->version&0xff;
		s->client_version=s->version;

		/* Random stuff */
		memcpy(p,s->s3->client_random,SSL3_RANDOM_SIZE);
		p+=SSL3_RANDOM_SIZE;

		/* Session ID */
		if (s->new_session)
			i=0;
		else
			i=s->session->session_id_length;
		*(p++)=i;
		if (i != 0)
			{
			if (i > sizeof s->session->session_id)
				{
				SSLerr(SSL_F_SSL3_CLIENT_HELLO, ERR_R_INTERNAL_ERROR);
				goto err;
				}
			memcpy(p,s->session->session_id,i);
			p+=i;
			}
		
		/* Ciphers supported */
		i=ssl_cipher_list_to_bytes(s,SSL_get_ciphers(s),&(p[2]),0);
		if (i == 0)
			{
			SSLerr(SSL_F_SSL3_CLIENT_HELLO,SSL_R_NO_CIPHERS_AVAILABLE);
			goto err;
			}
		s2n(i,p);
		p+=i;

		/* COMPRESSION */
		if (s->ctx->comp_methods == NULL)
			j=0;
		else
			j=sk_SSL_COMP_num(s->ctx->comp_methods);
		*(p++)=1+j;
		for (i=0; i<j; i++)
			{
			comp=sk_SSL_COMP_value(s->ctx->comp_methods,i);
			*(p++)=comp->id;
			}
		*(p++)=0; /* Add the NULL method */
		
		l=(p-d);
		d=buf;
		*(d++)=SSL3_MT_CLIENT_HELLO;
		l2n3(l,d);

		s->state=SSL3_ST_CW_CLNT_HELLO_B;
		/* number of bytes to write */
		s->init_num=p-buf;
		s->init_off=0;
		}

	/* SSL3_ST_CW_CLNT_HELLO_B */
	return(ssl3_do_write(s,SSL3_RT_HANDSHAKE));
err:
	return(-1);
	}

static int ssl3_get_server_hello(SSL *s)
	{
	STACK_OF(SSL_CIPHER) *sk;
	SSL_CIPHER *c;
	unsigned char *p,*d;
	int i,al,ok;
	unsigned int j;
	long n;
	SSL_COMP *comp;

	n=ssl3_get_message(s,
		SSL3_ST_CR_SRVR_HELLO_A,
		SSL3_ST_CR_SRVR_HELLO_B,
		SSL3_MT_SERVER_HELLO,
		300, /* ?? */
		&ok);

	if (!ok) return((int)n);
	d=p=(unsigned char *)s->init_msg;

	if ((p[0] != (s->version>>8)) || (p[1] != (s->version&0xff)))
		{
		SSLerr(SSL_F_SSL3_GET_SERVER_HELLO,SSL_R_WRONG_SSL_VERSION);
		s->version=(s->version&0xff00)|p[1];
		al=SSL_AD_PROTOCOL_VERSION;
		goto f_err;
		}
	p+=2;

	/* load the server hello data */
	/* load the server random */
	memcpy(s->s3->server_random,p,SSL3_RANDOM_SIZE);
	p+=SSL3_RANDOM_SIZE;

	/* get the session-id */
	j= *(p++);

	if ((j > sizeof s->session->session_id) || (j > SSL3_SESSION_ID_SIZE))
		{
		al=SSL_AD_ILLEGAL_PARAMETER;
		SSLerr(SSL_F_SSL3_GET_SERVER_HELLO,SSL_R_SSL3_SESSION_ID_TOO_LONG);
		goto f_err;
		}

	if (j != 0 && j == s->session->session_id_length
	    && memcmp(p,s->session->session_id,j) == 0)
	    {
	    if(s->sid_ctx_length != s->session->sid_ctx_length
	       || memcmp(s->session->sid_ctx,s->sid_ctx,s->sid_ctx_length))
		{
		/* actually a client application bug */
		al=SSL_AD_ILLEGAL_PARAMETER;
		SSLerr(SSL_F_SSL3_GET_SERVER_HELLO,SSL_R_ATTEMPT_TO_REUSE_SESSION_IN_DIFFERENT_CONTEXT);
		goto f_err;
		}
	    s->hit=1;
	    }
	else	/* a miss or crap from the other end */
		{
		/* If we were trying for session-id reuse, make a new
		 * SSL_SESSION so we don't stuff up other people */
		s->hit=0;
		if (s->session->session_id_length > 0)
			{
			if (!ssl_get_new_session(s,0))
				{
				al=SSL_AD_INTERNAL_ERROR;
				goto f_err;
				}
			}
		s->session->session_id_length=j;
		memcpy(s->session->session_id,p,j); /* j could be 0 */
		}
	p+=j;
	c=ssl_get_cipher_by_char(s,p);
	if (c == NULL)
		{
		/* unknown cipher */
		al=SSL_AD_ILLEGAL_PARAMETER;
		SSLerr(SSL_F_SSL3_GET_SERVER_HELLO,SSL_R_UNKNOWN_CIPHER_RETURNED);
		goto f_err;
		}
	p+=ssl_put_cipher_by_char(s,NULL,NULL);

	sk=ssl_get_ciphers_by_id(s);
	i=sk_SSL_CIPHER_find(sk,c);
	if (i < 0)
		{
		/* we did not say we would use this cipher */
		al=SSL_AD_ILLEGAL_PARAMETER;
		SSLerr(SSL_F_SSL3_GET_SERVER_HELLO,SSL_R_WRONG_CIPHER_RETURNED);
		goto f_err;
		}

	/* Depending on the session caching (internal/external), the cipher
	   and/or cipher_id values may not be set. Make sure that
	   cipher_id is set and use it for comparison. */
	if (s->session->cipher)
		s->session->cipher_id = s->session->cipher->id;
	if (s->hit && (s->session->cipher_id != c->id))
		{
		if (!(s->options &
			SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG))
			{
			al=SSL_AD_ILLEGAL_PARAMETER;
			SSLerr(SSL_F_SSL3_GET_SERVER_HELLO,SSL_R_OLD_SESSION_CIPHER_NOT_RETURNED);
			goto f_err;
			}
		}
	s->s3->tmp.new_cipher=c;

	/* lets get the compression algorithm */
	/* COMPRESSION */
	j= *(p++);
	if (j == 0)
		comp=NULL;
	else
		comp=ssl3_comp_find(s->ctx->comp_methods,j);
	
	if ((j != 0) && (comp == NULL))
		{
		al=SSL_AD_ILLEGAL_PARAMETER;
		SSLerr(SSL_F_SSL3_GET_SERVER_HELLO,SSL_R_UNSUPPORTED_COMPRESSION_ALGORITHM);
		goto f_err;
		}
	else
		{
		s->s3->tmp.new_compression=comp;
		}

	if (p != (d+n))
		{
		/* wrong packet length */
		al=SSL_AD_DECODE_ERROR;
		SSLerr(SSL_F_SSL3_GET_SERVER_HELLO,SSL_R_BAD_PACKET_LENGTH);
		goto err;
		}

	return(1);
f_err:
	ssl3_send_alert(s,SSL3_AL_FATAL,al);
err:
	return(-1);
	}

static int ssl3_get_server_certificate(SSL *s)
	{
	int al,i,ok,ret= -1;
	unsigned long n,nc,llen,l;
	X509 *x=NULL;
	unsigned char *p,*d,*q;
	STACK_OF(X509) *sk=NULL;
	SESS_CERT *sc;
	EVP_PKEY *pkey=NULL;
        int need_cert = 1; /* VRS: 0=> will allow null cert if auth == KRB5 */

	n=ssl3_get_message(s,
		SSL3_ST_CR_CERT_A,
		SSL3_ST_CR_CERT_B,
		-1,
		s->max_cert_list,
		&ok);

	if (!ok) return((int)n);

	if (s->s3->tmp.message_type == SSL3_MT_SERVER_KEY_EXCHANGE)
		{
		s->s3->tmp.reuse_message=1;
		return(1);
		}

	if (s->s3->tmp.message_type != SSL3_MT_CERTIFICATE)
		{
		al=SSL_AD_UNEXPECTED_MESSAGE;
		SSLerr(SSL_F_SSL3_GET_SERVER_CERTIFICATE,SSL_R_BAD_MESSAGE_TYPE);
		goto f_err;
		}
	d=p=(unsigned char *)s->init_msg;

	if ((sk=sk_X509_new_null()) == NULL)
		{
		SSLerr(SSL_F_SSL3_GET_SERVER_CERTIFICATE,ERR_R_MALLOC_FAILURE);
		goto err;
		}

	n2l3(p,llen);
	if (llen+3 != n)
		{
		al=SSL_AD_DECODE_ERROR;
		SSLerr(SSL_F_SSL3_GET_SERVER_CERTIFICATE,SSL_R_LENGTH_MISMATCH);
		goto f_err;
		}
	for (nc=0; nc<llen; )
		{
		n2l3(p,l);
		if ((l+nc+3) > llen)
			{
			al=SSL_AD_DECODE_ERROR;
			SSLerr(SSL_F_SSL3_GET_SERVER_CERTIFICATE,SSL_R_CERT_LENGTH_MISMATCH);
			goto f_err;
			}

		q=p;
		x=d2i_X509(NULL,&q,l);
		if (x == NULL)
			{
			al=SSL_AD_BAD_CERTIFICATE;
			SSLerr(SSL_F_SSL3_GET_SERVER_CERTIFICATE,ERR_R_ASN1_LIB);
			goto f_err;
			}
		if (q != (p+l))
			{
			al=SSL_AD_DECODE_ERROR;
			SSLerr(SSL_F_SSL3_GET_SERVER_CERTIFICATE,SSL_R_CERT_LENGTH_MISMATCH);
			goto f_err;
			}
		if (!sk_X509_push(sk,x))
			{
			SSLerr(SSL_F_SSL3_GET_SERVER_CERTIFICATE,ERR_R_MALLOC_FAILURE);
			goto err;
			}
		x=NULL;
		nc+=l+3;
		p=q;
		}

	i=ssl_verify_cert_chain(s,sk);
	if ((s->verify_mode != SSL_VERIFY_NONE) && (!i)
#ifndef OPENSSL_NO_KRB5
                && (s->s3->tmp.new_cipher->algorithms & (SSL_MKEY_MASK|SSL_AUTH_MASK))
                != (SSL_aKRB5|SSL_kKRB5)
#endif /* OPENSSL_NO_KRB5 */
                )
		{
		al=ssl_verify_alarm_type(s->verify_result);
		SSLerr(SSL_F_SSL3_GET_SERVER_CERTIFICATE,SSL_R_CERTIFICATE_VERIFY_FAILED);
		goto f_err; 
		}
	ERR_clear_error(); /* but we keep s->verify_result */

	sc=ssl_sess_cert_new();
	if (sc == NULL) goto err;

	if (s->session->sess_cert) ssl_sess_cert_free(s->session->sess_cert);
	s->session->sess_cert=sc;

	sc->cert_chain=sk;
	/* Inconsistency alert: cert_chain does include the peer's
	 * certificate, which we don't include in s3_srvr.c */
	x=sk_X509_value(sk,0);
	sk=NULL;
 	/* VRS 19990621: possible memory leak; sk=null ==> !sk_pop_free() @end*/

	pkey=X509_get_pubkey(x);

        /* VRS: allow null cert if auth == KRB5 */
        need_cert =	((s->s3->tmp.new_cipher->algorithms
			& (SSL_MKEY_MASK|SSL_AUTH_MASK))
                        == (SSL_aKRB5|SSL_kKRB5))? 0: 1;

#ifdef KSSL_DEBUG
	printf("pkey,x = %p, %p\n", pkey,x);
	printf("ssl_cert_type(x,pkey) = %d\n", ssl_cert_type(x,pkey));
	printf("cipher, alg, nc = %s, %lx, %d\n", s->s3->tmp.new_cipher->name,
                s->s3->tmp.new_cipher->algorithms, need_cert);
#endif    /* KSSL_DEBUG */

	if (need_cert && ((pkey == NULL) || EVP_PKEY_missing_parameters(pkey)))
		{
		x=NULL;
		al=SSL3_AL_FATAL;
		SSLerr(SSL_F_SSL3_GET_SERVER_CERTIFICATE,
			SSL_R_UNABLE_TO_FIND_PUBLIC_KEY_PARAMETERS);
		goto f_err;
		}

	i=ssl_cert_type(x,pkey);
	if (need_cert && i < 0)
		{
		x=NULL;
		al=SSL3_AL_FATAL;
		SSLerr(SSL_F_SSL3_GET_SERVER_CERTIFICATE,
			SSL_R_UNKNOWN_CERTIFICATE_TYPE);
		goto f_err;
		}

        if (need_cert)
                {
                sc->peer_cert_type=i;
                CRYPTO_add(&x->references,1,CRYPTO_LOCK_X509);
                /* Why would the following ever happen?
                 * We just created sc a couple of lines ago. */
                if (sc->peer_pkeys[i].x509 != NULL)
                        X509_free(sc->peer_pkeys[i].x509);
                sc->peer_pkeys[i].x509=x;
                sc->peer_key= &(sc->peer_pkeys[i]);

                if (s->session->peer != NULL)
                        X509_free(s->session->peer);
                CRYPTO_add(&x->references,1,CRYPTO_LOCK_X509);
                s->session->peer=x;
                }
        else
                {
                sc->peer_cert_type=i;
                sc->peer_key= NULL;

                if (s->session->peer != NULL)
                        X509_free(s->session->peer);
                s->session->peer=NULL;
                }
	s->session->verify_result = s->verify_result;

	x=NULL;
	ret=1;

	if (0)
		{
f_err:
		ssl3_send_alert(s,SSL3_AL_FATAL,al);
		}
err:
	EVP_PKEY_free(pkey);
	X509_free(x);
	sk_X509_pop_free(sk,X509_free);
	return(ret);
	}

static int ssl3_get_key_exchange(SSL *s)
	{
#ifndef OPENSSL_NO_RSA
	unsigned char *q,md_buf[EVP_MAX_MD_SIZE*2];
#endif
	EVP_MD_CTX md_ctx;
	unsigned char *param,*p;
	int al,i,j,param_len,ok;
	long n,alg;
	EVP_PKEY *pkey=NULL;
#ifndef OPENSSL_NO_RSA
	RSA *rsa=NULL;
#endif
#ifndef OPENSSL_NO_DH
	DH *dh=NULL;
#endif

	/* use same message size as in ssl3_get_certificate_request()
	 * as ServerKeyExchange message may be skipped */
	n=ssl3_get_message(s,
		SSL3_ST_CR_KEY_EXCH_A,
		SSL3_ST_CR_KEY_EXCH_B,
		-1,
		s->max_cert_list,
		&ok);

	if (!ok) return((int)n);

	if (s->s3->tmp.message_type != SSL3_MT_SERVER_KEY_EXCHANGE)
		{
		s->s3->tmp.reuse_message=1;
		return(1);
		}

	param=p=(unsigned char *)s->init_msg;

	if (s->session->sess_cert != NULL)
		{
#ifndef OPENSSL_NO_RSA
		if (s->session->sess_cert->peer_rsa_tmp != NULL)
			{
			RSA_free(s->session->sess_cert->peer_rsa_tmp);
			s->session->sess_cert->peer_rsa_tmp=NULL;
			}
#endif
#ifndef OPENSSL_NO_DH
		if (s->session->sess_cert->peer_dh_tmp)
			{
			DH_free(s->session->sess_cert->peer_dh_tmp);
			s->session->sess_cert->peer_dh_tmp=NULL;
			}
#endif
		}
	else
		{
		s->session->sess_cert=ssl_sess_cert_new();
		}

	param_len=0;
	alg=s->s3->tmp.new_cipher->algorithms;
	EVP_MD_CTX_init(&md_ctx);

#ifndef OPENSSL_NO_RSA
	if (alg & SSL_kRSA)
		{
		if ((rsa=RSA_new()) == NULL)
			{
			SSLerr(SSL_F_SSL3_GET_KEY_EXCHANGE,ERR_R_MALLOC_FAILURE);
			goto err;
			}
		n2s(p,i);
		param_len=i+2;
		if (param_len > n)
			{
			al=SSL_AD_DECODE_ERROR;
			SSLerr(SSL_F_SSL3_GET_KEY_EXCHANGE,SSL_R_BAD_RSA_MODULUS_LENGTH);
			goto f_err;
			}
		if (!(rsa->n=BN_bin2bn(p,i,rsa->n)))
			{
			SSLerr(SSL_F_SSL3_GET_KEY_EXCHANGE,ERR_R_BN_LIB);
			goto err;
			}
		p+=i;

		n2s(p,i);
		param_len+=i+2;
		if (param_len > n)
			{
			al=SSL_AD_DECODE_ERROR;
			SSLerr(SSL_F_SSL3_GET_KEY_EXCHANGE,SSL_R_BAD_RSA_E_LENGTH);
			goto f_err;
			}
		if (!(rsa->e=BN_bin2bn(p,i,rsa->e)))
			{
			SSLerr(SSL_F_SSL3_GET_KEY_EXCHANGE,ERR_R_BN_LIB);
			goto err;
			}
		p+=i;
		n-=param_len;

		/* this should be because we are using an export cipher */
		if (alg & SSL_aRSA)
			pkey=X509_get_pubkey(s->session->sess_cert->peer_pkeys[SSL_PKEY_RSA_ENC].x509);
		else
			{
			SSLerr(SSL_F_SSL3_GET_KEY_EXCHANGE,ERR_R_INTERNAL_ERROR);
			goto err;
			}
		s->session->sess_cert->peer_rsa_tmp=rsa;
		rsa=NULL;
		}
#else /* OPENSSL_NO_RSA */
	if (0)
		;
#endif
#ifndef OPENSSL_NO_DH
	else if (alg & SSL_kEDH)
		{
		if ((dh=DH_new()) == NULL)
			{
			SSLerr(SSL_F_SSL3_GET_KEY_EXCHANGE,ERR_R_DH_LIB);
			goto err;
			}
		n2s(p,i);
		param_len=i+2;
		if (param_len > n)
			{
			al=SSL_AD_DECODE_ERROR;
			SSLerr(SSL_F_SSL3_GET_KEY_EXCHANGE,SSL_R_BAD_DH_P_LENGTH);
			goto f_err;
			}
		if (!(dh->p=BN_bin2bn(p,i,NULL)))
			{
			SSLerr(SSL_F_SSL3_GET_KEY_EXCHANGE,ERR_R_BN_LIB);
			goto err;
			}
		p+=i;

		n2s(p,i);
		param_len+=i+2;
		if (param_len > n)
			{
			al=SSL_AD_DECODE_ERROR;
			SSLerr(SSL_F_SSL3_GET_KEY_EXCHANGE,SSL_R_BAD_DH_G_LENGTH);
			goto f_err;
			}
		if (!(dh->g=BN_bin2bn(p,i,NULL)))
			{
			SSLerr(SSL_F_SSL3_GET_KEY_EXCHANGE,ERR_R_BN_LIB);
			goto err;
			}
		p+=i;

		n2s(p,i);
		param_len+=i+2;
		if (param_len > n)
			{
			al=SSL_AD_DECODE_ERROR;
			SSLerr(SSL_F_SSL3_GET_KEY_EXCHANGE,SSL_R_BAD_DH_PUB_KEY_LENGTH);
			goto f_err;
			}
		if (!(dh->pub_key=BN_bin2bn(p,i,NULL)))
			{
			SSLerr(SSL_F_SSL3_GET_KEY_EXCHANGE,ERR_R_BN_LIB);
			goto err;
			}
		p+=i;
		n-=param_len;

#ifndef OPENSSL_NO_RSA
		if (alg & SSL_aRSA)
			pkey=X509_get_pubkey(s->session->sess_cert->peer_pkeys[SSL_PKEY_RSA_ENC].x509);
#else
		if (0)
			;
#endif
#ifndef OPENSSL_NO_DSA
		else if (alg & SSL_aDSS)
			pkey=X509_get_pubkey(s->session->sess_cert->peer_pkeys[SSL_PKEY_DSA_SIGN].x509);
#endif
		/* else anonymous DH, so no certificate or pkey. */

		s->session->sess_cert->peer_dh_tmp=dh;
		dh=NULL;
		}
	else if ((alg & SSL_kDHr) || (alg & SSL_kDHd))
		{
		al=SSL_AD_ILLEGAL_PARAMETER;
		SSLerr(SSL_F_SSL3_GET_KEY_EXCHANGE,SSL_R_TRIED_TO_USE_UNSUPPORTED_CIPHER);
		goto f_err;
		}
#endif /* !OPENSSL_NO_DH */
	if (alg & SSL_aFZA)
		{
		al=SSL_AD_HANDSHAKE_FAILURE;
		SSLerr(SSL_F_SSL3_GET_KEY_EXCHANGE,SSL_R_TRIED_TO_USE_UNSUPPORTED_CIPHER);
		goto f_err;
		}


	/* p points to the next byte, there are 'n' bytes left */


	/* if it was signed, check the signature */
	if (pkey != NULL)
		{
		n2s(p,i);
		n-=2;
		j=EVP_PKEY_size(pkey);

		if ((i != n) || (n > j) || (n <= 0))
			{
			/* wrong packet length */
			al=SSL_AD_DECODE_ERROR;
			SSLerr(SSL_F_SSL3_GET_KEY_EXCHANGE,SSL_R_WRONG_SIGNATURE_LENGTH);
			goto f_err;
			}

#ifndef OPENSSL_NO_RSA
		if (pkey->type == EVP_PKEY_RSA)
			{
			int num;

			j=0;
			q=md_buf;
			for (num=2; num > 0; num--)
				{
				EVP_MD_CTX_set_flags(&md_ctx,
					EVP_MD_CTX_FLAG_NON_FIPS_ALLOW);
				EVP_DigestInit_ex(&md_ctx,(num == 2)
					?s->ctx->md5:s->ctx->sha1, NULL);
				EVP_DigestUpdate(&md_ctx,&(s->s3->client_random[0]),SSL3_RANDOM_SIZE);
				EVP_DigestUpdate(&md_ctx,&(s->s3->server_random[0]),SSL3_RANDOM_SIZE);
				EVP_DigestUpdate(&md_ctx,param,param_len);
				
				EVP_DigestFinal_ex(&md_ctx,q,(unsigned int *)&i);
				q+=i;
				j+=i;
				}
			i=RSA_verify(NID_md5_sha1, md_buf, j, p, n,
								pkey->pkey.rsa);
			if (i < 0)
				{
				al=SSL_AD_DECRYPT_ERROR;
				SSLerr(SSL_F_SSL3_GET_KEY_EXCHANGE,SSL_R_BAD_RSA_DECRYPT);
				goto f_err;
				}
			if (i == 0)
				{
				/* bad signature */
				al=SSL_AD_DECRYPT_ERROR;
				SSLerr(SSL_F_SSL3_GET_KEY_EXCHANGE,SSL_R_BAD_SIGNATURE);
				goto f_err;
				}
			}
		else
#endif
#ifndef OPENSSL_NO_DSA
			if (pkey->type == EVP_PKEY_DSA)
			{
			/* lets do DSS */
			EVP_VerifyInit_ex(&md_ctx,EVP_dss1(), NULL);
			EVP_VerifyUpdate(&md_ctx,&(s->s3->client_random[0]),SSL3_RANDOM_SIZE);
			EVP_VerifyUpdate(&md_ctx,&(s->s3->server_random[0]),SSL3_RANDOM_SIZE);
			EVP_VerifyUpdate(&md_ctx,param,param_len);
			if (!EVP_VerifyFinal(&md_ctx,p,(int)n,pkey))
				{
				/* bad signature */
				al=SSL_AD_DECRYPT_ERROR;
				SSLerr(SSL_F_SSL3_GET_KEY_EXCHANGE,SSL_R_BAD_SIGNATURE);
				goto f_err;
				}
			}
		else
#endif
			{
			SSLerr(SSL_F_SSL3_GET_KEY_EXCHANGE,ERR_R_INTERNAL_ERROR);
			goto err;
			}
		}
	else
		{
		/* still data left over */
		if (!(alg & SSL_aNULL))
			{
			SSLerr(SSL_F_SSL3_GET_KEY_EXCHANGE,ERR_R_INTERNAL_ERROR);
			goto err;
			}
		if (n != 0)
			{
			al=SSL_AD_DECODE_ERROR;
			SSLerr(SSL_F_SSL3_GET_KEY_EXCHANGE,SSL_R_EXTRA_DATA_IN_MESSAGE);
			goto f_err;
			}
		}
	EVP_PKEY_free(pkey);
	EVP_MD_CTX_cleanup(&md_ctx);
	return(1);
f_err:
	ssl3_send_alert(s,SSL3_AL_FATAL,al);
err:
	EVP_PKEY_free(pkey);
#ifndef OPENSSL_NO_RSA
	if (rsa != NULL)
		RSA_free(rsa);
#endif
#ifndef OPENSSL_NO_DH
	if (dh != NULL)
		DH_free(dh);
#endif
	EVP_MD_CTX_cleanup(&md_ctx);
	return(-1);
	}

static int ssl3_get_certificate_request(SSL *s)
	{
	int ok,ret=0;
	unsigned long n,nc,l;
	unsigned int llen,ctype_num,i;
	X509_NAME *xn=NULL;
	unsigned char *p,*d,*q;
	STACK_OF(X509_NAME) *ca_sk=NULL;

	n=ssl3_get_message(s,
		SSL3_ST_CR_CERT_REQ_A,
		SSL3_ST_CR_CERT_REQ_B,
		-1,
		s->max_cert_list,
		&ok);

	if (!ok) return((int)n);

	s->s3->tmp.cert_req=0;

	if (s->s3->tmp.message_type == SSL3_MT_SERVER_DONE)
		{
		s->s3->tmp.reuse_message=1;
		return(1);
		}

	if (s->s3->tmp.message_type != SSL3_MT_CERTIFICATE_REQUEST)
		{
		ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_UNEXPECTED_MESSAGE);
		SSLerr(SSL_F_SSL3_GET_CERTIFICATE_REQUEST,SSL_R_WRONG_MESSAGE_TYPE);
		goto err;
		}

	/* TLS does not like anon-DH with client cert */
	if (s->version > SSL3_VERSION)
		{
		l=s->s3->tmp.new_cipher->algorithms;
		if (l & SSL_aNULL)
			{
			ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_UNEXPECTED_MESSAGE);
			SSLerr(SSL_F_SSL3_GET_CERTIFICATE_REQUEST,SSL_R_TLS_CLIENT_CERT_REQ_WITH_ANON_CIPHER);
			goto err;
			}
		}

	d=p=(unsigned char *)s->init_msg;

	if ((ca_sk=sk_X509_NAME_new(ca_dn_cmp)) == NULL)
		{
		SSLerr(SSL_F_SSL3_GET_CERTIFICATE_REQUEST,ERR_R_MALLOC_FAILURE);
		goto err;
		}

	/* get the certificate types */
	ctype_num= *(p++);
	if (ctype_num > SSL3_CT_NUMBER)
		ctype_num=SSL3_CT_NUMBER;
	for (i=0; i<ctype_num; i++)
		s->s3->tmp.ctype[i]= p[i];
	p+=ctype_num;

	/* get the CA RDNs */
	n2s(p,llen);
#if 0
{
FILE *out;
out=fopen("/tmp/vsign.der","w");
fwrite(p,1,llen,out);
fclose(out);
}
#endif

	if ((llen+ctype_num+2+1) != n)
		{
		ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_DECODE_ERROR);
		SSLerr(SSL_F_SSL3_GET_CERTIFICATE_REQUEST,SSL_R_LENGTH_MISMATCH);
		goto err;
		}

	for (nc=0; nc<llen; )
		{
		n2s(p,l);
		if ((l+nc+2) > llen)
			{
			if ((s->options & SSL_OP_NETSCAPE_CA_DN_BUG))
				goto cont; /* netscape bugs */
			ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_DECODE_ERROR);
			SSLerr(SSL_F_SSL3_GET_CERTIFICATE_REQUEST,SSL_R_CA_DN_TOO_LONG);
			goto err;
			}

		q=p;

		if ((xn=d2i_X509_NAME(NULL,&q,l)) == NULL)
			{
			/* If netscape tolerance is on, ignore errors */
			if (s->options & SSL_OP_NETSCAPE_CA_DN_BUG)
				goto cont;
			else
				{
				ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_DECODE_ERROR);
				SSLerr(SSL_F_SSL3_GET_CERTIFICATE_REQUEST,ERR_R_ASN1_LIB);
				goto err;
				}
			}

		if (q != (p+l))
			{
			ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_DECODE_ERROR);
			SSLerr(SSL_F_SSL3_GET_CERTIFICATE_REQUEST,SSL_R_CA_DN_LENGTH_MISMATCH);
			goto err;
			}
		if (!sk_X509_NAME_push(ca_sk,xn))
			{
			SSLerr(SSL_F_SSL3_GET_CERTIFICATE_REQUEST,ERR_R_MALLOC_FAILURE);
			goto err;
			}

		p+=l;
		nc+=l+2;
		}

	if (0)
		{
cont:
		ERR_clear_error();
		}

	/* we should setup a certificate to return.... */
	s->s3->tmp.cert_req=1;
	s->s3->tmp.ctype_num=ctype_num;
	if (s->s3->tmp.ca_names != NULL)
		sk_X509_NAME_pop_free(s->s3->tmp.ca_names,X509_NAME_free);
	s->s3->tmp.ca_names=ca_sk;
	ca_sk=NULL;

	ret=1;
err:
	if (ca_sk != NULL) sk_X509_NAME_pop_free(ca_sk,X509_NAME_free);
	return(ret);
	}

static int ca_dn_cmp(const X509_NAME * const *a, const X509_NAME * const *b)
	{
	return(X509_NAME_cmp(*a,*b));
	}

static int ssl3_get_server_done(SSL *s)
	{
	int ok,ret=0;
	long n;

	n=ssl3_get_message(s,
		SSL3_ST_CR_SRVR_DONE_A,
		SSL3_ST_CR_SRVR_DONE_B,
		SSL3_MT_SERVER_DONE,
		30, /* should be very small, like 0 :-) */
		&ok);

	if (!ok) return((int)n);
	if (n > 0)
		{
		/* should contain no data */
		ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_DECODE_ERROR);
		SSLerr(SSL_F_SSL3_GET_SERVER_DONE,SSL_R_LENGTH_MISMATCH);
		return -1;
		}
	ret=1;
	return(ret);
	}

static int ssl3_send_client_key_exchange(SSL *s)
	{
	unsigned char *p,*d;
	int n;
	unsigned long l;
#ifndef OPENSSL_NO_RSA
	unsigned char *q;
	EVP_PKEY *pkey=NULL;
#endif
#ifndef OPENSSL_NO_KRB5
        KSSL_ERR kssl_err;
#endif /* OPENSSL_NO_KRB5 */

	if (s->state == SSL3_ST_CW_KEY_EXCH_A)
		{
		d=(unsigned char *)s->init_buf->data;
		p= &(d[4]);

		l=s->s3->tmp.new_cipher->algorithms;

                /* Fool emacs indentation */
                if (0) {}
#ifndef OPENSSL_NO_RSA
		else if (l & SSL_kRSA)
			{
			RSA *rsa;
			unsigned char tmp_buf[SSL_MAX_MASTER_KEY_LENGTH];

			if (s->session->sess_cert->peer_rsa_tmp != NULL)
				rsa=s->session->sess_cert->peer_rsa_tmp;
			else
				{
				pkey=X509_get_pubkey(s->session->sess_cert->peer_pkeys[SSL_PKEY_RSA_ENC].x509);
				if ((pkey == NULL) ||
					(pkey->type != EVP_PKEY_RSA) ||
					(pkey->pkey.rsa == NULL))
					{
					SSLerr(SSL_F_SSL3_SEND_CLIENT_KEY_EXCHANGE,ERR_R_INTERNAL_ERROR);
					goto err;
					}
				rsa=pkey->pkey.rsa;
				EVP_PKEY_free(pkey);
				}
				
			tmp_buf[0]=s->client_version>>8;
			tmp_buf[1]=s->client_version&0xff;
			if (RAND_bytes(&(tmp_buf[2]),sizeof tmp_buf-2) <= 0)
					goto err;

			s->session->master_key_length=sizeof tmp_buf;

			q=p;
			/* Fix buf for TLS and beyond */
			if (s->version > SSL3_VERSION)
				p+=2;
			n=RSA_public_encrypt(sizeof tmp_buf,
				tmp_buf,p,rsa,RSA_PKCS1_PADDING);
#ifdef PKCS1_CHECK
			if (s->options & SSL_OP_PKCS1_CHECK_1) p[1]++;
			if (s->options & SSL_OP_PKCS1_CHECK_2) tmp_buf[0]=0x70;
#endif
			if (n <= 0)
				{
				SSLerr(SSL_F_SSL3_SEND_CLIENT_KEY_EXCHANGE,SSL_R_BAD_RSA_ENCRYPT);
				goto err;
				}

			/* Fix buf for TLS and beyond */
			if (s->version > SSL3_VERSION)
				{
				s2n(n,q);
				n+=2;
				}

			s->session->master_key_length=
				s->method->ssl3_enc->generate_master_secret(s,
					s->session->master_key,
					tmp_buf,sizeof tmp_buf);
			OPENSSL_cleanse(tmp_buf,sizeof tmp_buf);
			}
#endif
#ifndef OPENSSL_NO_KRB5
		else if (l & SSL_kKRB5)
                        {
                        krb5_error_code	krb5rc;
                        KSSL_CTX	*kssl_ctx = s->kssl_ctx;
                        /*  krb5_data	krb5_ap_req;  */
                        krb5_data	*enc_ticket;
                        krb5_data	authenticator, *authp = NULL;
			EVP_CIPHER_CTX	ciph_ctx;
			EVP_CIPHER	*enc = NULL;
			unsigned char	iv[EVP_MAX_IV_LENGTH];
			unsigned char	tmp_buf[SSL_MAX_MASTER_KEY_LENGTH];
			unsigned char	epms[SSL_MAX_MASTER_KEY_LENGTH 
						+ EVP_MAX_IV_LENGTH];
			int 		padl, outl = sizeof(epms);

			EVP_CIPHER_CTX_init(&ciph_ctx);

#ifdef KSSL_DEBUG
                        printf("ssl3_send_client_key_exchange(%lx & %lx)\n",
                                l, SSL_kKRB5);
#endif	/* KSSL_DEBUG */

			authp = NULL;
#ifdef KRB5SENDAUTH
			if (KRB5SENDAUTH)  authp = &authenticator;
#endif	/* KRB5SENDAUTH */

                        krb5rc = kssl_cget_tkt(kssl_ctx, &enc_ticket, authp,
				&kssl_err);
			enc = kssl_map_enc(kssl_ctx->enctype);
                        if (enc == NULL)
                            goto err;
#ifdef KSSL_DEBUG
                        {
                        printf("kssl_cget_tkt rtn %d\n", krb5rc);
                        if (krb5rc && kssl_err.text)
			  printf("kssl_cget_tkt kssl_err=%s\n", kssl_err.text);
                        }
#endif	/* KSSL_DEBUG */

                        if (krb5rc)
                                {
                                ssl3_send_alert(s,SSL3_AL_FATAL,
						SSL_AD_HANDSHAKE_FAILURE);
                                SSLerr(SSL_F_SSL3_SEND_CLIENT_KEY_EXCHANGE,
						kssl_err.reason);
                                goto err;
                                }

			/*  20010406 VRS - Earlier versions used KRB5 AP_REQ
			**  in place of RFC 2712 KerberosWrapper, as in:
			**
                        **  Send ticket (copy to *p, set n = length)
                        **  n = krb5_ap_req.length;
                        **  memcpy(p, krb5_ap_req.data, krb5_ap_req.length);
                        **  if (krb5_ap_req.data)  
                        **    kssl_krb5_free_data_contents(NULL,&krb5_ap_req);
                        **
			**  Now using real RFC 2712 KerberosWrapper
			**  (Thanks to Simon Wilkinson <sxw@sxw.org.uk>)
			**  Note: 2712 "opaque" types are here replaced
			**  with a 2-byte length followed by the value.
			**  Example:
			**  KerberosWrapper= xx xx asn1ticket 0 0 xx xx encpms
			**  Where "xx xx" = length bytes.  Shown here with
			**  optional authenticator omitted.
			*/

			/*  KerberosWrapper.Ticket		*/
			s2n(enc_ticket->length,p);
			memcpy(p, enc_ticket->data, enc_ticket->length);
			p+= enc_ticket->length;
			n = enc_ticket->length + 2;

			/*  KerberosWrapper.Authenticator	*/
			if (authp  &&  authp->length)  
				{
				s2n(authp->length,p);
				memcpy(p, authp->data, authp->length);
				p+= authp->length;
				n+= authp->length + 2;
				
				free(authp->data);
				authp->data = NULL;
				authp->length = 0;
				}
			else
				{
				s2n(0,p);/*  null authenticator length	*/
				n+=2;
				}
 
			if (RAND_bytes(tmp_buf,sizeof tmp_buf) <= 0)
			    goto err;

			/*  20010420 VRS.  Tried it this way; failed.
			**	EVP_EncryptInit_ex(&ciph_ctx,enc, NULL,NULL);
			**	EVP_CIPHER_CTX_set_key_length(&ciph_ctx,
			**				kssl_ctx->length);
			**	EVP_EncryptInit_ex(&ciph_ctx,NULL, key,iv);
			*/

			memset(iv, 0, sizeof iv);  /* per RFC 1510 */
			EVP_EncryptInit_ex(&ciph_ctx,enc, NULL,
				kssl_ctx->key,iv);
			EVP_EncryptUpdate(&ciph_ctx,epms,&outl,tmp_buf,
				sizeof tmp_buf);
			EVP_EncryptFinal_ex(&ciph_ctx,&(epms[outl]),&padl);
			outl += padl;
			if (outl > sizeof epms)
				{
				SSLerr(SSL_F_SSL3_SEND_CLIENT_KEY_EXCHANGE, ERR_R_INTERNAL_ERROR);
				goto err;
				}
			EVP_CIPHER_CTX_cleanup(&ciph_ctx);

			/*  KerberosWrapper.EncryptedPreMasterSecret	*/
			s2n(outl,p);
			memcpy(p, epms, outl);
			p+=outl;
			n+=outl + 2;

                        s->session->master_key_length=
                                s->method->ssl3_enc->generate_master_secret(s,
					s->session->master_key,
					tmp_buf, sizeof tmp_buf);

			OPENSSL_cleanse(tmp_buf, sizeof tmp_buf);
			OPENSSL_cleanse(epms, outl);
                        }
#endif
#ifndef OPENSSL_NO_DH
		else if (l & (SSL_kEDH|SSL_kDHr|SSL_kDHd))
			{
			DH *dh_srvr,*dh_clnt;

			if (s->session->sess_cert->peer_dh_tmp != NULL)
				dh_srvr=s->session->sess_cert->peer_dh_tmp;
			else
				{
				/* we get them from the cert */
				ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_HANDSHAKE_FAILURE);
				SSLerr(SSL_F_SSL3_SEND_CLIENT_KEY_EXCHANGE,SSL_R_UNABLE_TO_FIND_DH_PARAMETERS);
				goto err;
				}
			
			/* generate a new random key */
			if ((dh_clnt=DHparams_dup(dh_srvr)) == NULL)
				{
				SSLerr(SSL_F_SSL3_SEND_CLIENT_KEY_EXCHANGE,ERR_R_DH_LIB);
				goto err;
				}
			if (!DH_generate_key(dh_clnt))
				{
				SSLerr(SSL_F_SSL3_SEND_CLIENT_KEY_EXCHANGE,ERR_R_DH_LIB);
				goto err;
				}

			/* use the 'p' output buffer for the DH key, but
			 * make sure to clear it out afterwards */

			n=DH_compute_key(p,dh_srvr->pub_key,dh_clnt);

			if (n <= 0)
				{
				SSLerr(SSL_F_SSL3_SEND_CLIENT_KEY_EXCHANGE,ERR_R_DH_LIB);
				goto err;
				}

			/* generate master key from the result */
			s->session->master_key_length=
				s->method->ssl3_enc->generate_master_secret(s,
					s->session->master_key,p,n);
			/* clean up */
			memset(p,0,n);

			/* send off the data */
			n=BN_num_bytes(dh_clnt->pub_key);
			s2n(n,p);
			BN_bn2bin(dh_clnt->pub_key,p);
			n+=2;

			DH_free(dh_clnt);

			/* perhaps clean things up a bit EAY EAY EAY EAY*/
			}
#endif
		else
			{
			ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_HANDSHAKE_FAILURE);
			SSLerr(SSL_F_SSL3_SEND_CLIENT_KEY_EXCHANGE,ERR_R_INTERNAL_ERROR);
			goto err;
			}
		
		*(d++)=SSL3_MT_CLIENT_KEY_EXCHANGE;
		l2n3(n,d);

		s->state=SSL3_ST_CW_KEY_EXCH_B;
		/* number of bytes to write */
		s->init_num=n+4;
		s->init_off=0;
		}

	/* SSL3_ST_CW_KEY_EXCH_B */
	return(ssl3_do_write(s,SSL3_RT_HANDSHAKE));
err:
	return(-1);
	}

static int ssl3_send_client_verify(SSL *s)
	{
	unsigned char *p,*d;
	unsigned char data[MD5_DIGEST_LENGTH+SHA_DIGEST_LENGTH];
	EVP_PKEY *pkey;
#ifndef OPENSSL_NO_RSA
	unsigned u=0;
#endif
	unsigned long n;
#ifndef OPENSSL_NO_DSA
	int j;
#endif

	if (s->state == SSL3_ST_CW_CERT_VRFY_A)
		{
		d=(unsigned char *)s->init_buf->data;
		p= &(d[4]);
		pkey=s->cert->key->privatekey;

		s->method->ssl3_enc->cert_verify_mac(s,&(s->s3->finish_dgst2),
			&(data[MD5_DIGEST_LENGTH]));

#ifndef OPENSSL_NO_RSA
		if (pkey->type == EVP_PKEY_RSA)
			{
			s->method->ssl3_enc->cert_verify_mac(s,
				&(s->s3->finish_dgst1),&(data[0]));
			if (RSA_sign(NID_md5_sha1, data,
					 MD5_DIGEST_LENGTH+SHA_DIGEST_LENGTH,
					&(p[2]), &u, pkey->pkey.rsa) <= 0 )
				{
				SSLerr(SSL_F_SSL3_SEND_CLIENT_VERIFY,ERR_R_RSA_LIB);
				goto err;
				}
			s2n(u,p);
			n=u+2;
			}
		else
#endif
#ifndef OPENSSL_NO_DSA
			if (pkey->type == EVP_PKEY_DSA)
			{
			if (!DSA_sign(pkey->save_type,
				&(data[MD5_DIGEST_LENGTH]),
				SHA_DIGEST_LENGTH,&(p[2]),
				(unsigned int *)&j,pkey->pkey.dsa))
				{
				SSLerr(SSL_F_SSL3_SEND_CLIENT_VERIFY,ERR_R_DSA_LIB);
				goto err;
				}
			s2n(j,p);
			n=j+2;
			}
		else
#endif
			{
			SSLerr(SSL_F_SSL3_SEND_CLIENT_VERIFY,ERR_R_INTERNAL_ERROR);
			goto err;
			}
		*(d++)=SSL3_MT_CERTIFICATE_VERIFY;
		l2n3(n,d);

		s->state=SSL3_ST_CW_CERT_VRFY_B;
		s->init_num=(int)n+4;
		s->init_off=0;
		}
	return(ssl3_do_write(s,SSL3_RT_HANDSHAKE));
err:
	return(-1);
	}

static int ssl3_send_client_certificate(SSL *s)
	{
	X509 *x509=NULL;
	EVP_PKEY *pkey=NULL;
	int i;
	unsigned long l;

	if (s->state ==	SSL3_ST_CW_CERT_A)
		{
		if ((s->cert == NULL) ||
			(s->cert->key->x509 == NULL) ||
			(s->cert->key->privatekey == NULL))
			s->state=SSL3_ST_CW_CERT_B;
		else
			s->state=SSL3_ST_CW_CERT_C;
		}

	/* We need to get a client cert */
	if (s->state == SSL3_ST_CW_CERT_B)
		{
		/* If we get an error, we need to
		 * ssl->rwstate=SSL_X509_LOOKUP; return(-1);
		 * We then get retied later */
		i=0;
		if (s->ctx->client_cert_cb != NULL)
			i=s->ctx->client_cert_cb(s,&(x509),&(pkey));
		if (i < 0)
			{
			s->rwstate=SSL_X509_LOOKUP;
			return(-1);
			}
		s->rwstate=SSL_NOTHING;
		if ((i == 1) && (pkey != NULL) && (x509 != NULL))
			{
			s->state=SSL3_ST_CW_CERT_B;
			if (	!SSL_use_certificate(s,x509) ||
				!SSL_use_PrivateKey(s,pkey))
				i=0;
			}
		else if (i == 1)
			{
			i=0;
			SSLerr(SSL_F_SSL3_SEND_CLIENT_CERTIFICATE,SSL_R_BAD_DATA_RETURNED_BY_CALLBACK);
			}

		if (x509 != NULL) X509_free(x509);
		if (pkey != NULL) EVP_PKEY_free(pkey);
		if (i == 0)
			{
			if (s->version == SSL3_VERSION)
				{
				s->s3->tmp.cert_req=0;
				ssl3_send_alert(s,SSL3_AL_WARNING,SSL_AD_NO_CERTIFICATE);
				return(1);
				}
			else
				{
				s->s3->tmp.cert_req=2;
				}
			}

		/* Ok, we have a cert */
		s->state=SSL3_ST_CW_CERT_C;
		}

	if (s->state == SSL3_ST_CW_CERT_C)
		{
		s->state=SSL3_ST_CW_CERT_D;
		l=ssl3_output_cert_chain(s,
			(s->s3->tmp.cert_req == 2)?NULL:s->cert->key->x509);
		s->init_num=(int)l;
		s->init_off=0;
		}
	/* SSL3_ST_CW_CERT_D */
	return(ssl3_do_write(s,SSL3_RT_HANDSHAKE));
	}

#define has_bits(i,m)	(((i)&(m)) == (m))

static int ssl3_check_cert_and_algorithm(SSL *s)
	{
	int i,idx;
	long algs;
	EVP_PKEY *pkey=NULL;
	SESS_CERT *sc;
#ifndef OPENSSL_NO_RSA
	RSA *rsa;
#endif
#ifndef OPENSSL_NO_DH
	DH *dh;
#endif

	sc=s->session->sess_cert;

	if (sc == NULL)
		{
		SSLerr(SSL_F_SSL3_CHECK_CERT_AND_ALGORITHM,ERR_R_INTERNAL_ERROR);
		goto err;
		}

	algs=s->s3->tmp.new_cipher->algorithms;

	/* we don't have a certificate */
	if (algs & (SSL_aDH|SSL_aNULL|SSL_aKRB5))
		return(1);

#ifndef OPENSSL_NO_RSA
	rsa=s->session->sess_cert->peer_rsa_tmp;
#endif
#ifndef OPENSSL_NO_DH
	dh=s->session->sess_cert->peer_dh_tmp;
#endif

	/* This is the passed certificate */

	idx=sc->peer_cert_type;
	pkey=X509_get_pubkey(sc->peer_pkeys[idx].x509);
	i=X509_certificate_type(sc->peer_pkeys[idx].x509,pkey);
	EVP_PKEY_free(pkey);

	
	/* Check that we have a certificate if we require one */
	if ((algs & SSL_aRSA) && !has_bits(i,EVP_PK_RSA|EVP_PKT_SIGN))
		{
		SSLerr(SSL_F_SSL3_CHECK_CERT_AND_ALGORITHM,SSL_R_MISSING_RSA_SIGNING_CERT);
		goto f_err;
		}
#ifndef OPENSSL_NO_DSA
	else if ((algs & SSL_aDSS) && !has_bits(i,EVP_PK_DSA|EVP_PKT_SIGN))
		{
		SSLerr(SSL_F_SSL3_CHECK_CERT_AND_ALGORITHM,SSL_R_MISSING_DSA_SIGNING_CERT);
		goto f_err;
		}
#endif
#ifndef OPENSSL_NO_RSA
	if ((algs & SSL_kRSA) &&
		!(has_bits(i,EVP_PK_RSA|EVP_PKT_ENC) || (rsa != NULL)))
		{
		SSLerr(SSL_F_SSL3_CHECK_CERT_AND_ALGORITHM,SSL_R_MISSING_RSA_ENCRYPTING_CERT);
		goto f_err;
		}
#endif
#ifndef OPENSSL_NO_DH
	if ((algs & SSL_kEDH) &&
		!(has_bits(i,EVP_PK_DH|EVP_PKT_EXCH) || (dh != NULL)))
		{
		SSLerr(SSL_F_SSL3_CHECK_CERT_AND_ALGORITHM,SSL_R_MISSING_DH_KEY);
		goto f_err;
		}
	else if ((algs & SSL_kDHr) && !has_bits(i,EVP_PK_DH|EVP_PKS_RSA))
		{
		SSLerr(SSL_F_SSL3_CHECK_CERT_AND_ALGORITHM,SSL_R_MISSING_DH_RSA_CERT);
		goto f_err;
		}
#ifndef OPENSSL_NO_DSA
	else if ((algs & SSL_kDHd) && !has_bits(i,EVP_PK_DH|EVP_PKS_DSA))
		{
		SSLerr(SSL_F_SSL3_CHECK_CERT_AND_ALGORITHM,SSL_R_MISSING_DH_DSA_CERT);
		goto f_err;
		}
#endif
#endif

	if (SSL_C_IS_EXPORT(s->s3->tmp.new_cipher) && !has_bits(i,EVP_PKT_EXP))
		{
#ifndef OPENSSL_NO_RSA
		if (algs & SSL_kRSA)
			{
			if (rsa == NULL
			    || RSA_size(rsa)*8 > SSL_C_EXPORT_PKEYLENGTH(s->s3->tmp.new_cipher))
				{
				SSLerr(SSL_F_SSL3_CHECK_CERT_AND_ALGORITHM,SSL_R_MISSING_EXPORT_TMP_RSA_KEY);
				goto f_err;
				}
			}
		else
#endif
#ifndef OPENSSL_NO_DH
			if (algs & (SSL_kEDH|SSL_kDHr|SSL_kDHd))
			    {
			    if (dh == NULL
				|| DH_size(dh)*8 > SSL_C_EXPORT_PKEYLENGTH(s->s3->tmp.new_cipher))
				{
				SSLerr(SSL_F_SSL3_CHECK_CERT_AND_ALGORITHM,SSL_R_MISSING_EXPORT_TMP_DH_KEY);
				goto f_err;
				}
			}
		else
#endif
			{
			SSLerr(SSL_F_SSL3_CHECK_CERT_AND_ALGORITHM,SSL_R_UNKNOWN_KEY_EXCHANGE_TYPE);
			goto f_err;
			}
		}
	return(1);
f_err:
	ssl3_send_alert(s,SSL3_AL_FATAL,SSL_AD_HANDSHAKE_FAILURE);
err:
	return(0);
	}

