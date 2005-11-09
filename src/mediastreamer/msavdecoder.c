/*
  The mediastreamer library aims at providing modular media processing and I/O
	for linphone, but also for any telephony application.
  Copyright (C) 2001  Simon MORLAT simon.morlat@linphone.org
  										
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "msavdecoder.h"
#include "mscodec.h"
extern MSFilter *ms_mpeg_encoder_new();
extern MSFilter *ms_mpeg4_encoder_new();
extern MSFilter *ms_h263_encoder_new();


MSCodecInfo MPEGinfo={
	{
		"MPEG1 codec",
		0,
		MS_FILTER_VIDEO_CODEC,
		ms_mpeg_encoder_new,
		"This is a MPEG1 codec taken from the ffmpeg project."
	},
	ms_mpeg_encoder_new,
	ms_mpeg_decoder_new,
	0,
	0,
	0,	/*bitrate */
	0,	/*sample freq */
	0,
	"MPV",
	1,
	1
};

MSCodecInfo h263info={
	{
		"H263 codec",
		0,
		MS_FILTER_VIDEO_CODEC,
		ms_h263_encoder_new,
		"This is a H263 codec taken from the ffmpeg project."
	},
	ms_h263_encoder_new,
	ms_h263_decoder_new,
	0,
	0,
	0,	/*bitrate */
	0,	/*sample freq */
	0,
	"H263",
	1,
	1
};

MSCodecInfo MPEG4info={
	{
		"MPEG4 codec",
		0,
		MS_FILTER_VIDEO_CODEC,
		ms_mpeg4_encoder_new,
		"This is a MPEG4 codec taken from the ffmpeg project."
	},
	ms_mpeg4_encoder_new,
	ms_mpeg4_decoder_new,
	0,
	0,
	0,	/*bitrate */
	0,	/*sample freq */
	0,
	"MP4V-ES",
	1,
	1
};


void ms_AVCodec_init()
{
	avcodec_init();
	avcodec_register_all();
	ms_filter_register((MSFilterInfo*)&h263info);
	//ms_filter_register((MSFilterInfo*)&MPEG4info);
}


static MSAVDecoderClass *ms_avdecoder_class=NULL;

MSFilter *ms_h263decoder_new()
{
	return ms_AVdecoder_new_with_codec(CODEC_ID_H263);
}

MSFilter *ms_mpeg_decoder_new()
{
	return ms_AVdecoder_new_with_codec(CODEC_ID_MPEG1VIDEO);
}

MSFilter *ms_mpeg4_decoder_new()
{
	return ms_AVdecoder_new_with_codec(CODEC_ID_MPEG4);
}

MSFilter *ms_h263_decoder_new(){
	return ms_AVdecoder_new_with_codec(CODEC_ID_H263);
}

MSFilter * ms_AVdecoder_new_with_codec(enum CodecID codec_id)
{
	MSAVDecoder *enc;

	enc=g_malloc0(sizeof(MSAVDecoder));
	if (ms_avdecoder_class==NULL)
	{
		 ms_avdecoder_class=g_malloc0(sizeof(MSAVDecoderClass));
		 ms_AVdecoder_class_init(ms_avdecoder_class);
	}
	MS_FILTER(enc)->klass=(MSFilterClass*)ms_avdecoder_class;
	ms_AVdecoder_init(enc,avcodec_find_decoder(codec_id));
	return MS_FILTER(enc);
}


void ms_AVdecoder_init(MSAVDecoder *dec, AVCodec *codec)
{
	gint error;
	
	ms_filter_init(MS_FILTER(dec));
	MS_FILTER(dec)->inqueues=dec->q_inputs;
	MS_FILTER(dec)->outqueues=dec->q_outputs;
	avcodec_get_context_defaults(&dec->av_context);
	dec->av_codec=codec;
	dec->av_opened=0;
	dec->skip_gob=1;
	dec->obufwrap=NULL;
}

void ms_AVdecoder_class_init(MSAVDecoderClass *klass)
{
	ms_filter_class_init(MS_FILTER_CLASS(klass));
	ms_filter_class_set_name( MS_FILTER_CLASS(klass),"AVdecoder");
	MS_FILTER_CLASS(klass)->max_qinputs=MSAVDECODER_MAX_INPUTS;
	MS_FILTER_CLASS(klass)->max_qoutputs=MSAVDECODER_MAX_INPUTS;
	MS_FILTER_CLASS(klass)->r_maxgran=0;
	MS_FILTER_CLASS(klass)->w_maxgran=0;
	MS_FILTER_CLASS(klass)->destroy=(MSFilterDestroyFunc)ms_AVdecoder_destroy;
	MS_FILTER_CLASS(klass)->process=(MSFilterProcessFunc)ms_AVdecoder_process;
}

