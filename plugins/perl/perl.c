/*
 * gaim
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
#ifdef HAVE_CONFIG_H
#include <config.h>
# ifdef HAVE_LIMITS_H
#  include <limits.h>
#  ifndef NAME_MAX
#   define NAME_MAX _POSIX_NAME_MAX
#  endif
# endif
#endif

#ifdef DEBUG
# undef DEBUG
#endif

#undef PACKAGE

#define group perl_group

#ifdef _WIN32
/* This took me an age to figure out.. without this __declspec(dllimport)
 * will be ignored.
 */
# define HASATTRIBUTE
#endif

#include <EXTERN.h>

#ifndef _SEM_SEMUN_UNDEFINED
# define HAS_UNION_SEMUN
#endif

#include <perl.h>
#include <XSUB.h>

#ifndef _WIN32
# include <sys/mman.h>
#endif

#undef PACKAGE

#ifndef _WIN32
# include <dirent.h>
#else
 /* We're using perl's win32 port of this */
# define dirent direct
#endif

#undef group

/* perl module support */
#ifdef OLD_PERL
extern void boot_DynaLoader _((CV * cv));
#else
extern void boot_DynaLoader _((pTHX_ CV * cv)); /* perl is so wacky */
#endif

#undef _
#ifdef DEBUG
# undef DEBUG
#endif
#ifdef _WIN32
# undef pipe
#endif

#ifdef _WIN32
#define _WIN32DEP_H_
#endif
#include "internal.h"
#include "debug.h"
#include "plugin.h"
#include "signals.h"
#include "version.h"

#include "perl-common.h"
#include "perl-handlers.h"

#define PERL_PLUGIN_ID "core-perl"

typedef struct
{
	GaimPlugin *plugin;
	char *package;
	char *load_sub;
	char *unload_sub;
} GaimPerlScript;


PerlInterpreter *my_perl = NULL;

static void
#ifdef OLD_PERL
xs_init()
#else
xs_init(pTHX)
#endif
{
	char *file = __FILE__;

	/* This one allows dynamic loading of perl modules in perl
           scripts by the 'use perlmod;' construction*/
	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}


static void
perl_init(void)
{
	/* changed the name of the variable from load_file to
	   perl_definitions since now it does much more than defining
	   the load_file sub. Moreover, deplaced the initialisation to
	   the xs_init function. (TheHobbit)*/
	char *perl_args[] = { "", "-e", "0", "-w" };
	char perl_definitions[] =
	{
		/* We use to function one to load a file the other to
		   execute the string obtained from the first and holding
		   the file contents. This allows to have a really local $/
		   without introducing temp variables to hold the old
		   value. Just a question of style:) */
		"package Gaim::PerlLoader;"
		"use Symbol;"

		"sub load_file {"
		  "my $f_name=shift;"
		  "local $/=undef;"
		  "open FH,$f_name or return \"__FAILED__\";"
		  "$_=<FH>;"
		  "close FH;"
		  "return $_;"
		"}"

		"sub destroy_package {"
		  "eval { $_[0]->UNLOAD() if $_[0]->can('UNLOAD'); };"
		  "Symbol::delete_package($_[0]);"
		"}"

		"sub load_n_eval {"
		  "my ($f_name, $package) = @_;"
		  "destroy_package($package);"
		  "my $strin=load_file($f_name);"
		  "return 2 if($strin eq \"__FAILED__\");"
		  "my $eval = qq{package $package; $strin;};"

		  "{"
		  "  eval $eval;"
		  "}"

		  "if($@) {"
		    /*"  #something went wrong\n"*/
		    "die(\"Errors loading file $f_name: $@\");"
		  "}"

		  "return 0;"
		"}"
	};

	my_perl = perl_alloc();
        PERL_SET_CONTEXT(my_perl);
	PL_perl_destruct_level = 1;
	perl_construct(my_perl);
#ifdef DEBUG
	perl_parse(my_perl, xs_init, 4, perl_args, NULL);
#else
	perl_parse(my_perl, xs_init, 3, perl_args, NULL);
#endif
#ifdef HAVE_PERL_EVAL_PV
	eval_pv(perl_definitions, TRUE);
#else
	perl_eval_pv(perl_definitions, TRUE); /* deprecated */
#endif
	perl_run(my_perl);
}

