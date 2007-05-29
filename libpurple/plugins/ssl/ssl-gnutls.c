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
#include "plugin.h"
#include "request.h"
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
	gnutls_global_init();

	gnutls_certificate_allocate_credentials(&xcred);
	/*gnutls_certificate_set_x509_trust_file(xcred, "ca.pem",
	  GNUTLS_X509_FMT_PEM);*/
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

/** Callback from the dialog in ssl_gnutls_authcheck_ask */
static void ssl_gnutls_authcheck_cb(PurpleSslConnection * gsc, gint choice)
{
  if (NULL == gsc)
    {
      purple_debug_error("gnutls","Inappropriate NULL argument at %s:%d\n",
			 __FILE__, (int) __LINE__);
      return;
    }

  switch(choice)
    {
    case 1: /* "Accept" */
      /* TODO: Shoud PURPLE_INPUT_READ be hardcoded? */
      gsc->connect_cb(gsc->connect_cb_data, gsc, PURPLE_INPUT_READ);
      break;

    default: /* "Cancel" or otherwise...? */
      purple_debug_info("gnutls",
			"User rejected certificate from %s\n",
			gsc->host);
      if(gsc->error_cb != NULL)
	gsc->error_cb(gsc, PURPLE_SSL_PEER_AUTH_FAILED,
		      gsc->connect_cb_data);
      purple_ssl_close(gsc);
    }
}

/** Pop up a dialog asking for verification of the given certificate */
static void ssl_gnutls_authcheck_ask(PurpleSslConnection * gsc)
{
  PurpleSslGnutlsData *gnutls_data = PURPLE_SSL_GNUTLS_DATA(gsc);

  const gnutls_datum_t *cert_list;
  unsigned int cert_list_size = 0;
  gnutls_session_t session=gnutls_data->session;
  
  cert_list =
    gnutls_certificate_get_peers(session, &cert_list_size);

  if (0 == cert_list_size || NULL == cert_list)
    {
      /* Peer provided no certificates at all.
	 TODO: We should write a witty message here.
      */
      gchar * primary = g_strdup_printf
	(
	 _("Peer %s provided no certificates.\n Connect anyway?"),
	 gsc->host
	 );

      purple_request_accept_cancel
	(gsc,
	 _("SSL Authorization Request"),
	 primary,
	 _("The server you are connecting to presented no certificates identifying itself. You have no assurance that you are not connecting to an imposter. Connect anyway?"),
	 2, /* Default action is "Cancel" */
	 NULL, NULL, /* There is no way to extract account data from
			a connection handle, it seems. */
	 NULL,       /* Same goes for the conversation data */
	 gsc,        /* Pass connection ptr to callback */
	 ssl_gnutls_authcheck_cb, /* Accept */
	 ssl_gnutls_authcheck_cb  /* Cancel */
	 );
      g_free(primary);
    }
  else
    {
      /* Grab the first certificate and display some data about it */
      guchar fpr_bin[256];     /* Raw binary key fingerprint */
      gsize fpr_bin_sz = sizeof(fpr_bin); /* Size of above (used later) */
      gchar * fpr_asc = NULL; /* ASCII representation of key fingerprint */
      guchar ser_bin[256];     /* Certificate Serial Number field */
      gsize ser_bin_sz = sizeof(ser_bin);
      gchar * ser_asc = NULL;
      gchar dn[1024];          /* Certificate Name field */
      gsize dn_sz = sizeof(dn);
      /* TODO: Analyze certificate time/date stuff */
      gboolean CERT_OK = TRUE; /* Is the certificate "good"? */

      gnutls_x509_crt_t cert; /* Certificate data itself */

      /* Suck the certificate data into the structure */
      gnutls_x509_crt_init(&cert);
      gnutls_x509_crt_import (cert, &cert_list[0],
			      GNUTLS_X509_FMT_DER);

      /* Read key fingerprint */
      gnutls_x509_crt_get_fingerprint(cert, GNUTLS_MAC_SHA,
				      fpr_bin, &fpr_bin_sz);
      fpr_asc = purple_base16_encode_chunked(fpr_bin,fpr_bin_sz);

      /* Read serial number */
      gnutls_x509_crt_get_serial(cert, ser_bin, &ser_bin_sz);
      ser_asc = purple_base16_encode_chunked(ser_bin,ser_bin_sz);

      /* Read the certificate DN field */
      gnutls_x509_crt_get_dn(cert, dn, &dn_sz);

      /* TODO: Certificate checking here */


      /* Build the dialog */
      {
	gchar * primary = NULL;
	gchar * secondary = NULL;

	if ( CERT_OK == TRUE )
	  {
	    primary = g_strdup_printf
	      (
	       _("Certificate from %s is valid. Accept?"),
	       gsc->host
	       );
	  }
	else
	  {
	    primary = g_strdup_printf
	      (
	       _("Certificate from %s not valid! Accept anyway?"),
	       gsc->host
	       );
	  }

	secondary = g_strdup_printf
	  (
	   _("Certificate name:\n%s\n\nKey fingerprint (SHA1): %s\n\nSerial Number: %s\n\nTODO: Expiration dates, etc.\n"),
	   dn, fpr_asc, ser_asc
	   );

	purple_request_accept_cancel
	  (gsc,
	   _("SSL Authorization Request"),
	   primary,
	   secondary,
	   (CERT_OK == TRUE ? 1:2), /* Default action depends on certificate
				       status. */
	   NULL, NULL, /* There is no way to extract account data from
			  a connection handle, it seems. */
	   NULL,       /* Same goes for the conversation data */
	   gsc,        /* Pass connection ptr to callback */
	   ssl_gnutls_authcheck_cb, /* Accept */
	   ssl_gnutls_authcheck_cb  /* Cancel */
	 );

	g_free(primary);
	g_free(secondary);
      } /* End dialog construction */


      /* Cleanup! */
      g_free(fpr_asc);
      g_free(ser_asc);

      gnutls_x509_crt_deinit(cert);
    } /* if (0 == ... */

  purple_debug_info("gnutls","Requested user verification for certificate from %s\n", gsc->host);
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

		/* Spit some key info to debug */
		{
		  const gnutls_datum_t *cert_list;
		  unsigned int cert_list_size = 0;
		  gnutls_session_t session=gnutls_data->session;
		  int i;
		  
		  cert_list =
		    gnutls_certificate_get_peers(session, &cert_list_size);
		  
		  purple_debug_info("gnutls",
				    "Peer %s provided %d certs\n",
				    gsc->host,
				    cert_list_size);

		  for (i=0; i<cert_list_size; i++)
		    {
		      guchar fpr_bin[256];
		      gsize fpr_bin_sz = sizeof(fpr_bin);
		      gchar * fpr_asc = NULL;
		      guchar tbuf[256];
		      gsize tsz=sizeof(tbuf);
		      gchar * tasc = NULL;
		      gnutls_x509_crt_t cert;
		      int ret;
		      
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
					"Serial: %s(%d bytes, ret=%d)\n",
					tasc, tsz, ret);
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

		      tsz=sizeof(tbuf);
		      gnutls_x509_crt_get_key_id(cert,0, tbuf, &tsz);
		      tasc = purple_base16_encode_chunked(tbuf, tsz);
		      purple_debug_info("gnutls",
					"Key ID: %s\n",
					tasc);
		      g_free(tasc);

		      g_free(fpr_asc); fpr_asc = NULL;
		      gnutls_x509_crt_deinit(cert);
		    } /* for */
		  
		} /* End keydata spitting */

		/* Ask for cert verification */
		ssl_gnutls_authcheck_ask(gsc);
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
