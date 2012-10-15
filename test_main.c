/*
 *  本工程的主要作用 就是完成读取一个mp4 文件，实现将一个文件转码输出成另外一个mp4文件
 *
 * */
#include <stdio.h>
#include <stdlib.h>

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"

#include "input_handle.h"
#include "output_handle.h"
#include "chris_error.h"

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

					while (data_size >= frame_bytes) {

						encode_audio_frame(ptr_output_ctx ,audio_buf ,frame_bytes /*data_size*/);  //
						data_size -= frame_bytes;
						audio_buf += frame_bytes;
					}

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
