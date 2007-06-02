/**
 * @file ssl-gnutls.c GNUTLS SSL plugin.
 *
 * purple
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "internal.h"
#include "debug.h"
#include "certificate.h"
#include "plugin.h"
#include "sslconn.h"
#include "version.h"
#include "util.h"

#define SSL_GNUTLS_PLUGIN_ID "ssl-gnutls"

#ifdef HAVE_GNUTLS

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

typedef struct
{
	gnutls_session session;
	guint handshake_handler;
} PurpleSslGnutlsData;

#define PURPLE_SSL_GNUTLS_DATA(gsc) ((PurpleSslGnutlsData *)gsc->private_data)

static gnutls_certificate_client_credentials xcred;

static void
ssl_gnutls_init_gnutls(void)
{
	/* Configure GnuTLS to use glib memory management */
	/* I expect that this isn't really necessary, but it may prevent
	   some bugs */
	/* TODO: It may be necessary to wrap this allocators for GnuTLS.
	   If there are strange bugs, perhaps look here (yes, I am a
	   hypocrite) */
	gnutls_global_set_mem_functions(
		(gnutls_alloc_function)   g_malloc0, /* malloc */
		(gnutls_alloc_function)   g_malloc0, /* secure malloc */
		NULL,      /* mem_is_secure */
		(gnutls_realloc_function) g_realloc, /* realloc */
		(gnutls_free_function)    g_free     /* free */
		);
	
	gnutls_global_init();

	gnutls_certificate_allocate_credentials(&xcred);

	/* TODO: I can likely remove this */
	gnutls_certificate_set_x509_trust_file(xcred, "ca.pem",
		GNUTLS_X509_FMT_PEM);
}

static gboolean
ssl_gnutls_init(void)
{
   return TRUE;
}

static void
ssl_gnutls_uninit(void)
{
	gnutls_global_deinit();

	gnutls_certificate_free_credentials(xcred);
}


static void ssl_gnutls_handshake_cb(gpointer data, gint source,
		PurpleInputCondition cond)
{
	PurpleSslConnection *gsc = data;
	PurpleSslGnutlsData *gnutls_data = PURPLE_SSL_GNUTLS_DATA(gsc);
	ssize_t ret;

	purple_debug_info("gnutls", "Handshaking\n");
	ret = gnutls_handshake(gnutls_data->session);

	if(ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED)
		return;

	purple_input_remove(gnutls_data->handshake_handler);
	gnutls_data->handshake_handler = 0;

	if(ret != 0) {
		purple_debug_error("gnutls", "Handshake failed. Error %s\n",
			gnutls_strerror(ret));

		if(gsc->error_cb != NULL)
			gsc->error_cb(gsc, PURPLE_SSL_HANDSHAKE_FAILED,
				gsc->connect_cb_data);

		purple_ssl_close(gsc);
	} else {
		purple_debug_info("gnutls", "Handshake complete\n");

		{
		  const gnutls_datum_t *cert_list;
		  unsigned int cert_list_size = 0;
		  gnutls_session_t session=gnutls_data->session;
		  
		  cert_list =
		    gnutls_certificate_get_peers(session, &cert_list_size);
		  
		  purple_debug_info("gnutls",
				    "Peer provided %d certs\n",
				    cert_list_size);
		  int i;
		  for (i=0; i<cert_list_size; i++)
		    {
		      gchar fpr_bin[256];
		      gsize fpr_bin_sz = sizeof(fpr_bin);
		      gchar * fpr_asc = NULL;
		      gchar tbuf[256];
		      gsize tsz=sizeof(tbuf);
		      gchar * tasc = NULL;
		      gnutls_x509_crt_t cert;
		      
		      gnutls_x509_crt_init(&cert);
		      gnutls_x509_crt_import (cert, &cert_list[i],
					      GNUTLS_X509_FMT_DER);
		      
		      gnutls_x509_crt_get_fingerprint(cert, GNUTLS_MAC_SHA,
						      fpr_bin, &fpr_bin_sz);
		      
		      fpr_asc =
			purple_base16_encode_chunked(fpr_bin,fpr_bin_sz);
		      
		      purple_debug_info("gnutls", 
					"Lvl %d SHA1 fingerprint: %s\n",
					i, fpr_asc);
		      
		      tsz=sizeof(tbuf);
		      gnutls_x509_crt_get_serial(cert,tbuf,&tsz);
		      tasc=
			purple_base16_encode_chunked(tbuf, tsz);
		      purple_debug_info("gnutls",
					"Serial: %s\n",
					tasc);
		      g_free(tasc);

		      tsz=sizeof(tbuf);
		      gnutls_x509_crt_get_dn (cert, tbuf, &tsz);
		      purple_debug_info("gnutls",
					"Cert DN: %s\n",
					tbuf);
		      tsz=sizeof(tbuf);
		      gnutls_x509_crt_get_issuer_dn (cert, tbuf, &tsz);
		      purple_debug_info("gnutls",
					"Cert Issuer DN: %s\n",
					tbuf);

		      g_free(fpr_asc); fpr_asc = NULL;
		      gnutls_x509_crt_deinit(cert);
		    }
		  
		}
		gsc->connect_cb(gsc->connect_cb_data, gsc, cond);
	}

}


