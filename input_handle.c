/*
 * input_handle.c
 *
 *  Created on: Sep 24, 2012
 *      Author: chris
 */
#include <stdio.h>
#include <stdlib.h>

#include "input_handle.h"
#include "chris_error.h"
//ffmpeg header
#include <libavutil/avutil.h>

int init_input(INPUT_CONTEXT *ptr_input_ctx, char* input_file) {

	av_register_all();
	avformat_network_init();

	//open input file
	ptr_input_ctx->ptr_format_ctx = NULL;
	if( avformat_open_input(&ptr_input_ctx->ptr_format_ctx, input_file, NULL, NULL) != 0){
		printf("inputfile init ,avformat_open_input failed .\n");
		exit(AV_OPEN_INPUT_FAIL);
	}

	//find stream info
	if ( avformat_find_stream_info(ptr_input_ctx->ptr_format_ctx, NULL) < 0){
		printf("inputfile init ,avformat_find_stream_info failed .\n");
		exit(AV_FIND_STREAM_INFO_FAIL);
	}

	//find streams in the input file
	ptr_input_ctx->audio_index = -1;
	ptr_input_ctx->video_index = -1;
	int i;
	for (i = 0; i < ptr_input_ctx->ptr_format_ctx->nb_streams; i++) {

		//the first video stream
		if (ptr_input_ctx->video_index < 0
				&& ptr_input_ctx->ptr_format_ctx->streams[i]->codec->codec_type
						== AVMEDIA_TYPE_VIDEO) {
			ptr_input_ctx->video_index = i;

		}

		//the first audio stream
		if (ptr_input_ctx->audio_index < 0
				&& ptr_input_ctx->ptr_format_ctx->streams[i]->codec->codec_type
						== AVMEDIA_TYPE_AUDIO) {
			ptr_input_ctx->audio_index = i;

		}
	}

	printf("audio_index = %d ,video_index = %d \n" ,ptr_input_ctx->audio_index ,
			ptr_input_ctx->video_index);

	if(ptr_input_ctx->video_index < 0 ){

		printf("do not find video stream ..\n");
		exit(NO_VIDEO_STREAM);
	}

	if(ptr_input_ctx->audio_index < 0 ){

		printf("do not find audio stream ..\n");
		exit(NO_AUDIO_STREAM);
	}

	//open video codec
	ptr_input_ctx->video_codec_ctx = ptr_input_ctx->ptr_format_ctx->streams[ptr_input_ctx->video_index]->codec;
	ptr_input_ctx->video_codec = avcodec_find_decoder(ptr_input_ctx->video_codec_ctx->codec_id);
	if(ptr_input_ctx->video_codec == NULL ){

		printf("in inputfile init ,unsupported video codec ..\n");
		exit(UNSPPORT_VIDEO_CODEC);
	}

	if(avcodec_open2(ptr_input_ctx->video_codec_ctx ,ptr_input_ctx->video_codec ,NULL) < 0){

		printf("in inputfile init ,can not open video_codec_ctx ..\n");
		exit(OPEN_VIDEO_CODEC_FAIL);
	}

	//open audio codec
	ptr_input_ctx->audio_codec_ctx = ptr_input_ctx->ptr_format_ctx->streams[ptr_input_ctx->audio_index]->codec;
	ptr_input_ctx->audio_codec = avcodec_find_decoder(ptr_input_ctx->audio_codec_ctx->codec_id);
	if(ptr_input_ctx->audio_codec == NULL ){

		printf("in inputfile init ,unsupported audio codec ..\n");
		exit(UNSPPORT_AUDIO_CODEC);
	}

	if(avcodec_open2(ptr_input_ctx->audio_codec_ctx ,ptr_input_ctx->audio_codec ,NULL) < 0){

		printf("in inputfile init ,can not open audio_codec_ctx ..\n");
		exit(OPEN_AUDIO_CODEC_FAIL);
	}

	printf("in here ,have open video codec ,and audio codec .\n");

	/*	set some mark 	*/
	//ptr_input_ctx->mark_have_frame = 0;

	/*	malloc memory 	*/
	ptr_input_ctx->yuv_frame = avcodec_alloc_frame();
	if(ptr_input_ctx->yuv_frame == NULL){
		printf("yuv_frame allocate failed %s ,%d line\n" ,__FILE__ ,__LINE__);
		exit(MEMORY_MALLOC_FAIL);
	}
//    /*	malloc buffer	*/
//	ptr_input_ctx->encoded_pict = avcodec_alloc_frame();
//    if(ptr_input_ctx->encoded_pict == NULL){
//		printf("yuv_frame allocate failed %s ,%d line\n" ,__FILE__ ,__LINE__);
//		exit(MEMORY_MALLOC_FAIL);
//	}
//    int size = avpicture_get_size(ptr_input_ctx->video_codec_ctx->pix_fmt ,
//    		ptr_input_ctx->video_codec_ctx->width ,
//    		ptr_input_ctx->video_codec_ctx->height);
//
//    ptr_input_ctx->pict_buf = av_malloc(size);
//    if(ptr_input_ctx->pict_buf == NULL){
//    	printf("pict allocate failed ...\n");
//    	exit(MEMORY_MALLOC_FAIL);
//    }
//    //bind
//    avpicture_fill((AVPicture *)ptr_input_ctx->encoded_pict ,ptr_input_ctx->pict_buf ,
//    		ptr_input_ctx->video_codec_ctx->pix_fmt ,
//    		ptr_input_ctx->video_codec_ctx->width ,
//    		ptr_input_ctx->video_codec_ctx->height);

	//audio frame
	ptr_input_ctx->audio_decode_frame = avcodec_alloc_frame();
	if(ptr_input_ctx->audio_decode_frame == NULL){
		printf("audio_decode_frame allocate failed %s ,%d line\n" ,__FILE__ ,__LINE__);
		exit(MEMORY_MALLOC_FAIL);
	}

	return 0;

}


