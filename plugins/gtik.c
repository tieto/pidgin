/*
 *	GNOME Stock Ticker 
 *	(C) 2000 Jayson Lorenzen, Jim Garrison, Rached Blili
 *
 *	based on:
 *	desire, and the old great slash applet.
 *	
 *
 *	Authors: Jayson Lorenzen (jaysonl@pacbell.net)
 *               Jim Garrison (garrison@users.sourceforge.net)
 *               Rached Blili (striker@Dread.net)
 *
 *	The Gnome Stock Ticker is a free, Internet based application. 
 *      These quotes are not guaranteed to be timely or accurate.
 *
 *	Do not use the Gnome Stock Ticker for making investment decisions, 
 *      it is for informational purposes only.
 *
 *      Modified by EWarmenhoven to become a gaim plugin. There was little
 *      enough that needed to be changed that I can't really claim any credit.
 *      (you need to add -lghttp to the GTK_LIBS part of the Makefile)
 *      TODO: config, saving info
 *
 */

#define GAIM_PLUGINS
#include "gaim.h"

#include <gtk/gtk.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ghttp.h"
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <gdk/gdkx.h>


	GtkWidget *applet; /* this will become the main window */
	GtkWidget *label;


	static GdkPixmap *pixmap = NULL;
	GtkWidget * drawing_area;

	int location;
	int MOVE;

	char output[64];

	/**
	 * FOR COLOR
	 * LEN and the length of output, and colorNum must match
	 */
	const int LEN = 20;
	char outputArray[20][64];
	char changeArray[20][64];
	int colorArray[20];

	const int RED = 1;
	const int GREEN = 2;

	static const int max_rgb_str_len = 7;
	static const int max_rgb_str_size = 8;

	int setCounter, getCounter, setColorCounter,getColorCounter;

	GdkGC *gc;
	GdkColor gdkUcolor,gdkDcolor;

	/* end of COLOR vars */


	char configFileName[256];


	/*  properties vars */
 	 
	GtkWidget *tik_syms_entry;
	gchar tik_syms[256];

	GtkWidget * pb = NULL;

	typedef struct
	{
		char *tik_syms;
		char *output;
		char *scroll;
		gint timeout;
		gchar dcolor[8];
		gchar ucolor[8];

	} gtik_properties;

	gtik_properties props = {"cald+rhat+corl","default","right2left",
					5,"#ff0000","#00ff00"};

	/* end prop vars */


	gint timeout = 0;
	gint drawTimeID, updateTimeID;
	GdkFont * my_font;
	GdkFont * extra_font;
	GdkFont * small_font;
	static gint symbolfont = 1;
	static gint destroycb;


	int configured();
	void timeout_cb( GtkWidget *widget, GtkWidget *spin );
	static int http_get_to_file(gchar *a_host, gint a_port, 
				gchar *a_resource, FILE *a_file);
	int http_got();
	void properties_save( char *path) ;
	void gaim_plugin_remove();


	/* FOR COLOR */

	void updateOutput() ;
	static void reSetOutputArray() ;
	static void setOutputArray(char *param1) ;
	static void setColorArray(int theColor) ;
	void setup_colors(void);
	int create_gc(void) ;

	/* end of color funcs */



	/*-----------------------------------------------------------------*/
	void remove_self(GtkWidget *w, void *handle)
	{
		gtk_signal_disconnect(GTK_OBJECT(applet), destroycb);
		if (drawTimeID > 0) { g_source_remove(drawTimeID); }
		if (updateTimeID >0) { g_source_remove(updateTimeID); }
		gtk_widget_destroy(applet);
		gaim_plugin_unload(handle);
	}


	/*-----------------------------------------------------------------*/
	void load_fonts()
	{
		my_font = gdk_font_load ("fixed");
		extra_font = gdk_font_load ("-urw-symbol-medium-r-normal-*-*-100-*-*-p-*-adobe-fontspecific");
		small_font = gdk_font_load ("-schumacher-clean-medium-r-normal-*-*-100-*-*-c-*-iso8859-1");

		/* If fonts do not load */
		if (!my_font)
			g_error("Could not load fonts!");
		if (!extra_font) {
			extra_font = gdk_font_load("fixed");
			symbolfont = 0;
		}
		if (!small_font)
			small_font = gdk_font_load("fixed");
	}

	/*-----------------------------------------------------------------*/
	/*void load_properties( char *path) {


		gnome_config_push_prefix (path);
		if( gnome_config_get_string ("gtik/tik_syms") != NULL ) 
		   props.tik_syms = gnome_config_get_string("gtik/tik_syms");
		

		timeout = gnome_config_get_int("gtik/timeout") > 0 ? gnome_config_get_int ("gtik/timeout") : props.timeout;
	

		if ( gnome_config_get_string ("gtik/output") != NULL ) 
			props.output = gnome_config_get_string("gtik/output");
		
		if ( gnome_config_get_string ("gtik/scroll") != NULL ) 
			props.scroll = gnome_config_get_string("gtik/scroll");
		
		if ( gnome_config_get_string ("gtik/ucolor") != NULL ) 
		   strcpy(props.ucolor, gnome_config_get_string("gtik/ucolor"));
		
		if ( gnome_config_get_string ("gtik/dcolor") != NULL ) 
		   strcpy(props.dcolor, gnome_config_get_string("gtik/dcolor"));
		
		gnome_config_pop_prefix ();
	}*/



	/*-----------------------------------------------------------------*/
	/*void properties_save( char *path) {

		gnome_config_push_prefix (path);
		gnome_config_set_string( "gtik/tik_syms", props.tik_syms );
		gnome_config_set_string( "gtik/output", props.output );
		gnome_config_set_string( "gtik/scroll", props.scroll );
		gnome_config_set_string( "gtik/ucolor", props.ucolor );
		gnome_config_set_string( "gtik/dcolor", props.dcolor );

		gnome_config_set_int("gtik/timeout",props.timeout);

		gnome_config_pop_prefix ();
		gnome_config_sync();
		gnome_config_drop_all();
	}*/	


	/*-----------------------------------------------------------------*/
	char * extractText(const char *line) {

		int  i=0;
		int  j=0;
		static char Text[256]="";

		while (i < (strlen(line) -1)) {
			if (line[i] != '>')
				i++;
			else {
				i++;
				while (line[i] != '<') {
					Text[j] = line[i];
					i++;j++;
				}
			}
		}
		Text[j] = '\0';
		i = 0;
		while (i < (strlen(Text)) ) {
			if (Text[i] < 32)
				Text[i] = '\0';
			i++;
		}
		return(Text);
				
	}

	/*-----------------------------------------------------------------*/
	char * parseQuote(FILE *CONFIG, char line[512]) {
		
		char symbol[512];
		char buff[512];
		char price[16];
		char change[16];
		char percent[16];
		static char result[512]="";
		int  linenum=0;
		int AllOneLine=0;
		int flag=0;
		char *section;
		char *ptr;

		if (strlen(line) > 64) AllOneLine=1;

		if (AllOneLine) {
			strcpy(buff,line);
			while (!flag) {
				if ((ptr=strstr(buff,"</td>"))!=NULL) {
					ptr[0] = '|';
				}	
				else flag=1;
			}
			section = strtok(buff,"|");
		}
		/* Get the stock symbol */
		if (!AllOneLine) strcpy(symbol,extractText(line));
		else strcpy(symbol,extractText(section));
		linenum++;

		/* Skip the time... */
		if (!AllOneLine) fgets(line,255,CONFIG);
		else section = strtok(NULL,"|");
		linenum++;

		while (linenum < 8 ) {
			if (!AllOneLine) {
				fgets(line,255,CONFIG);
			
				if (strstr(line,
			  	 "<td align=center nowrap colspan=2>")) {
					strcpy(change,"");
					strcpy(percent,"");
					linenum=100;
				}
			}
			else {
				section = strtok(NULL,"|");
				if (strstr(section,
			  	 "<td align=center nowrap colspan=2>")) {
					strcpy(change,"");
					strcpy(percent,"");
					linenum=100;
				}
			}

			if (linenum == 2) { 
				if (!AllOneLine) 
					strcpy(price,extractText(line));
				else
					strcpy(price,extractText(section));
			}
			else if (linenum == 3) {
				if (!AllOneLine)
					strcpy(change,extractText(line));
				else
					strcpy(change,extractText(section));

				if (strstr(change,"-")) {
					setColorArray(RED);
				}
				else if (strstr(change,"+")) {
					setColorArray(GREEN);
				}
				else {
					setColorArray(0);
				}
				
			}
			else if (linenum == 4) {
				if (!AllOneLine) 
					strcpy(percent,extractText(line));
				else
					strcpy(percent,extractText(section));
			}
			linenum++;
		}
		sprintf(result,"%s:%s:%s:%s",
				symbol,price,change,percent);			
		return(result);

	}



	/*-----------------------------------------------------------------*/
	int configured() {
		int retVar;

		char  buffer[512];
		static FILE *CONFIG;

		CONFIG = fopen((const char *)configFileName,"r");

		retVar = 0;

		/* clear the output variable */
		reSetOutputArray();

		if ( CONFIG ) {
			while ( !feof(CONFIG) ) {
				fgets(buffer,511,CONFIG);

				if (strstr(buffer,
				    "<td nowrap align=left><a href=\"/q\?s=")) {

				      setOutputArray(parseQuote(CONFIG,buffer));
				      retVar = 1;
				}
				else {
				      retVar = (retVar > 0) ? retVar : 0;
				}
			}
			fclose(CONFIG);

		}
		else {
			retVar = 0;
		}

		return retVar;
	}


	/*-----------------------------------------------------------------*/
	/* Shamelessly stolen from the Slashapp applet
	 */
	static int http_get_to_file(gchar *a_host, gint a_port, 
				    gchar *a_resource, FILE *a_file) {
		int length = -1;
		ghttp_request *request = NULL;
		gchar s_port[8];
		gchar *uri = NULL;
		gchar *body;
		gchar *proxy = g_getenv("http_proxy");

		g_snprintf(s_port, 8, "%d", a_port);
		uri = g_strconcat("http://", a_host, ":", s_port, 
						a_resource, NULL);

		fprintf(stderr,"Asking for %s\n", uri);

		request = ghttp_request_new();
		if (!request)
			goto ec;
		if (proxy && (ghttp_set_proxy(request,proxy) != 0))
			goto ec;

		if (ghttp_set_uri(request, uri) != 0)
			goto ec;
		ghttp_set_header(request, http_hdr_Connection, "close");
		if (ghttp_prepare(request) != 0)
			goto ec;
		if (ghttp_process(request) != ghttp_done)
			goto ec;
		length = ghttp_get_body_len(request);
		body = ghttp_get_body(request);
		if (body != NULL)
			fwrite(body, length, 1, a_file);

		ec:
			if (request)
				ghttp_request_destroy(request);
			if (uri)
				
				g_free(uri);
		return length;
	}




	/*-----------------------------------------------------------------*/
	int http_got() {

		int retVar;
		FILE *local_file;

		char tmpBuff[256];
		memset(tmpBuff,0,sizeof(tmpBuff));

		strcat(tmpBuff,"/q?s=");
		strcat(tmpBuff, props.tik_syms);
		strcat(tmpBuff,"&d=v2");

		retVar = 0;

		local_file = fopen(configFileName, "w");
		retVar = http_get_to_file("finance.yahoo.com", 80, 
						tmpBuff, local_file);

		fclose(local_file);

		return retVar;
	}





	/*-----------------------------------------------------------------*/
	gint expose_event (GtkWidget *widget,GdkEventExpose *event) {

		gdk_draw_pixmap(widget->window,
		widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
		pixmap,
		event->area.x, event->area.y,
		event->area.x, event->area.y,
		event->area.width,event->area.height);

		return FALSE;
	}



	/*-----------------------------------------------------------------*/
	static gint configure_event(GtkWidget *widget,GdkEventConfigure *event){

		if(pixmap) {
			gdk_pixmap_unref (pixmap);
		}

		pixmap = gdk_pixmap_new(widget->window,
		widget->allocation.width,
		widget->allocation.height,
		-1);

		return TRUE;
	}






	/*-----------------------------------------------------------------*/
	gint Repaint (gpointer data) {
		GtkWidget* drawing_area = (GtkWidget *) data;
		GdkRectangle update_rect;
		int	comp;

		/* FOR COLOR */
		char *tmpSym;
		int totalLoc;
		int totalLen;
		int i;


		totalLoc = 0;
		totalLen = 0;

		/* clear the pixmap */
		gdk_draw_rectangle (pixmap,
		drawing_area->style->black_gc,
		TRUE,
		0,0,
		drawing_area->allocation.width,
		drawing_area->allocation.height);


		for(i=0;i<LEN;i++) {
			totalLen += strlen(outputArray[i]);
		}
		if (!strcmp(props.output,"default")) {
			for(i=0;i<LEN;i++) {
				totalLen += strlen(changeArray[i]);
			}
		}

		comp = 1 - ( totalLen *8 );

		if (MOVE == 1) { MOVE = 0; } else { MOVE = 1; }

		if (MOVE == 1) {


		  if (!strcmp(props.scroll,"right2left")) {
			if (location > comp) {
				location--;
			}
			else {
				location = drawing_area->allocation.width;
			}

		  }
		  else {
                       if (location < drawing_area->allocation.width) {
                                location ++;
                        }
                        else {
                                location = comp;
                        }
		  }



		}

		for (i=0;i<LEN;i++) {

			/* COLOR */
			if (colorArray[i] == GREEN) {
				gdk_gc_set_foreground( gc, &gdkUcolor );
			}
			else if (colorArray[i] == RED) {
				gdk_gc_set_foreground( gc, &gdkDcolor );
			}
			else {
				gdk_gc_copy( gc, drawing_area->style->white_gc );
			}

			tmpSym = outputArray[i];
			gdk_draw_string (pixmap,my_font,
			gc,
			location + (totalLoc * 6 ) ,12,outputArray[i]);
			totalLoc += (strlen(tmpSym) + 1); 


			if (!strcmp(props.output,"default")) {
				tmpSym = changeArray[i];
				if (*(changeArray[i]))
					gdk_draw_text (pixmap,extra_font,
					     gc, location + (totalLoc * 6) ,
					     12,changeArray[i],1);
				gdk_draw_string (pixmap,small_font,
				     gc, location + ((totalLoc +2) * 6) ,
				     12, &changeArray[i][1]);
				totalLoc += (strlen(tmpSym) + 1); 
			}
			
		}
		update_rect.x = 0;
		update_rect.y = 0;
		update_rect.width = drawing_area->allocation.width;
		update_rect.height = drawing_area->allocation.height;

		gtk_widget_draw(drawing_area,&update_rect);
		return 1;
	}




	/*-----------------------------------------------------------------*/
	
        struct gaim_plugin_description desc;  
        struct gaim_plugin_description *gaim_plugin_desc() {
		desc.api_version = PLUGIN_API_VERSION;
		desc.name = g_strdup("Stock Ticker");
		desc.version = g_strdup(VERSION);
		desc.description = g_strdup(
					    " This program uses ghttp to connect to "
					    "a popular stock quote site, then downloads "
					    "and parses the html returned from the "
					    "site to scroll delayed quotes"
					    "\n\n The Gnome Stock Ticker is a free, Internet based application. These quotes are not "
					    "guaranteed to be timely or accurate. "
					    "Do not use the Gnome Stock Ticker for making investment decisions; it is for "
					    "informational purposes only.");
		desc.authors = g_strdup("Jayson Lorenzen, Jim Garrison, Rached Blili");
		desc.url = g_strdup(WEBSITE);
		return &desc;
	}

        char *description() {
		return
		" This program uses ghttp to connect to "
		"a popular stock quote site, then downloads "
		"and parses the html returned from the "
		"site to scroll delayed quotes"
		"\n\n The Gnome Stock Ticker is a free, Internet based application. These quotes are not "
		"guaranteed to be timely or accurate. "
		"Do not use the Gnome Stock Ticker for making investment decisions; it is for "
		"informational purposes only."
		"\n\n (C) 2000 Jayson Lorenzen, Jim Garrison, Rached Blili";
	}

	char *name() {
		return "The Gnome Stock Ticker for Gaim";
	}






	/*-----------------------------------------------------------------*/
	void changed_cb(GtkWidget *pb, gpointer data) {
		/* gnome_property_box_changed(GNOME_PROPERTY_BOX(pb)); */
	}


	/*-----------------------------------------------------------------*/
	void toggle_output_cb(GtkWidget *widget, gpointer data) {
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
			props.output = g_strdup("nochange");
		else
			props.output = g_strdup("default");
			
	}

	/*-----------------------------------------------------------------*/
	void toggle_scroll_cb(GtkWidget *widget, gpointer data) {
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
			props.scroll = g_strdup("left2right");
		else
			props.scroll = g_strdup("right2left");
			
	}

	/*-----------------------------------------------------------------*/
	void timeout_cb( GtkWidget *widget, GtkWidget *spin ) {
		timeout=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));
		/* gnome_property_box_changed(GNOME_PROPERTY_BOX(pb)); */
	}




	/*-----------------------------------------------------------------*/
	static void apply_cb( GtkWidget *widget, void *data ) {
		char *tmpText;


		tmpText = gtk_entry_get_text(GTK_ENTRY(tik_syms_entry));
		props.tik_syms = g_strdup(tmpText);
		if  (props.timeout) {	
			props.timeout = timeout > 0 ? timeout : props.timeout;
		}

		/* properties_save(APPLET_WIDGET(applet)->privcfgpath); */

		setup_colors();		
		updateOutput();
	}




	/*-----------------------------------------------------------------*/
	gint destroy_cb( GtkWidget *widget, void *data ) {
		pb = NULL;
		return FALSE;
	}



	/*-----------------------------------------------------------------*/
	void gaim_plugin_config() {
		GtkWidget * vbox;
		GtkWidget *urlcheck, *launchcheck;

		GtkWidget *panela, *panel1 ,*panel2, *panel3, *panel4;
		GtkWidget *label1,*label2,*label3 ;


		GtkWidget *timeout_label,*timeout_c;
		GtkObject *timeout_a;

		GtkWidget *upColor, *downColor, *upLabel, *downLabel;
		GtkWidget *check,*check2;

		int ur,ug,ub, dr,dg,db; 

		if (pb) return;
		pb = gtk_window_new(GTK_WINDOW_TOPLEVEL);

		gtk_window_set_title(GTK_WINDOW(pb), _("Gnome Stock Ticker Properties"));

		vbox = gtk_vbox_new(FALSE, 8);

		panela = gtk_hbox_new(FALSE, 5);
		panel1 = gtk_hbox_new(FALSE, 5);
		panel2 = gtk_hbox_new(FALSE, 5);
		panel3 = gtk_hbox_new(FALSE, 5);
		panel4 = gtk_hbox_new(FALSE, 5);

		gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

		timeout_label = gtk_label_new(_("Update Frequency in min"));
		timeout_a = gtk_adjustment_new( timeout, 0.5, 128, 1, 8, 8 );
		timeout_c  = gtk_spin_button_new( GTK_ADJUSTMENT(timeout_a), 1, 0 );

		gtk_box_pack_start_defaults( GTK_BOX(panel2), timeout_label );
		gtk_box_pack_start_defaults( GTK_BOX(panel2), timeout_c );
	
		g_signal_connect_swapped(GTK_OBJECT(timeout_c), "changed",G_CALLBACK(changed_cb),GTK_OBJECT(pb));

		g_signal_connect( GTK_OBJECT(timeout_a),"value_changed",
		G_CALLBACK(timeout_cb), timeout_c );
		g_signal_connect( GTK_OBJECT(timeout_c),"changed",
		G_CALLBACK(timeout_cb), timeout_c );
		gtk_spin_button_set_update_policy( GTK_SPIN_BUTTON(timeout_c),
		GTK_UPDATE_ALWAYS );

		label1 = gtk_label_new(_("Enter symbols delimited with \"+\" in the box below."));

		tik_syms_entry = gtk_entry_new_with_max_length(60);

		/* tik_syms var is her if want a default value */
		gtk_entry_set_text(GTK_ENTRY(tik_syms_entry), props.tik_syms ? props.tik_syms : tik_syms);
		g_signal_connect_swapped(GTK_OBJECT(tik_syms_entry), "changed",G_CALLBACK(changed_cb),GTK_OBJECT(pb));

		/* OUTPUT FORMAT and SCROLL DIRECTION */

		label2 = gtk_label_new(_("Check this box to display only symbols and price:"));
		label3 = gtk_label_new(_("Check this box to scroll left to right:"));
		check = gtk_check_button_new();
		check2 = gtk_check_button_new();
		gtk_box_pack_start_defaults(GTK_BOX(panel3),label2);
		gtk_box_pack_start_defaults(GTK_BOX(panel3),check);
		gtk_box_pack_start_defaults(GTK_BOX(panel4),label3);
		gtk_box_pack_start_defaults(GTK_BOX(panel4),check2);

		/* Set the checkbox according to current prefs */
		if (strcmp(props.output,"default")!=0)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
							TRUE);
		g_signal_connect_swapped(GTK_OBJECT(check),"toggled",
				G_CALLBACK(changed_cb),GTK_OBJECT(pb));
		g_signal_connect(GTK_OBJECT(check),"toggled",
				G_CALLBACK(toggle_output_cb),NULL);

		/* Set the checkbox according to current prefs */
		if (strcmp(props.scroll,"right2left")!=0)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check2),
							TRUE);
		g_signal_connect_swapped(GTK_OBJECT(check2),"toggled",
				G_CALLBACK(changed_cb),GTK_OBJECT(pb));
		g_signal_connect(GTK_OBJECT(check2),"toggled",
				G_CALLBACK(toggle_scroll_cb),NULL);

		/* COLOR */