static void
perl_end(void)
{
	if (my_perl == NULL)
		return;
	
	PL_perl_destruct_level = 1;
        PERL_SET_CONTEXT(my_perl);
	perl_eval_pv(
		"foreach my $lib (@DynaLoader::dl_modules) {"
		  "if ($lib =~ /^Gaim\\b/) {"
		    "$lib .= '::deinit();';"
		    "eval $lib;"
		  "}"
		"}",
		TRUE);

	PL_perl_destruct_level = 1;
        PERL_SET_CONTEXT(my_perl);
	perl_destruct(my_perl);
	perl_free(my_perl);
	my_perl = NULL;
}

void
gaim_perl_callXS(void (*subaddr)(pTHX_ CV *cv), CV *cv, SV **mark)
{
	dSP;

	PUSHMARK(mark);
	(*subaddr)(aTHX_ cv);

	PUTBACK;
}

static gboolean
probe_perl_plugin(GaimPlugin *plugin)
{
	/* XXX This would be much faster if I didn't create a new
	 *     PerlInterpreter every time I probed a plugin */
	
	PerlInterpreter *prober = perl_alloc();
	char *argv[] = {"", plugin->path };
	gboolean status = TRUE;
	HV *plugin_info;
        PERL_SET_CONTEXT(prober);
	PL_perl_destruct_level = 1;
	perl_construct(prober);

	perl_parse(prober, xs_init, 2, argv, NULL);

	perl_run(prober);

	plugin_info = perl_get_hv("PLUGIN_INFO", FALSE);

	if (plugin_info == NULL)
		status = FALSE;
	else if (!hv_exists(plugin_info, "perl_api_version",
						strlen("perl_api_version")) ||
			 !hv_exists(plugin_info, "name", strlen("name")) ||
			 !hv_exists(plugin_info, "load", strlen("load")))
	{
		/* Not a valid plugin. */

		status = FALSE;
	}
	else
	{
		SV **key;
		int perl_api_ver;

		key = hv_fetch(plugin_info, "perl_api_version",
					   strlen("perl_api_version"), 0);

		perl_api_ver = SvIV(*key);

		if (perl_api_ver != 2)
			status = FALSE;
		else
		{
			GaimPluginInfo *info;
			GaimPerlScript *gps;
			char *basename;
			STRLEN len;

			gaim_debug(GAIM_DEBUG_INFO, "perl", "Found plugin info\n");

			info = g_new0(GaimPluginInfo, 1);
			gps  = g_new0(GaimPerlScript, 1);

			info->magic = GAIM_PLUGIN_MAGIC;
			info->major_version = GAIM_MAJOR_VERSION;
			info->minor_version = GAIM_MINOR_VERSION;
			info->type = GAIM_PLUGIN_STANDARD;

			info->dependencies = g_list_append(info->dependencies,
											   PERL_PLUGIN_ID);

			gps->plugin = plugin;

			basename = g_path_get_basename(plugin->path);
			gaim_perl_normalize_script_name(basename);
			gps->package = g_strdup_printf("Gaim::Script::%s", basename);
			g_free(basename);

			/* We know this one exists. */
			key = hv_fetch(plugin_info, "name", strlen("name"), 0);
			info->name = g_strdup(SvPV(*key, len));

			if ((key = hv_fetch(plugin_info, "GTK_UI", strlen("GTK_UI"), 0)))
				info->ui_requirement = GAIM_GTK_PLUGIN_TYPE;

			if ((key = hv_fetch(plugin_info, "url", strlen("url"), 0)))
				info->homepage = g_strdup(SvPV(*key, len));
			
			if ((key = hv_fetch(plugin_info, "author", strlen("author"), 0)))
				info->author = g_strdup(SvPV(*key, len));

			if ((key = hv_fetch(plugin_info, "summary",
								strlen("summary"), 0)))
				info->summary = g_strdup(SvPV(*key, len));

			if ((key = hv_fetch(plugin_info, "description",
								strlen("description"), 0)))
				info->description = g_strdup(SvPV(*key, len));

			if ((key = hv_fetch(plugin_info, "version", strlen("version"), 0)))
				info->version = g_strdup(SvPV(*key, len));

			if ((key = hv_fetch(plugin_info, "load", strlen("load"), 0)))
				gps->load_sub = g_strdup_printf("%s::%s", gps->package,
												SvPV(*key, len));

			if ((key = hv_fetch(plugin_info, "unload", strlen("unload"), 0)))
				gps->unload_sub = g_strdup_printf("%s::%s", gps->package,
												  SvPV(*key, len));

			/********************************************************/
			/* Only one of the next two options should be present 	*/
			/*							*/
			/* prefs_info - Uses non-GUI (read GTK) gaim API calls	*/
			/*		and creates a GaimPluginPrefInfo type.	*/
			/*							*/
			/* gtk_prefs_info - Requires gtk2-perl be installed by	*/
			/*		the user and he must create a GtkWidget */
			/* 		representing the plugin preferences	*/
			/*		page.					*/
			/********************************************************/
			if ((key = hv_fetch(plugin_info, "prefs_info", strlen("prefs_info"), 0))) {
				/* key now is the name of the Perl sub that will create a frame for us */
				info->prefs_info = gaim_perl_plugin_pref(g_strdup_printf("%s::%s", gps->package, SvPV(*key, len)));
			}
			
			if ((key = hv_fetch(plugin_info, "gtk_prefs_info", strlen("gtk_prefs_info"), 0))) {
				/* key now is the name of the Perl sub that will create a frame for us */
				info->ui_info = gaim_perl_gtk_plugin_pref(g_strdup_printf("%s::%s", gps->package, SvPV(*key, len)));
			}

			/********************************************************/
			/* 							*/
			/* plugin_action - This is given to the plugin info	*/		
			/*	as the action GList.  There are two parts 	*/
			/*	so the user can set the title as it will appear	*/
			/*	in the plugin action menu.  The name is 	*/
			/*	extracted and then the callback perl sub's name	*/
			/* 	both of which then are handled by an internal	*/
			/*	gaim_perl function that sets up the single cb	*/
			/*	function which is then inserted into 'info'.	*/
			/********************************************************/
			if ((key = hv_fetch(plugin_info, "plugin_action_label", strlen("plugin_action_label"), 0))) {
				gaim_perl_plugin_action_label = g_strdup(SvPV(*key, len));
			}

			if ((key = hv_fetch(plugin_info, "plugin_action", strlen("plugin_action"), 0))) {
				gaim_perl_plugin_action_callback_sub = g_strdup_printf("%s::%s", gps->package, SvPV(*key, len));
				info->actions = gaim_perl_plugin_action;
			}

			plugin->info = info;
			info->extra_info = gps;

			status = gaim_plugin_register(plugin);
		}
	}
	
	PL_perl_destruct_level = 1;
        PERL_SET_CONTEXT(prober);
	perl_destruct(prober);
	perl_free(prober);
	return status;
}

