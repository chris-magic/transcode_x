/*
 * output_handle.c
 *
 *  Created on: Sep 24, 2012
 *      Author: chris
 */
#include <stdio.h>
#include <stdlib.h>

#include "libavutil/samplefmt.h"


#include "output_handle.h"
#include "input_handle.h"

#include "chris_error.h"
#include "chris_global.h"
//参考了output-example.c
AVStream * add_video_stream (AVFormatContext *fmt_ctx ,enum CodecID codec_id){

	AVCodecContext *avctx;
	AVStream *st;

	//add video stream
	st = avformat_new_stream(fmt_ctx ,NULL);
	if(st == NULL){
		printf("in out file ,new video stream failed ..\n");
		exit(NEW_VIDEO_STREAM_FAIL);
	}

	//set the index of the stream
	st->id = 0;

	//set AVCodecContext of the stream
	avctx = st->codec;

	avctx->codec_id = codec_id;
	avctx->codec_type = AVMEDIA_TYPE_VIDEO;

	avctx->bit_rate = 600 * 1000;
	avctx->width = 640;
	avctx->height = 360;

	avctx->pix_fmt = PIX_FMT_YUV420P;
	avctx->me_range = 16;
	avctx->qcompress = 0.6;
	avctx->qmin = 10;
	avctx->qmax = 51;
	avctx->max_qdiff = 4;
	avctx->crf = 22;

	avctx->rc_lookahead = 60;
	avctx->gop_size = 25;
	avctx->time_base.den = 25;
	avctx->time_base.num = 1;


//	avctx->me_cmp = 2;
//	avctx->me_sub_cmp = 2;
//	    avctx->mb_decision = 2;

//
//	avctx->max_b_frames = 20;
//	avctx->keyint_min = 2;

	avctx->profile = FF_PROFILE_H264_MAIN;
	// some formats want stream headers to be separate(for example ,asfenc.c ,but not mpegts)
	if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		avctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

	return st;
}



static AVStream * add_audio_stream (AVFormatContext *fmt_ctx ,enum CodecID codec_id){
	AVCodecContext *avctx;
	AVStream *st;

	//add video stream
	st = avformat_new_stream(fmt_ctx ,NULL);
	if(st == NULL){
		printf("in out file ,new video stream failed ..\n");
		exit(NEW_VIDEO_STREAM_FAIL);
	}

	//set the index of the stream
	st->id = 1;

	//set AVCodecContext of the stream
	avctx = st->codec;
	avctx->codec_id = codec_id;
	avctx->codec_type = AVMEDIA_TYPE_AUDIO;

	avctx->sample_fmt = AV_SAMPLE_FMT_S16;
	avctx->bit_rate = 64000;
	avctx->sample_rate = 44100;
	avctx->channels = 2;

	// some formats want stream headers to be separate(for example ,asfenc.c ,but not mpegts)
	if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		avctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

	return st;
}

int init_output(OUTPUT_CONTEXT *ptr_output_ctx, char* output_file){

	//set AVOutputFormat
    /* allocate the output media context */
	printf("output_file = %s \n" ,output_file);
    avformat_alloc_output_context2(&ptr_output_ctx->ptr_format_ctx, NULL, NULL, output_file);
    if (ptr_output_ctx->ptr_format_ctx == NULL) {
        printf("Could not deduce[推断] output format from file extension: using MPEG.\n");
        avformat_alloc_output_context2(&ptr_output_ctx->ptr_format_ctx, NULL, "mpeg", output_file);
        if(ptr_output_ctx->ptr_format_ctx == NULL){
        	 printf("Could not find suitable output format\n");
        	 exit(NOT_GUESS_OUT_FORMAT);
        }
    }
    //in here ,if I get AVOutputFormat succeed ,the filed audio_codec and video_codec will be set default.
    ptr_output_ctx->fmt = ptr_output_ctx->ptr_format_ctx->oformat;


    /* add audio stream and video stream 	*/
    ptr_output_ctx->video_stream = NULL;
    ptr_output_ctx->audio_stream = NULL;

    ptr_output_ctx->audio_codec_id = CODEC_ID_AAC; //aac
    ptr_output_ctx->video_codec_id = CODEC_ID_H264; //h264

    if (ptr_output_ctx->fmt->video_codec != CODEC_ID_NONE) {

    	ptr_output_ctx->video_stream = add_video_stream(ptr_output_ctx->ptr_format_ctx, ptr_output_ctx->video_codec_id);
    	if(ptr_output_ctx->video_stream == NULL){
    		printf("in output ,add video stream failed \n");
    		exit(ADD_VIDEO_STREAM_FAIL);
    	}
    }

    if (ptr_output_ctx->fmt->audio_codec != CODEC_ID_NONE) {

    	ptr_output_ctx->audio_stream = add_audio_stream(ptr_output_ctx->ptr_format_ctx, ptr_output_ctx->audio_codec_id);
    	if(ptr_output_ctx->audio_stream == NULL){
    		printf(".in output ,add audio stream failed \n");
    		exit(ADD_AUDIO_STREAM_FAIL);
    	}
    }


    /*	malloc buffer	*/
    ptr_output_ctx->encoded_yuv_pict = avcodec_alloc_frame();
    if(ptr_output_ctx->encoded_yuv_pict == NULL){
		printf("yuv_frame allocate failed %s ,%d line\n" ,__FILE__ ,__LINE__);
		exit(MEMORY_MALLOC_FAIL);
	}
    int size = avpicture_get_size(ptr_output_ctx->video_stream->codec->pix_fmt ,
    		ptr_output_ctx->video_stream->codec->width ,
    		ptr_output_ctx->video_stream->codec->height);

    ptr_output_ctx->pict_buf = av_malloc(size);
    if(ptr_output_ctx->pict_buf == NULL){
    	printf("pict allocate failed ...\n");
    	exit(MEMORY_MALLOC_FAIL);
    }
    //bind
    avpicture_fill((AVPicture *)ptr_output_ctx->encoded_yuv_pict ,ptr_output_ctx->pict_buf ,
    				ptr_output_ctx->video_stream->codec->pix_fmt ,
    		    		ptr_output_ctx->video_stream->codec->width ,
    		    		ptr_output_ctx->video_stream->codec->height);


    /*output the file information */
    av_dump_format(ptr_output_ctx->ptr_format_ctx, 0, output_file, 1);

}