void ms_AVdecoder_uninit(MSAVDecoder *dec)
{
	if (dec->obufwrap!=NULL) ms_buffer_destroy(dec->obufwrap);
	if (dec->av_opened) avcodec_close(&dec->av_context);
}
void ms_AVdecoder_destroy( MSAVDecoder *obj)
{
	ms_AVdecoder_uninit(obj);
	g_free(obj);
}

gint ms_AVdecoder_set_format(MSAVDecoder *dec, gchar *fmt)
{
	gint format;
	if (strcmp(fmt,"YUV420P")==0) format=PIX_FMT_YUV420P;
	else if (strcmp(fmt,"YUV422")==0) format=PIX_FMT_YUV422;
	else if (strcmp(fmt,"RGB24")==0) format=PIX_FMT_RGB24;	
	else if (strcmp(fmt,"BGR24")==0) format=PIX_FMT_BGR24;
	else if (strcmp(fmt,"YUV422P")==0) format=PIX_FMT_YUV422P;
	else if (strcmp(fmt,"YUV444P")==0) format=PIX_FMT_YUV444P;
	else {
		g_warning("ms_AVdecoder_set_format: unsupported format %s.",fmt);
		return -1;
	}
	dec->output_pix_fmt=format;
	return 0;
}

void ms_AVdecoder_process(MSAVDecoder *r)
{
	AVFrame orig;
	AVFrame transformed;
	MSQueue *inq,*outq;
	MSMessage *inm,*outm;
	gint error;
	gint got_picture;
	gint len;
	unsigned char *data;
	AVCodecContext *ctx=&r->av_context;
	gint gob_num;
	
	inq=r->q_inputs[0];
	outq=r->q_outputs[0];
	
	/* get a picture from the input queue */
	inm=ms_queue_get(inq);
	g_return_if_fail(inm!=NULL);
	if (inm->size > 0)
	{
		guint32 *p = inm->data;
		
		if (!r->av_opened){
			error=avcodec_open(&r->av_context, r->av_codec);
			if (error!=0) g_warning("avcodec_open() failed: %i",error);
			else r->av_opened=1;
		}
		
		gob_num = (ntohl(*p) >> 10) & 0x1f;
		ms_trace("gob %i, size %i", gob_num, inm->size);
		ms_trace("ms_AVdecoder_process: received %08x %08x", ntohl(p[0]), ntohl(p[1]));
		
		/* remove H.263 Payload Header */
		p[0] = htonl( ntohl(p[0]) & 0x0000ffff ); 

		if (gob_num == 0){
			if (r->skip_gob == 0)
			{
				unsigned char *data = r->buf_compressed;
				ms_trace("ms_AVdecoder_process: decoding %08x %08x %08x", ntohl(((unsigned int *)data)[0]), ntohl(((unsigned int *)data)[1]), ntohl(((unsigned int *)data)[2]));
				while (r->buf_size > 0) {
					len=avcodec_decode_video(&r->av_context,&orig,&got_picture,data,r->buf_size /*inm->size*/);
					if (len<0) {
						ms_warning("ms_AVdecoder_process: error %i.",len);
						break;
					}
					if (got_picture) {
						ms_trace("ms_AVdecoder_process: got_picture: width=%i height=%i fmt=%i",
								ctx->width,ctx->height,ctx->pix_fmt);
						/* set the image in the wanted format */
						outm=ms_message_alloc();
						if (r->obufwrap==NULL){
							r->obufwrap=ms_buffer_new(avpicture_get_size(r->output_pix_fmt,ctx->width,ctx->height));
							r->obufwrap->ref_count++;
						}
						ms_message_set_buf(outm,r->obufwrap);
						avpicture_fill(&transformed,outm->data,r->output_pix_fmt,ctx->width,ctx->height);
						img_convert(&transformed, r->output_pix_fmt,
								&orig,ctx->pix_fmt,ctx->width,ctx->height);
						ms_queue_put(outq,outm);
					}
					r->buf_size -= len;
					data += len;
				}
			}
			else {
				r->skip_gob = 0;
			}
			memcpy(r->buf_compressed, inm->data, inm->size);
			r->buf_size = inm->size;
		}
		else {
			memcpy(r->buf_compressed + r->buf_size, inm->data, inm->size);
			r->buf_size += inm->size;
		}
	}
    ms_message_destroy(inm);
}


void ms_AVdecoder_set_width(MSAVDecoder *av,gint w)
{
	av->av_context.width=av->width=w;
}

void ms_AVdecoder_set_height(MSAVDecoder *av,gint h)
{
	av->av_context.height=av->height=h;
}