static void
ssl_gnutls_connect(PurpleSslConnection *gsc)
{
	PurpleSslGnutlsData *gnutls_data;
	static const int cert_type_priority[2] = { GNUTLS_CRT_X509, 0 };

	gnutls_data = g_new0(PurpleSslGnutlsData, 1);
	gsc->private_data = gnutls_data;

	gnutls_init(&gnutls_data->session, GNUTLS_CLIENT);
	gnutls_set_default_priority(gnutls_data->session);

	gnutls_certificate_type_set_priority(gnutls_data->session,
		cert_type_priority);

	gnutls_credentials_set(gnutls_data->session, GNUTLS_CRD_CERTIFICATE,
		xcred);

	gnutls_transport_set_ptr(gnutls_data->session, GINT_TO_POINTER(gsc->fd));

	gnutls_data->handshake_handler = purple_input_add(gsc->fd,
		PURPLE_INPUT_READ, ssl_gnutls_handshake_cb, gsc);

	/* Orborde asks: Why are we configuring a callback, then
	   immediately calling it?

	   Answer: gnutls_handshake (up in handshake_cb) needs to be called
	   once in order to get the ball rolling on the SSL connection.
	   Once it has done so, only then will the server reply, triggering
	   the callback.

	   Since the logic driving gnutls_handshake is the same with the first
	   and subsequent calls, we'll just fire the callback immediately to
	   accomplish this.
	*/
	ssl_gnutls_handshake_cb(gsc, gsc->fd, PURPLE_INPUT_READ);
}

static void
ssl_gnutls_close(PurpleSslConnection *gsc)
{
	PurpleSslGnutlsData *gnutls_data = PURPLE_SSL_GNUTLS_DATA(gsc);

	if(!gnutls_data)
		return;

	if(gnutls_data->handshake_handler)
		purple_input_remove(gnutls_data->handshake_handler);

	gnutls_bye(gnutls_data->session, GNUTLS_SHUT_RDWR);

	gnutls_deinit(gnutls_data->session);

	g_free(gnutls_data);
	gsc->private_data = NULL;
}

static size_t
ssl_gnutls_read(PurpleSslConnection *gsc, void *data, size_t len)
{
	PurpleSslGnutlsData *gnutls_data = PURPLE_SSL_GNUTLS_DATA(gsc);
	ssize_t s;

	s = gnutls_record_recv(gnutls_data->session, data, len);

	if(s == GNUTLS_E_AGAIN || s == GNUTLS_E_INTERRUPTED) {
		s = -1;
		errno = EAGAIN;
	} else if(s < 0) {
		purple_debug_error("gnutls", "receive failed: %s\n",
				gnutls_strerror(s));
		s = -1;
		/*
		 * TODO: Set errno to something more appropriate.  Or even
		 *       better: allow ssl plugins to keep track of their
		 *       own error message, then add a new ssl_ops function
		 *       that returns the error message.
		 */
		errno = EIO;
	}

	return s;
}

static size_t
ssl_gnutls_write(PurpleSslConnection *gsc, const void *data, size_t len)
{
	PurpleSslGnutlsData *gnutls_data = PURPLE_SSL_GNUTLS_DATA(gsc);
	ssize_t s = 0;

	/* XXX: when will gnutls_data be NULL? */
	if(gnutls_data)
		s = gnutls_record_send(gnutls_data->session, data, len);

	if(s == GNUTLS_E_AGAIN || s == GNUTLS_E_INTERRUPTED) {
		s = -1;
		errno = EAGAIN;
	} else if(s < 0) {
		purple_debug_error("gnutls", "send failed: %s\n",
				gnutls_strerror(s));
		s = -1;
		/*
		 * TODO: Set errno to something more appropriate.  Or even
		 *       better: allow ssl plugins to keep track of their
		 *       own error message, then add a new ssl_ops function
		 *       that returns the error message.
		 */
		errno = EIO;
	}

	return s;
}

