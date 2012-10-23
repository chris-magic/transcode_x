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
	st->id = ID_VIDEO_STREAM;

	//set AVCodecContext of the stream
	avctx = st->codec;

	avctx->codec_id = codec_id;
	avctx->codec_type = AVMEDIA_TYPE_VIDEO;

	avctx->bit_rate = VIDEO_BIT_RATE;
	avctx->width = VIDEO_WIDTH;
	avctx->height = VIDEO_HEIGHT;

	avctx->pix_fmt = PIX_FMT_YUV420P;
	avctx->me_range = 16;
	avctx->qcompress = 0.6;
	avctx->qmin = 10;
	avctx->qmax = 51;
	avctx->max_qdiff = 4;
	avctx->crf = 22;

	avctx->rc_lookahead = 60;
	avctx->gop_size = VIDEO_FRAME_RATE;

	avctx->time_base.den = VIDEO_FRAME_RATE;
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



static AVStream * add_audio_stream (AVFormatContext *fmt_ctx ,enum CodecID codec_id ,INPUT_CONTEXT *ptr_input_ctx){
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
	avctx->bit_rate = AUDIO_BIT_RATE;
	avctx->sample_rate = AUDIO_SAMPLE_RATE;//ptr_input_ctx->audio_codec_ctx->sample_rate/*44100*/;

	avctx->channels = 2;

	// some formats want stream headers to be separate(for example ,asfenc.c ,but not mpegts)
	if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		avctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

	return st;
}