void decode_frame(INPUT_CONTEXT *ptr_input_ctx ,AVPacket *pkt){

//	if(pkt->stream_index == ptr_input_ctx->video_index){
//		//decode video packet
//		int got_picture = 0;
//		ptr_input_ctx->mark_have_frame = 0;
//		avcodec_decode_video2(ptr_input_ctx->video_codec_ctx, ptr_input_ctx->yuv_frame,
//				&got_picture, pkt);
//
//		if(got_picture){
//			ptr_input_ctx->mark_have_frame = 1;
//		}
//
//
//	}else if(pkt->stream_index == ptr_input_ctx->audio_index){
//		//decode audio packet
//
//		int got_frame = 0;
//		int len = avcodec_decode_audio4( ptr_input_ctx->audio_codec_ctx, ptr_input_ctx->audio_decode_frame, &got_frame, &pkt );
//
//		if( len < 0 ) { //decode failed ,skip frame
//				fprintf( stderr, "Error while decoding audio frame\n" );
////					exit(AUDIO_DECODER_FAIL);
//				break;
//		}
//
//		//media_handle->audio_pkt_size -= len;
//		if( got_frame ) {
//				//acquire the large of the decoded audio info...
//				int data_size = av_samples_get_buffer_size(NULL, ptr_input_ctx->audio_codec_ctx->channels,
//						ptr_input_ctx->audio_decode_frame->nb_samples,
//						ptr_input_ctx->audio_codec_ctx->sample_fmt, 1);
//				ptr_input_ctx->audio_size = data_size;  //audio data size
//				audio_buf_index += data_size;
//
////					fwrite(media_handle->audio_decode_frame->data[0] ,1 ,data_size ,ptr_audio_file);
//				return ;
//
//		}else{ //no data
//
//				 chris_printf("no data\n");
//				 media_handle->audio_pkt_size = 0;l
//
//				 if( pkt.data)
//					av_free_packet(&pkt);
//				 continue;
//		}




	//}
}
