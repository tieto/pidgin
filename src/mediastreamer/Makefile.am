EXTRA_DIST= 

noinst_LTLIBRARIES = libmediastreamer.la 

useless_files=mstcpserv.c mstcpserv.h mstcpclient.c mstcpclient.h 

libmediastreamer_la_SOURCES=msfilter.c msfilter.h msutils.h waveheader.h\
							mscodec.c mscodec.h \
							mssoundread.c mssoundread.h \
							mssoundwrite.c mssoundwrite.h \
							msbuffer.c msbuffer.h  \
							msqueue.c msqueue.h \
							msfifo.c msfifo.h \
							ms.c ms.h\
							mssync.c mssync.h \
							msnosync.c msnosync.h \
							msread.c msread.h \
							mswrite.c mswrite.h \
							mscopy.c mscopy.h \
							msosswrite.c msosswrite.h  \
							msossread.c msossread.h \
							msringplayer.c msringplayer.h \
							msrtprecv.c msrtprecv.h   \
							msrtpsend.c msrtpsend.h	\
							msAlawenc.c msAlawenc.h g711common.h \
							msAlawdec.c msAlawdec.h g711common.h \
							msMUlawenc.c msMUlawenc.h g711common.h \
							msMUlawdec.c msMUlawdec.h g711common.h \
							mstimer.c mstimer.h \
							msqdispatcher.c msqdispatcher.h \
							msfdispatcher.c msfdispatcher.h \
							sndcard.c sndcard.h \
							osscard.c osscard.h\
							hpuxsndcard.c \
							alsacard.c alsacard.h \
							jackcard.c jackcard.h \
							audiostream.c mediastream.h \
							msspeexenc.c msspeexenc.h msspeexdec.c msspeexdec.h
#							msGSMdecoder.c msGSMdecoder.h 
#							msGSMencoder.c msGSMencoder.h
#							msLPC10encoder.c msLPC10encoder.h 
#							msLPC10decoder.c msLPC10decoder.h 

libmediastreamer_la_LDFLAGS = -export-dynamic

libmediastreamer_la_LIBADD= $(GLIB_LIBS) $(VV_LIBS)

AM_CFLAGS=$(GLIB_CFLAGS) -DG_LOG_DOMAIN=\"MediaStreamer\" $(VV_CFLAGS) $(IPV6_CFLAGS) -DHAVE_GLIB $(DEBUG_CFLAGS)


INCLUDES=  -I$(top_srcdir) -I$(top_srcdir)/mediastreamer $(VV_CFLAGS) 