//===========================================================
static void open_video (OUTPUT_CONTEXT *ptr_output_ctx ,AVStream * st){

	AVCodec *video_encode;
	AVCodecContext *video_codec_ctx;

	video_codec_ctx = st->codec;

	//find video encode
	video_encode = avcodec_find_encoder(video_codec_ctx->codec_id);
	if(video_encode == NULL){
		printf("in output ,open_video ,can not find video encode.\n");
		exit(NO_FIND_VIDEO_ENCODE);
	}

	//open video encode
	if(avcodec_open2(video_codec_ctx ,video_encode ,NULL) < 0){

		printf("in open_video function ,can not open video encode.\n");
		exit(OPEN_VIDEO_ENCODE_FAIL);
	}

	//set video encoded buffer
	ptr_output_ctx->video_outbuf = NULL;
	if (!(ptr_output_ctx->ptr_format_ctx->oformat->flags & AVFMT_RAWPICTURE)) {//in ffmpeg,only nullenc and yuv4mpeg have this flags
																		//so ,mp4 and mpegts both go in here
		printf(".....malloc video buffer ...\n");
		ptr_output_ctx->video_outbuf_size = VIDEO_OUT_BUF_SIZE;
		ptr_output_ctx->video_outbuf = av_malloc(ptr_output_ctx->video_outbuf_size);
		if(ptr_output_ctx->video_outbuf == NULL){
			printf("video_outbuf malloc failed ...\n");
			exit(MEMORY_MALLOC_FAIL);
		}
	}


}

static void open_audio (OUTPUT_CONTEXT *ptr_output_ctx ,AVStream * st){

	AVCodec *audio_encode;
	AVCodecContext *audio_codec_ctx;

	audio_codec_ctx = st->codec;

	//find audio encode
	audio_encode = avcodec_find_encoder(audio_codec_ctx->codec_id);
	if(audio_encode == NULL){
		printf("in output ,open_audio ,can not find audio encode.\n");
		exit(NO_FIND_AUDIO_ENCODE);
	}

//    //add acc experimental
//    audio_codec_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL; //if not set ,the follow codec can not perform

	//open audio encode
	if(avcodec_open2(audio_codec_ctx ,audio_encode ,NULL) < 0){

		printf("in open_audio function ,can not open audio encode.\n");
		exit(OPEN_AUDIO_ENCODE_FAIL);
	}

	ptr_output_ctx->audio_outbuf_size = AUDIO_OUT_BUF_SIZE;
	ptr_output_ctx->audio_outbuf = av_malloc(ptr_output_ctx->audio_outbuf_size);
	if (ptr_output_ctx->audio_outbuf == NULL) {
		printf("audio_outbuf malloc failed ...\n");
		exit(MEMORY_MALLOC_FAIL);
	}

    /* ugly hack for PCM codecs (will be removed ASAP with new PCM
       support to compute the input frame size in samples */
	int audio_input_frame_size;
    if (audio_codec_ctx->frame_size <= 1) {
    	audio_input_frame_size = ptr_output_ctx->audio_outbuf_size / audio_codec_ctx->channels;
    	printf("&&$$&&#&#&#&&#&#&#&&#\n\n");
    	sleep(10);
        switch(st->codec->codec_id) {
        case CODEC_ID_PCM_S16LE:
        case CODEC_ID_PCM_S16BE:
        case CODEC_ID_PCM_U16LE:
        case CODEC_ID_PCM_U16BE:
            audio_input_frame_size >>= 1;
            break;
        default:
            break;
        }
    } else {
        audio_input_frame_size = audio_codec_ctx->frame_size;
    }
    ptr_output_ctx->samples = av_malloc(audio_input_frame_size * 2 * audio_codec_ctx->channels);

}