static gboolean
load_perl_plugin(GaimPlugin *plugin)
{
	GaimPerlScript *gps = (GaimPerlScript *)plugin->info->extra_info;
	char *atmp[3] = { plugin->path, NULL, NULL };

	if (gps == NULL || gps->load_sub == NULL)
		return FALSE;

	gaim_debug(GAIM_DEBUG_INFO, "perl", "Loading perl script\n");

	if (my_perl == NULL)
		perl_init();

	plugin->handle = gps;

	atmp[1] = gps->package;
	
        PERL_SET_CONTEXT(my_perl);
	execute_perl("Gaim::PerlLoader::load_n_eval", 2, atmp);

	{
		PERL_SET_CONTEXT(my_perl);
		dSP;
		ENTER;
		SAVETMPS;
		PUSHMARK(sp);
		XPUSHs(sv_2mortal(gaim_perl_bless_object(plugin, "Gaim::Plugin")));
		PUTBACK;

		perl_call_pv(gps->load_sub, G_EVAL | G_SCALAR);
		SPAGAIN;

		if (SvTRUE(ERRSV)) {
			STRLEN len;

			gaim_debug(GAIM_DEBUG_ERROR, "perl",
					   "Perl function %s exited abnormally: %s\n",
					   gps->load_sub, SvPV(ERRSV, len));
		}

		PUTBACK;
		FREETMPS;
		LEAVE;
	}

	return TRUE;
}