int init_output(OUTPUT_CONTEXT *ptr_output_ctx, char* output_file ,INPUT_CONTEXT *ptr_input_ctx){

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

    	ptr_output_ctx->audio_stream = add_audio_stream(ptr_output_ctx->ptr_format_ctx, ptr_output_ctx->audio_codec_id ,ptr_input_ctx);
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


    /*	init some member value */
    ptr_output_ctx->audio_resample = 0;
    ptr_output_ctx->swr = NULL;

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

void encode_video_frame(OUTPUT_CONTEXT *ptr_output_ctx, AVFrame *pict,
		INPUT_CONTEXT *ptr_input_ctx) {

	static int frame_count = 0;
	int nb_frames;
	double sync_ipts;
	double duration = 0;

	/* compute the duration */
	duration =
			FFMAX(av_q2d(ptr_input_ctx->ptr_format_ctx->streams[ptr_input_ctx->video_index]->time_base),
							av_q2d(ptr_input_ctx->video_codec_ctx->time_base));

	if (ptr_input_ctx->ptr_format_ctx->streams[ptr_input_ctx->video_index]->avg_frame_rate.num)
		duration = FFMAX(duration, 1/av_q2d(ptr_input_ctx->ptr_format_ctx->streams[ptr_input_ctx->video_index]->avg_frame_rate));

	duration /= av_q2d(ptr_output_ctx->video_stream->codec->time_base);


	/*	compute the sync_ipts ,use for to determine duplicate or drop the encode pic*/
	sync_ipts = ptr_output_ctx->sync_ipts / av_q2d(ptr_output_ctx->video_stream->codec->time_base);


    /* by default, we output a single frame ,there is no different in fps of input file  and fps of output file*/
    nb_frames = 1;


    //compute the vdelta ,do not forget the duration
    double vdelta = sync_ipts - frame_count + duration;

	// FIXME set to 0.5 after we fix some dts/pts bugs like in avidec.c
	if (vdelta < -1.1)
		nb_frames = 0;
	else if (vdelta > 1.1)
		nb_frames = lrintf(vdelta);

	//set chris_count
	int tmp_count;
	for (tmp_count = 0; tmp_count < nb_frames; tmp_count++) {
		//encode the image
		int video_encoded_out_size;
		pict->pts = frame_count++;

		video_encoded_out_size = avcodec_encode_video(
				ptr_output_ctx->video_stream->codec,
				ptr_output_ctx->video_outbuf, ptr_output_ctx->video_outbuf_size,
				pict);

		if (video_encoded_out_size < 0) {
			fprintf(stderr, "Error encoding video frame\n");
			exit(VIDEO_ENCODE_ERROR);
		}

//		if (video_encoded_out_size == 0)  //the first several number  pict ,there will no data to output because of the AVCodecContext buffer
//			return;
		//in here ,video_encodec_out_size > 0
		if(video_encoded_out_size > 0){
			AVPacket pkt;
			av_init_packet(&pkt);
			pkt.stream_index = ptr_output_ctx->video_stream->index;
			pkt.data = ptr_output_ctx->video_outbuf; // packet data will be allocated by the encoder
			pkt.size = video_encoded_out_size;

			if (ptr_output_ctx->video_stream->codec->coded_frame->pts
					!= AV_NOPTS_VALUE)
				pkt.pts = av_rescale_q(
						ptr_output_ctx->video_stream->codec->coded_frame->pts,
						ptr_output_ctx->video_stream->codec->time_base,
						ptr_output_ctx->video_stream->time_base);

			if (ptr_output_ctx->video_stream->codec->coded_frame->key_frame)
				pkt.flags |= AV_PKT_FLAG_KEY;

			int i = av_write_frame(ptr_output_ctx->ptr_format_ctx, &pkt);

			av_free_packet(&pkt);
		}

	}

}


void encode_audio_frame(OUTPUT_CONTEXT *ptr_output_ctx , uint8_t *buf ,int buf_size){

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

	frame->nb_samples = buf_size /
					(c->channels * av_get_bytes_per_sample(c->sample_fmt));

	if ((ret = avcodec_fill_audio_frame(frame, c->channels, AV_SAMPLE_FMT_S16,
				buf, buf_size, 1)) < 0) {
		av_log(NULL, AV_LOG_FATAL, ".Audio encoding failed\n");
		exit(AUDIO_ENCODE_ERROR);
	}

	int got_packet = 0;
	if (avcodec_encode_audio2(c, &pkt, frame, &got_packet) < 0) {
		av_log(NULL, AV_LOG_FATAL, "..Audio encoding failed\n");
		exit(AUDIO_ENCODE_ERROR);
	}
	pkt.pts = 0;
	pkt.stream_index = ptr_output_ctx->audio_stream->index;

	int i = av_write_frame(ptr_output_ctx->ptr_format_ctx, &pkt);

	av_free(frame);
	av_free_packet(&pkt);
}



/*	add silence audio data	*/
static void generate_silence(uint8_t* buf, enum AVSampleFormat sample_fmt, size_t size)
{
    int fill_char = 0x00;
    if (sample_fmt == AV_SAMPLE_FMT_U8)
        fill_char = 0x80;
    memset(buf, fill_char, size);
}

void encode_flush(OUTPUT_CONTEXT *ptr_output_ctx , int nb_ostreams){

	int i ;

	for (i = 0; i < nb_ostreams; i++){

		AVStream *st = ptr_output_ctx->ptr_format_ctx->streams[i];
		AVCodecContext *enc = st->codec;
		int stop_encoding = 0;

		for (;;){
			AVPacket pkt;
			int fifo_bytes;
			av_init_packet(&pkt);
			pkt.data = NULL;
			pkt.size = 0;

			switch (st->codec->codec_type) {
			/*audio stream*/
			case AVMEDIA_TYPE_AUDIO:
			{


				fifo_bytes = av_fifo_size(ptr_output_ctx->fifo);
				if (fifo_bytes > 0) {
					/* encode any samples remaining in fifo */
					int frame_bytes = fifo_bytes;
					av_fifo_generic_read(ptr_output_ctx->fifo, ptr_output_ctx->audio_buf, fifo_bytes, NULL);

					/* pad last frame with silence if needed */
					frame_bytes = enc->frame_size * enc->channels *
								  av_get_bytes_per_sample(enc->sample_fmt);
					if (ptr_output_ctx->allocated_audio_buf_size < frame_bytes)
						exit(1);
					generate_silence(ptr_output_ctx->audio_buf+fifo_bytes, enc->sample_fmt, frame_bytes - fifo_bytes);

					printf("audio ...........\n");
					encode_audio_frame(ptr_output_ctx, ptr_output_ctx->audio_buf, frame_bytes);

				} else {
					/* flush encoder with NULL frames until it is done
					   returning packets */
					int got_packet = 0;
					int ret1;
					ret1 = avcodec_encode_audio2(enc, &pkt, NULL, &got_packet);
					if ( ret1 < 0) {
						av_log(NULL, AV_LOG_FATAL, "..Audio encoding failed\n");
						exit(AUDIO_ENCODE_ERROR);
					}

					printf("audio ...........\n");
					if (ret1 == 0){
						stop_encoding = 1;
						break;
					}
					pkt.pts = 0;
					pkt.stream_index = ptr_output_ctx->audio_stream->index;

					int i = av_write_frame(ptr_output_ctx->ptr_format_ctx, &pkt);
				}

				break;

			}
			/*video stream*/
			case AVMEDIA_TYPE_VIDEO:
			{
				 int nEncodedBytes = avcodec_encode_video(
								ptr_output_ctx->video_stream->codec,
								ptr_output_ctx->video_outbuf, ptr_output_ctx->video_outbuf_size,
								NULL);

				if (nEncodedBytes < 0) {
					av_log(NULL, AV_LOG_FATAL, "Video encoding failed\n");
					exit(VIDEO_FLUSH_ERROR);
				}

				printf("video ...........\n");
				if(nEncodedBytes > 0){
					pkt.stream_index = ptr_output_ctx->video_stream->index;
					pkt.data = ptr_output_ctx->video_outbuf; // packet data will be allocated by the encoder
					pkt.size = nEncodedBytes;

					if (ptr_output_ctx->video_stream->codec->coded_frame->pts
							!= AV_NOPTS_VALUE)
						pkt.pts =
								av_rescale_q(
										ptr_output_ctx->video_stream->codec->coded_frame->pts,
										ptr_output_ctx->video_stream->codec->time_base,
										ptr_output_ctx->video_stream->time_base);

					if (ptr_output_ctx->video_stream->codec->coded_frame->key_frame)
						pkt.flags |= AV_PKT_FLAG_KEY;

					av_write_frame(ptr_output_ctx->ptr_format_ctx, &pkt);

					av_free_packet(&pkt);
				}else if(nEncodedBytes == 0){
					stop_encoding = 1;
					break;
				}
				break;
			}
			}//end switch

			if(stop_encoding) break;

		}//end for


	}


}


void do_audio_out(OUTPUT_CONTEXT *ptr_output_ctx ,INPUT_CONTEXT *ptr_input_ctx ,AVFrame *decoded_frame){

	uint8_t *buftmp;
	int64_t audio_buf_size, size_out;

	int frame_bytes, resample_changed;
	AVCodecContext *enc = ptr_output_ctx->audio_stream->codec;
	AVCodecContext *dec = ptr_input_ctx->audio_codec_ctx;
	int osize = av_get_bytes_per_sample(enc->sample_fmt);
	int isize = av_get_bytes_per_sample(dec->sample_fmt);

	/*	in buf ,is the decoded audio data	*/
	uint8_t *buf = ptr_input_ctx->audio_decode_frame->data[0];
	int size = ptr_input_ctx->audio_decode_frame->nb_samples * dec->channels * isize;

	int64_t allocated_for_size = size;

need_realloc:
	audio_buf_size = (allocated_for_size + isize * dec->channels - 1) / (isize * dec->channels);
	audio_buf_size = (audio_buf_size * enc->sample_rate + dec->sample_rate) / dec->sample_rate;
	audio_buf_size = audio_buf_size * 2 + 10000; // safety factors for the deprecated resampling API
	audio_buf_size = FFMAX(audio_buf_size, enc->frame_size);
	audio_buf_size *= osize * enc->channels;

	if (audio_buf_size > INT_MAX) {
		av_log(NULL, AV_LOG_FATAL, "Buffer sizes too large\n");
		exit(1);
	}

	av_fast_malloc(&(ptr_output_ctx->audio_buf), &(ptr_output_ctx->allocated_audio_buf_size), audio_buf_size);
	if (!ptr_output_ctx->audio_buf) {
		av_log(NULL, AV_LOG_FATAL, "Out of memory in do_audio_out\n");
		exit(1);
	}


	/*	judge resample or not*/
    if (enc->channels != dec->channels
    		|| enc->sample_fmt != dec->sample_fmt
    		|| enc->sample_rate!= dec->sample_rate
    )
    	ptr_output_ctx->audio_resample = 1;

    /*	init SwrContext	,perform only one time	*/
	if (ptr_output_ctx->audio_resample && !ptr_output_ctx->swr) {

		printf("ptr_output_ctx->audio_resample = %d ,and we need resample \n" ,ptr_output_ctx->audio_resample);
		ptr_output_ctx->swr = swr_alloc_set_opts(NULL, enc->channel_layout,
				enc->sample_fmt, enc->sample_rate, dec->channel_layout,
				dec->sample_fmt, dec->sample_rate, 0, NULL);

		if (av_opt_set_int(ptr_output_ctx->swr, "ich", dec->channels, 0) < 0) {
			av_log(NULL, AV_LOG_FATAL,
					"Unsupported number of input channels\n");
			exit(1);
		}
		if (av_opt_set_int(ptr_output_ctx->swr, "och", enc->channels, 0) < 0) {
			av_log(NULL, AV_LOG_FATAL,
					"Unsupported number of output channels\n");
			exit(1);
		}

		if (ptr_output_ctx->swr && swr_init(ptr_output_ctx->swr) < 0) {
			av_log(NULL, AV_LOG_FATAL, "swr_init() failed\n");
			swr_free(&ptr_output_ctx->swr);
		}

		if (!ptr_output_ctx->swr) {
			av_log(NULL, AV_LOG_FATAL,
					"Can not resample %d channels @ %d Hz to %d channels @ %d Hz\n",
					dec->channels, dec->sample_rate, enc->channels,
					enc->sample_rate);
			exit(1);
		}

	}

	if(ptr_output_ctx->audio_resample ){

		//swr_convert
		buftmp = ptr_output_ctx->audio_buf;
		size_out = swr_convert(ptr_output_ctx->swr, ( uint8_t*[]) {buftmp},audio_buf_size / (enc->channels * osize),
															 (const uint8_t*[]){buf   }, size / (dec->channels * isize));
		size_out = size_out * enc->channels * osize;
	}else {
		buftmp = buf ;
		size_out = size;

	}


	//write data
	if (1) {
		if (av_fifo_realloc2(ptr_output_ctx->fifo,
				av_fifo_size(ptr_output_ctx->fifo) + size_out) < 0) {
			av_log(NULL, AV_LOG_FATAL, "av_fifo_realloc2() failed\n");
			exit(1);
		}
		av_fifo_generic_write(ptr_output_ctx->fifo, buftmp, size_out, NULL);

		frame_bytes = enc->frame_size * osize * enc->channels;

		while (av_fifo_size(ptr_output_ctx->fifo) >= frame_bytes) {
			//							printf("av_fifo_size(ost->fifo) = %d \n" ,av_fifo_size(ptr_output_ctx->fifo));
			av_fifo_generic_read(ptr_output_ctx->fifo, ptr_output_ctx->audio_buf, frame_bytes,
					NULL);
			encode_audio_frame(ptr_output_ctx, ptr_output_ctx->audio_buf, frame_bytes);
		}
	}

}