/*		upLabel = gtk_label_new(_("+ Color"));
		upColor = gnome_color_picker_new();
	
		sscanf( props.ucolor, "#%02x%02x%02x", &ur,&ug,&ub );
	
		gnome_color_picker_set_i8(GNOME_COLOR_PICKER (upColor), 
					  ur, ug, ub, 255);
		
		g_signal_connect(GTK_OBJECT(upColor), "color_set",
				G_CALLBACK(ucolor_set_cb), NULL);

		gtk_box_pack_start_defaults( GTK_BOX(panela), upLabel );
		gtk_box_pack_start_defaults( GTK_BOX(panela), upColor );

		downLabel = gtk_label_new(_("- Color"));
		downColor = gnome_color_picker_new();

		sscanf( props.dcolor, "#%02x%02x%02x", &dr,&dg,&db );

		gnome_color_picker_set_i8(GNOME_COLOR_PICKER (downColor), 
					  dr, dg, db, 255);

		g_signal_connect(GTK_OBJECT(downColor), "color_set",
				G_CALLBACK(dcolor_set_cb), NULL);

		gtk_box_pack_start_defaults( GTK_BOX(panela), downLabel );
		gtk_box_pack_start_defaults( GTK_BOX(panela), downColor );

*/
		gtk_box_pack_start(GTK_BOX(panel1), label1, FALSE, 
				   FALSE, 8);

		gtk_box_pack_start(GTK_BOX(vbox), panel2, FALSE,
				    FALSE, 8);
		gtk_box_pack_start(GTK_BOX(vbox), panel3, FALSE,
				    FALSE, 8);
		gtk_box_pack_start(GTK_BOX(vbox), panel4, FALSE,
				    FALSE, 8);
		gtk_box_pack_start(GTK_BOX(vbox), panela, FALSE,
				    FALSE, 8);
		gtk_box_pack_start(GTK_BOX(vbox), panel1, FALSE,
				    FALSE, 8);

		gtk_box_pack_start(GTK_BOX(vbox), tik_syms_entry,
				    FALSE, FALSE, 8);

		gtk_container_add(GTK_CONTAINER(pb), vbox);

		g_signal_connect_swapped(GTK_OBJECT(tik_syms_entry), 
				   "changed",G_CALLBACK(changed_cb),
				   GTK_OBJECT(pb));

		g_signal_connect(GTK_OBJECT(pb), "apply",
				    G_CALLBACK(apply_cb), NULL);

		gtk_widget_show_all(pb);
	}





	/*-----------------------------------------------------------------*/
	char *gaim_plugin_init(GModule *handle) { /* used to be main() */
		GtkWidget *label;

		GtkWidget * vbox;

		memset(configFileName,0,sizeof(configFileName));
		strcat(configFileName, getenv("HOME"));
		strcat(configFileName, "/.gtik.conf");

		applet = gtk_window_new(GTK_WINDOW_TOPLEVEL); /* or not */

		vbox = gtk_hbox_new (FALSE,0);

		drawing_area = gtk_drawing_area_new();
		gtk_drawing_area_size(GTK_DRAWING_AREA (drawing_area),200,20);

		gtk_widget_show(drawing_area);
		gtk_box_pack_start(GTK_BOX (vbox), drawing_area,TRUE,TRUE,0);

		gtk_widget_show(vbox);

		/* applet_widget_add (APPLET_WIDGET (applet), vbox); */
		gtk_container_add(GTK_CONTAINER(applet), vbox);

		g_signal_connect(GTK_OBJECT(drawing_area),"expose_event",
		(GtkSignalFunc) expose_event, NULL);

		g_signal_connect(GTK_OBJECT(drawing_area),"configure_event",
		(GtkSignalFunc) configure_event, NULL);


		destroycb = g_signal_connect(GTK_OBJECT(applet), "destroy",
			G_CALLBACK(remove_self), handle);



		gtk_widget_show (applet);
		create_gc();

		/* load_properties(APPLET_WIDGET(applet)->privcfgpath); */

		setup_colors();
		load_fonts();		
		updateOutput();


		/* KEEPING TIMER ID FOR CLEANUP IN DESTROY */
		drawTimeID   = g_timeout_add(2,Repaint,drawing_area);
		updateTimeID = g_timeout_add(props.timeout * 60000,
				   updateOutput,"NULL");


		return NULL;
	}



	/*-----------------------------------------------------------------*/
	void updateOutput() {
		if ( http_got() == -1 || !(configured()) ) {  
			reSetOutputArray();
			printf("No data!\n");
			setOutputArray("No data available or properties not set");
		}
	}