void open_stream_codec(OUTPUT_CONTEXT *ptr_output_ctx){

	open_video (ptr_output_ctx ,ptr_output_ctx->video_stream);

	open_audio (ptr_output_ctx ,ptr_output_ctx->audio_stream);

}

void encode_video_frame(OUTPUT_CONTEXT *ptr_output_ctx ,AVFrame *pict ,INPUT_CONTEXT *ptr_input_ctx){

	static int frame_count = 0;

	//encode the image
	int video_encoded_out_size;
//	pict->pts = av_rescale_q(frame_count ,AV_TIME_BASE * (int64_t)ptr_output_ctx->video_stream->codec->time_base.num
//						,ptr_output_ctx->video_stream->codec->time_base.den);
	pict->pts = frame_count ++;

	printf("video ...start encode ....\n");
	video_encoded_out_size = avcodec_encode_video(ptr_output_ctx->video_stream->codec
			,ptr_output_ctx->video_outbuf ,ptr_output_ctx->video_outbuf_size
			,pict);

	if (video_encoded_out_size < 0) {
		fprintf(stderr, "Error encoding video frame\n");
		exit(VIDEO_ENCODE_ERROR);
	}

	if(video_encoded_out_size == 0) return ;
	//in here ,video_encodec_out_size > 0
	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.stream_index = ptr_output_ctx->video_stream->index;
	pkt.data = ptr_output_ctx->video_outbuf; // packet data will be allocated by the encoder
	pkt.size = video_encoded_out_size;

	if (ptr_output_ctx->video_stream->codec->coded_frame->pts != AV_NOPTS_VALUE)
		pkt.pts = av_rescale_q(ptr_output_ctx->video_stream->codec->coded_frame->pts, ptr_output_ctx->video_stream->codec->time_base,
				ptr_output_ctx->video_stream->time_base);

	if(ptr_output_ctx->video_stream->codec->coded_frame->key_frame)
		pkt.flags |= AV_PKT_FLAG_KEY;


//	printf("pkt.pts + %lld \n" ,pkt.pts);
//	if (c->coded_frame->pts != AV_NOPTS_VALUE)
//		pkt.pts = av_rescale_q(c->coded_frame->pts, c->time_base,
//				st->time_base);
//	if (c->coded_frame->key_frame)
//		pkt.flags |= AV_PKT_FLAG_KEY;

//	int i = av_interleaved_write_frame(ptr_output_ctx->ptr_format_ctx, &pkt);  //write no data
	int i = av_write_frame(ptr_output_ctx->ptr_format_ctx, &pkt);

	av_free_packet(&pkt);
}


void encode_audio_frame(OUTPUT_CONTEXT *ptr_output_ctx , AVFrame * audio_frame ,int buf_size){

	int ret;
	AVCodecContext *c = ptr_output_ctx->audio_stream->codec;
	//packet for output
	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;
	//frame for input
	AVFrame *frame = avcodec_alloc_frame();
	if (frame == NULL) {
		printf("frame malloc failed ...\n");
		exit(1);
	}

	printf("frame_size = %d \n" ,c->frame_size);
	frame->nb_samples = buf_size /
					(c->channels * av_get_bytes_per_sample(c->sample_fmt));

//	printf(".....%d.\n" ,av_get_bytes_per_sample(c->sample_fmt) );
	if ((ret = avcodec_fill_audio_frame(frame, c->channels, AV_SAMPLE_FMT_S16,
			audio_frame->data[0], buf_size, 1)) < 0) {
		av_log(NULL, AV_LOG_FATAL, "Audio encoding failed\n");
		exit(AUDIO_ENCODE_ERROR);
	}

	int got_packet = 0;
	if (avcodec_encode_audio2(c, &pkt, frame, &got_packet) < 0) {
		av_log(NULL, AV_LOG_FATAL, "Audio encoding failed\n");
		exit(AUDIO_ENCODE_ERROR);
	}
	pkt.pts = 0;
	pkt.stream_index = ptr_output_ctx->audio_stream->index;

	int i = av_write_frame(ptr_output_ctx->ptr_format_ctx, &pkt);
//	printf("write frame .... ,i = %d\n", i);

	av_free(frame);
	av_free_packet(&pkt);
}