static void
destroy_package(const char *package)
{
        PERL_SET_CONTEXT(my_perl);
	dSP;

	ENTER;
	SAVETMPS;

	PUSHMARK(SP);
	XPUSHs(sv_2mortal(newSVpv(package, strlen(package))));
	PUTBACK;

	perl_call_pv("Gaim::PerlLoader::destroy_package",
				 G_VOID | G_EVAL | G_DISCARD);

	SPAGAIN;

	PUTBACK;
	FREETMPS;
	LEAVE;
}

static gboolean
unload_perl_plugin(GaimPlugin *plugin)
{
	GaimPerlScript *gps = (GaimPerlScript *)plugin->info->extra_info;

	if (gps == NULL)
		return FALSE;

	gaim_debug(GAIM_DEBUG_INFO, "perl", "Unloading perl script\n");

	if (gps->unload_sub != NULL)
	{
        	PERL_SET_CONTEXT(my_perl);
		dSP;
		ENTER;
		SAVETMPS;
		PUSHMARK(sp);
		XPUSHs(sv_2mortal(gaim_perl_bless_object(plugin, "Gaim::Plugin")));
		PUTBACK;

		perl_call_pv(gps->unload_sub, G_EVAL | G_SCALAR);
		SPAGAIN;

		if (SvTRUE(ERRSV)) {
			STRLEN len;

			gaim_debug(GAIM_DEBUG_ERROR, "perl",
					   "Perl function %s exited abnormally: %s\n",
					   gps->load_sub, SvPV(ERRSV, len));
		}

		PUTBACK;
		FREETMPS;
		LEAVE;
	}

	gaim_perl_signal_clear_for_plugin(plugin);
	gaim_perl_timeout_clear_for_plugin(plugin);

	destroy_package(gps->package);

	return TRUE;
}

static void
destroy_perl_plugin(GaimPlugin *plugin)
{
	if (plugin->info != NULL)
	{
		GaimPerlScript *gps;

		if (plugin->info->name != NULL)
			g_free(plugin->info->name);

		if (plugin->info->version != NULL)
			g_free(plugin->info->version);

		if (plugin->info->summary != NULL)
			g_free(plugin->info->summary);

		if (plugin->info->description != NULL)
			g_free(plugin->info->description);

		if (plugin->info->author != NULL)
			g_free(plugin->info->author);

		if (plugin->info->homepage != NULL)
			g_free(plugin->info->homepage);

		gps = (GaimPerlScript *)plugin->info->extra_info;

		if (gps != NULL)
		{
			if (gps->load_sub != NULL)
				g_free(gps->load_sub);

			if (gps->unload_sub != NULL)
				g_free(gps->unload_sub);

			if (gps->package != NULL)
				g_free(gps->package);

			g_free(gps);
			plugin->info->extra_info = NULL;
		}
	}
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	perl_end();

	return TRUE;
}

static GaimPluginLoaderInfo loader_info =
{
	NULL,                                             /**< exts           */
	probe_perl_plugin,                                /**< probe          */
	load_perl_plugin,                                 /**< load           */
	unload_perl_plugin,                               /**< unload         */
	destroy_perl_plugin                               /**< destroy        */
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_LOADER,                               /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	PERL_PLUGIN_ID,                                   /**< id             */
	N_("Perl Plugin Loader"),                         /**< name           */
	VERSION,                                          /**< version        */
	N_("Provides support for loading perl plugins."), /**< summary        */
	N_("Provides support for loading perl plugins."), /**< description    */
	"Christian Hammond <chipx86@gnupdate.org>",       /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&loader_info,                                     /**< extra_info     */
	NULL,
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
	loader_info.exts = g_list_append(loader_info.exts, "pl");
}

GAIM_INIT_PLUGIN(perl, init_plugin, info)