/* JHACK */
	/*-----------------------------------------------------------------*/
	void gaim_plugin_remove() {
		gtk_signal_disconnect(GTK_OBJECT(applet), destroycb);
		if (drawTimeID > 0) { g_source_remove(drawTimeID); }
		if (updateTimeID >0) { g_source_remove(updateTimeID); }
		gtk_widget_destroy(applet);
	}




	
/*HERE*/
	/*-----------------------------------------------------------------*/
	static void reSetOutputArray() {
		int i;
		
		for (i=0;i<LEN;i++) {
			/* CLEAR EACH SYMBOL'S SPACE */
			memset(outputArray[i],0,sizeof(outputArray[i]));

			/* CLEAR ASSOC COLOR ARRAY */
			colorArray[i] = 0;

			/* CLEAR ADDITIONAL INFO */
			memset(changeArray[i],0,sizeof(changeArray[i]));

		}

		setCounter = 0;
		getCounter = 0;
		setColorCounter = 0;
		getColorCounter = 0;
		
	}	


	/*-----------------------------------------------------------------*/
	char *splitPrice(char *data) {
		char buff[128]="";
		static char buff2[128]="";
		char *var1, *var2;
		int i;

		strcpy(buff,data);
		var1 = strtok(buff,":");
		var2 = strtok(NULL,":");

		sprintf(buff2,"  %s %s",var1,var2);
		return(buff2);
	}

	/*-----------------------------------------------------------------*/
	char *splitChange(char *data) {
		char buff[128]="";
		static char buff2[128]="";
		char *var1, *var2, *var3, *var4;
		int i;

		strcpy(buff,data);
		var1 = strtok(buff,":");
		var2 = strtok(NULL,":");
		var3 = strtok(NULL,":");
		var4 = strtok(NULL,"");

		if (var3[0] == '+') { 
			if (symbolfont)
				var3[0] = 221;
			var4[0] = '(';
		}
		else if (var3[0] == '-') {
			if (symbolfont)
				var3[0] = 223;	
			var4[0] = '(';
		}
		else {
			var3 = strdup(_("(No"));
			var4 = strdup(_("Change"));
		}

		sprintf(buff2,"%s %s)",var3,var4);
		return(buff2);
	}

	/*-----------------------------------------------------------------*/
	static void setOutputArray(char *param1) {
		char *price;
		char *change;

		price = splitPrice(param1);
		change = splitChange(param1);

		if (setCounter < LEN) {
			
			strcpy(outputArray[setCounter],price);
			strcpy(changeArray[setCounter],change);
		}
		setCounter++;
	}



	/*-----------------------------------------------------------------*/
	static void setColorArray(int theColor) {
		if (setColorCounter < LEN) {
			colorArray[setColorCounter] = theColor;
		}
		setColorCounter++;
	}

	void setup_colors(void) {
		GdkColormap *colormap;

		colormap = gtk_widget_get_colormap(drawing_area);

		gdk_color_parse(props.ucolor, &gdkUcolor);
		gdk_color_alloc(colormap, &gdkUcolor);

		gdk_color_parse(props.dcolor, &gdkDcolor);
		gdk_color_alloc(colormap, &gdkDcolor);
	}

	
	int create_gc(void) {
		gc = gdk_gc_new( drawing_area->window );
		gdk_gc_copy( gc, drawing_area->style->white_gc );
		return 0;
	}