/************************************************************************/
/* X.509 functionality                                                  */
/************************************************************************/
const gchar * SCHEME_NAME = "x509";

/* X.509 certificate operations provided by this plugin */
/* TODO: Flesh this out! */
static CertificateScheme x509_gnutls = {
	"x509"   /* Scheme name */
};

/** Imports a PEM-formatted X.509 certificate from the specified file.
 * @param filename Filename to import from. Format is PEM
 *
 * @return A newly allocated Certificate structure of the x509_gnutls scheme
 */
static Certificate *
x509_import_from_file(const gchar * filename)
{
	Certificate *crt;  /* Certificate being constructed */
	gchar *buf;        /* Used to load the raw file data */
	gsize buf_sz;      /* Size of the above */
	gnutls_datum_t dt; /* Struct to pass to GnuTLS */

	/* Internal certificate data structure */
	gnutls_x509_crt_t *certdat;

	purple_debug_info("gnutls",
			  "Attempting to load X.509 certificate from %s\n",
			  filename);
	
	/* Next, we'll simply yank the entire contents of the file
	   into memory */
	/* TODO: Should I worry about very large files here? */
	/* TODO: Error checking */
	g_file_get_contents(filename,
			    &buf,
			    &buf_sz,
			    NULL      /* No error checking for now */
		);
	
	/* Allocate and prepare the internal certificate data */
	certdat = g_new(gnutls_x509_crt_t, 1);
	gnutls_x509_crt_init(certdat);
	
	/* Load the datum struct */
	dt.data = (unsigned char *) buf;
	dt.size = buf_sz;

	/* Perform the actual certificate parse */
	/* Yes, certdat SHOULD be dereferenced */
	gnutls_x509_crt_import(*certdat, &dt, GNUTLS_X509_FMT_PEM);
	
	/* Allocate the certificate and load it with data */
	crt = g_new(Certificate, 1);
	crt->scheme = &x509_gnutls;
	crt->data = certdat;

	/* Cleanup */
	g_free(buf);

	return crt;
}

/** Frees a Certificate
 *
 *  Destroys a Certificate's internal data structures and frees the pointer
 *  given.
 *  @param crt  Certificate instance to be destroyed. It WILL NOT be destroyed
 *              if it is not of the correct CertificateScheme. Can be NULL
 *
 */
static void
x509_destroy_certificate(Certificate * crt)
{
	/* TODO: Issue a warning here? */
	if (NULL == crt) return;

	/* Check that the scheme is x509_gnutls */
	if ( crt->scheme != &x509_gnutls ) {
		purple_debug_error("gnutls",
				   "destroy_certificate attempted on certificate of wrong scheme (scheme was %s, expected %s)\n",
				   crt->scheme->name,
				   SCHEME_NAME);
		return;
	}

	/* TODO: Different error checking? */
	g_return_if_fail(crt->data != NULL);
	g_return_if_fail(crt->scheme != NULL);

	/* Destroy the GnuTLS-specific data */
	gnutls_x509_crt_deinit( *( (gnutls_x509_crt_t *) crt->data ) );
	g_free(crt->data);

	/* TODO: Reference counting here? */

	/* Kill the structure itself */
	g_free(crt);
}

static PurpleSslOps ssl_ops =
{
	ssl_gnutls_init,
	ssl_gnutls_uninit,
	ssl_gnutls_connect,
	ssl_gnutls_close,
	ssl_gnutls_read,
	ssl_gnutls_write,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

#endif /* HAVE_GNUTLS */

static gboolean
plugin_load(PurplePlugin *plugin)
{
#ifdef HAVE_GNUTLS
	if(!purple_ssl_get_ops()) {
		purple_ssl_set_ops(&ssl_ops);
	}

	/* Init GNUTLS now so others can use it even if sslconn never does */
	ssl_gnutls_init_gnutls();

	return TRUE;
#else
	return FALSE;
#endif
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
#ifdef HAVE_GNUTLS
	if(purple_ssl_get_ops() == &ssl_ops) {
		purple_ssl_set_ops(NULL);
	}
#endif

	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	PURPLE_PLUGIN_FLAG_INVISIBLE,                       /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                            /**< priority       */

	SSL_GNUTLS_PLUGIN_ID,                             /**< id             */
	N_("GNUTLS"),                                     /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Provides SSL support through GNUTLS."),
	                                                  /**  description    */
	N_("Provides SSL support through GNUTLS."),
	"Christian Hammond <chipx86@gnupdate.org>",
	PURPLE_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,                                             /**< prefs_info     */
	NULL,                                             /**< actions        */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(ssl_gnutls, init_plugin, info)
