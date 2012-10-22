/*
 *  本工程的主要作用 就是完成读取一个mp4 文件，实现将一个文件转码输出成另外一个mp4文件
 *
 * */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"

#include "input_handle.h"
#include "output_handle.h"
#include "chris_error.h"

static uint8_t *audio_buf;
static unsigned int allocated_audio_buf_size;

int main(int argc ,char *argv[]){

	/*first ,open input file ,and obtain input file information*/
	INPUT_CONTEXT *ptr_input_ctx;

	if( (ptr_input_ctx = malloc (sizeof(INPUT_CONTEXT))) == NULL){
		printf("ptr_input_ctx malloc failed .\n");
		exit(MEMORY_MALLOC_FAIL);
	}

	/*second ,open output file ,and set output file information*/
	OUTPUT_CONTEXT *ptr_output_ctx;
	if( (ptr_output_ctx = malloc (sizeof(OUTPUT_CONTEXT))) == NULL){
		printf("ptr_output_ctx malloc failed .\n");
		exit(MEMORY_MALLOC_FAIL);
	}

	//init inputfile ,and get input file information
	init_input(ptr_input_ctx ,argv[1]);


	//init oputfile ,and set output file information
	init_output(ptr_output_ctx ,argv[2] ,ptr_input_ctx);

	ptr_output_ctx->fifo = av_fifo_alloc(1024);
	                if (!ptr_output_ctx->fifo) {
	                    exit (1);
	                }
	                av_log(NULL, AV_LOG_WARNING ,"--av_fifo_size(ost->fifo) = %d \n" ,av_fifo_size(ptr_output_ctx->fifo));  //输出是0？！
	//open video and audio ,set video_out_buf and audio_out_buf
	open_stream_codec(ptr_output_ctx);


    // open the output file, if needed
    if (!(ptr_output_ctx->fmt->flags & AVFMT_NOFILE)) {		//for mp4 or mpegts ,this must be performed
        if (avio_open(&ptr_output_ctx->ptr_format_ctx->pb, argv[2], AVIO_FLAG_WRITE) < 0) {
            fprintf(stderr, "Could not open '%s'\n", argv[2]);
            exit(OPEN_MUX_FILE_FAIL);
        }
    }

    // write the stream header, if any
    avformat_write_header(ptr_output_ctx->ptr_format_ctx ,NULL);

    printf("ptr_output_ctx->ptr_format_ctx->nb_streams = %d \n\n" ,ptr_output_ctx->ptr_format_ctx->nb_streams);  //streams number in output file

    ptr_output_ctx->img_convert_ctx = sws_getContext(
    		ptr_input_ctx->video_codec_ctx->width ,ptr_input_ctx->video_codec_ctx->height ,PIX_FMT_YUV420P,
    		 ptr_output_ctx->video_stream->codec->width ,ptr_output_ctx->video_stream->codec->height ,PIX_FMT_YUV420P ,
    		 SWS_BICUBIC ,NULL ,NULL ,NULL);


    printf("src_width = %d ,src_height = %d \n" ,ptr_input_ctx->video_codec_ctx->width ,ptr_input_ctx->video_codec_ctx->height);
    printf("dts_width = %d ,dts_height = %d \n" ,ptr_output_ctx->video_stream->codec->width ,
    		ptr_output_ctx->video_stream->codec->height);

//    while(1);

    //init audio resample
    if(ptr_output_ctx->swr == NULL){
    	printf("swr is null ...\n");
    }

    printf(" channel layout for out put = %lld \n" ,ptr_output_ctx->audio_stream->codec->channel_layout);
    printf(" channel layout for input put = %lld \n" ,ptr_input_ctx->audio_codec_ctx->channel_layout);

    //add here ,alter ????!!!!!
//    ptr_output_ctx->audio_stream->codec->channel_layout = ptr_input_ctx->audio_codec_ctx->channel_layout;
//    ptr_output_ctx->swr = swr_alloc_set_opts(/*ptr_output_ctx->swr*/ NULL,
//       /*ptr_output_ctx->audio_stream->codec->channel_layout*/
//    		ptr_input_ctx->audio_codec_ctx->channel_layout
//       , ptr_output_ctx->audio_stream->codec->sample_fmt, ptr_output_ctx->audio_stream->codec->sample_rate,
//    	ptr_input_ctx->audio_codec_ctx->channel_layout, ptr_input_ctx->audio_codec_ctx->sample_fmt, ptr_input_ctx->audio_codec_ctx->sample_rate,
//            0, NULL);
//
//    swr_init(ptr_output_ctx->swr);
	printf("before av_read_frame ...\n");
	/*************************************************************************************/
	/*decoder loop*/
	//
	//
	//***********************************************************************************/
	while(av_read_frame(ptr_input_ctx->ptr_format_ctx ,&ptr_input_ctx->pkt) >= 0){

		if (ptr_input_ctx->pkt.stream_index == ptr_input_ctx->video_index) {

			#if 1

			//decode video packet
			int got_picture = 0;
			ptr_input_ctx->mark_have_frame = 0;
			avcodec_decode_video2(ptr_input_ctx->video_codec_ctx,
					ptr_input_ctx->yuv_frame, &got_picture, &ptr_input_ctx->pkt);

			if (got_picture) {
				//encode video
				ptr_output_ctx->sync_ipts = av_q2d(ptr_input_ctx->ptr_format_ctx->streams[ptr_input_ctx->video_index]->time_base) *
						(ptr_input_ctx->yuv_frame->best_effort_timestamp  )
						- (double)ptr_input_ctx->ptr_format_ctx->start_time / AV_TIME_BASE;

				//first swscale
				sws_scale(ptr_output_ctx->img_convert_ctx ,
						(const uint8_t* const*)ptr_input_ctx->yuv_frame->data ,ptr_input_ctx->yuv_frame->linesize ,
						0 ,
						ptr_input_ctx->video_codec_ctx->height ,
						ptr_output_ctx->encoded_yuv_pict->data ,ptr_output_ctx->encoded_yuv_pict->linesize);

				//second swscale
				encode_video_frame(ptr_output_ctx , ptr_output_ctx->encoded_yuv_pict ,ptr_input_ctx);
			}
			#endif

		} else if (ptr_input_ctx->pkt.stream_index == ptr_input_ctx->audio_index) {

//			printf("HAHA.................\n");
			#if 1
			//decode audio packet
			while (ptr_input_ctx->pkt.size > 0) {
				int got_frame = 0;
				int len = avcodec_decode_audio4(ptr_input_ctx->audio_codec_ctx,
						ptr_input_ctx->audio_decode_frame, &got_frame,
						&ptr_input_ctx->pkt);

				if (len < 0) { //decode failed ,skip frame
					fprintf(stderr, "Error while decoding audio frame\n");
					break;
				}

				if (got_frame) {
					//acquire the large of the decoded audio info...

#if 0
					int data_size = av_samples_get_buffer_size(NULL,
							ptr_input_ctx->audio_codec_ctx->channels,
							ptr_input_ctx->audio_decode_frame->nb_samples,
							ptr_input_ctx->audio_codec_ctx->sample_fmt, 1);
					ptr_input_ctx->audio_size = data_size; //audio data size

					//encode audio
					int frame_bytes = ptr_output_ctx->audio_stream->codec->frame_size
					* av_get_bytes_per_sample(ptr_output_ctx->audio_stream->codec->sample_fmt)
					* ptr_output_ctx->audio_stream->codec->channels;

					uint8_t * audio_buf = ptr_input_ctx->audio_decode_frame->data[0];
#endif
#if 1

				    uint8_t *buftmp;
				    int64_t audio_buf_size, size_out;

				    int frame_bytes, resample_changed;
				    AVCodecContext *enc = ptr_output_ctx->audio_stream->codec;
				    AVCodecContext *dec = ptr_input_ctx->audio_codec_ctx;
				    int osize = av_get_bytes_per_sample(enc->sample_fmt);
				    int isize = av_get_bytes_per_sample(dec->sample_fmt);
				    uint8_t *buf = ptr_input_ctx->audio_decode_frame->data[0];
				    int size     = ptr_input_ctx->audio_decode_frame->nb_samples * dec->channels * isize;
				    int64_t allocated_for_size = size;

				need_realloc:
				    audio_buf_size  = (allocated_for_size + isize * dec->channels - 1) / (isize * dec->channels);
				    audio_buf_size  = (audio_buf_size * enc->sample_rate + dec->sample_rate) / dec->sample_rate;
				    audio_buf_size  = audio_buf_size * 2 + 10000; // safety factors for the deprecated resampling API
				    audio_buf_size  = FFMAX(audio_buf_size, enc->frame_size);
				    audio_buf_size *= osize * enc->channels;

				    if (audio_buf_size > INT_MAX) {
				        av_log(NULL, AV_LOG_FATAL, "Buffer sizes too large\n");
				        exit(1);
				    }

				    av_fast_malloc(&audio_buf, &allocated_audio_buf_size, audio_buf_size);
				    if (!audio_buf) {
				        av_log(NULL, AV_LOG_FATAL, "Out of memory in do_audio_out\n");
				        exit(1);
				    }

				    static int chris_ii = 0;
//				    if(chris_ii == 0){
				    if(1){
				    	chris_ii ++;

				    	ptr_output_ctx->swr = swr_alloc_set_opts(NULL,
				    					                                             enc->channel_layout, enc->sample_fmt, enc->sample_rate,
				    					                                             dec->channel_layout, dec->sample_fmt, dec->sample_rate,
				    					                                             0, NULL);


							if (av_opt_set_int(ptr_output_ctx->swr, "ich", dec->channels, 0) < 0) {
							   av_log(NULL, AV_LOG_FATAL, "Unsupported number of input channels\n");
							   exit(1);
						   }
						   if (av_opt_set_int(ptr_output_ctx->swr, "och", enc->channels, 0) < 0) {
							   av_log(NULL, AV_LOG_FATAL, "Unsupported number of output channels\n");
							   exit(1);
						   }

						   if(ptr_output_ctx->swr && swr_init(ptr_output_ctx->swr) < 0){
								 av_log(NULL, AV_LOG_FATAL, "swr_init() failed\n");
								 swr_free(&ptr_output_ctx->swr);
							 }

							 if (!ptr_output_ctx->swr) {
								 av_log(NULL, AV_LOG_FATAL, "Can not resample %d channels @ %d Hz to %d channels @ %d Hz\n",
										 dec->channels, dec->sample_rate,
										 enc->channels, enc->sample_rate);
								 exit(1);
							 }

				    }



					 buftmp = audio_buf;
					size_out = swr_convert(ptr_output_ctx->swr, (      uint8_t*[]){buftmp}, audio_buf_size / (enc->channels * osize),
													 (const uint8_t*[]){buf   }, size / (dec->channels * isize));
					size_out = size_out * enc->channels * osize;


					if(1){
						if (av_fifo_realloc2(ptr_output_ctx->fifo, av_fifo_size(ptr_output_ctx->fifo) + size_out) < 0) {
							av_log(NULL, AV_LOG_FATAL, "av_fifo_realloc2() failed\n");
							exit(1);
						}
						av_fifo_generic_write(ptr_output_ctx->fifo, buftmp, size_out, NULL);

						frame_bytes = enc->frame_size * osize * enc->channels;

						while (av_fifo_size(ptr_output_ctx->fifo) >= frame_bytes) {
//							printf("av_fifo_size(ost->fifo) = %d \n" ,av_fifo_size(ptr_output_ctx->fifo));
							av_fifo_generic_read(ptr_output_ctx->fifo, audio_buf, frame_bytes, NULL);
							encode_audio_frame(ptr_output_ctx, audio_buf, frame_bytes);
						}
					}

#endif
#if 0
/////这中间的是 为了resample 添加的
					while (data_size >= frame_bytes) {

						encode_audio_frame(ptr_output_ctx ,audio_buf ,frame_bytes /*data_size*/);  //
						data_size -= frame_bytes;
						audio_buf += frame_bytes;
					}
#endif
				} else { //no data
					printf("======>avcodec_decode_audio4 ,no data ..\n");
					continue;
				}

				ptr_input_ctx->pkt.size -= len;
				ptr_input_ctx->pkt.data += len;

			}
			#endif
		}

	}//endwhile


	printf("before flush ,ptr_output_ctx->ptr_format_ctx->nb_streams = %d \n\n" ,ptr_output_ctx->ptr_format_ctx->nb_streams);
	encode_flush(ptr_output_ctx ,ptr_output_ctx->ptr_format_ctx->nb_streams);

	printf("before wirite tailer ...\n\n");

	av_write_trailer(ptr_output_ctx->ptr_format_ctx );

	/*free memory*/
}
