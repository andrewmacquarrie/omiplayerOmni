#include "stdafx.h"
#include "VideoPlayer.h"


VideoPlayer::VideoPlayer()
{
	format = nullptr;

	video_decoder = nullptr;
	video_context = nullptr;
	video_stream = nullptr;
	video_convert_ctx = nullptr;
	video_frame = nullptr;
	video_stream_index = -1;
	//video_tex = 0;
	rgb_size = 0;
	width = 0;
	height = 0;
	video_current_pbo = 0;
	//memset(video_pbos, 0, sizeof(video_pbos));

	audio_decoder = nullptr;
	audio_context = nullptr;
	audio_stream = nullptr;
	audio_frame = nullptr;
	audio_resample = nullptr;
	audio_stream_index = -1;
	audio_source = 0;
	audio_play = false;
}


VideoPlayer::~VideoPlayer()
{
}


bool VideoPlayer::load(const char* filename, bool ignore_audio)
{
	path = filename;

	// Open file and read streams info
	if (avformat_open_input(&format, filename, nullptr, nullptr) < 0)
		return false;
	if (!avformat_find_stream_info(format, nullptr) < 0)
		return false;

	// Open video stream
	video_stream_index = av_find_best_stream(format,
		AVMEDIA_TYPE_VIDEO, -1, -1, &video_decoder, 0);
	if (video_stream_index >= 0)
	{
		video_stream = format->streams[video_stream_index];
		video_context = video_stream->codec;
		if (avcodec_open2(video_context, video_decoder, nullptr) == 0)
		{
			width = video_context->width;
			width2 = width / 2;
			height = video_context->height;
			height2 = height / 2;

			// Setup video buffers
			video_frame = av_frame_alloc();
			video_frame->format = video_context->pix_fmt;
			video_frame->width = width;
			video_frame->height = height;
			av_frame_get_buffer(video_frame, 0);
			avcodec_default_get_buffer2(video_context, video_frame, AV_GET_BUFFER_FLAG_REF);

			rgb_size = width * (height+height2);
		}
		else
		{
			printf("Unable to open the video codec.\n");
			video_stream_index = -1;
			return 0;
		}
	}
	else
	{
		printf("Video stream not found.\n");
		video_stream_index = -1;
		return 0;
	}

	// Open audio stream
	audio_stream_index = av_find_best_stream(format,
		AVMEDIA_TYPE_AUDIO, -1, -1, &audio_decoder, 0);
	if (audio_stream_index >= 0 && !VP_IGNORE_AUDIO && !ignore_audio)
	{
		audio_stream = format->streams[audio_stream_index];
		audio_context = audio_stream->codec;
		if (avcodec_open2(audio_context, audio_decoder, nullptr) == 0)
		{
			// Setup buffer
			audio_frame = av_frame_alloc();

			// Setup audio resampler
			audio_resample = avresample_alloc_context();
			av_opt_set_int(audio_resample, "in_channel_layout", audio_context->channel_layout, 0);
			av_opt_set_int(audio_resample, "in_sample_rate", audio_context->sample_rate, 0);
			av_opt_set_int(audio_resample, "in_sample_fmt", audio_context->sample_fmt, 0);
			av_opt_set_int(audio_resample, "out_channel_layout", audio_context->channel_layout, 0);
			av_opt_set_int(audio_resample, "out_sample_rate", audio_context->sample_rate, 0);
			av_opt_set_int(audio_resample, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
			if (avresample_open(audio_resample) != 0)
			{
				printf("Cannot open audio resampler.\n");
				audio_stream_index = -1;
			}

			// Create OpenAL buffers
			alGenSources(1, &audio_source);
		}
		else
		{
			printf("Unable to open the audio codec.\n");
			audio_stream_index = -1;
		}
	}
	else
	{
		printf("No audio track found.\n");
		audio_stream_index = -1;
	}

	if (onLoad)
		onLoad();

	return true;
}

FrameTexture VideoPlayer::decodeFrame(ID3D11Device* d3d_device, uint8_t* textureBuffer)
{
	static uint8_t* buffer = new uint8_t[width * (height + height2)];
	static AVPacket packet;
	static int rewinded = 0;
	int decoded_video = 0;
	int decoded_audio = 0;

	FrameTexture texture;

	if (!format)
		return texture;

	int64_t cur_video_dst = 0;
	int64_t last_audio_dst = 0;

	int audio_queue_size = 10;
	while (!decoded_video || (audio_stream_index >= 0 && (audio_queue_size < 5/* || last_audio_dst <= cur_video_dst*/)))
	{
		av_init_packet(&packet);
		av_read_frame(format, &packet);
		if (packet.stream_index == video_stream_index)
		{
			int ret = avcodec_decode_video2(video_context, video_frame, &decoded_video, &packet);
			if (ret <= 0 && !decoded_video && !rewinded)
			{
				if (onComplete)
					onComplete();
				rewind();
				rewinded = 1;
				break;
			}
			if (decoded_video)
			{
				rewinded = 0;
				//printf("video frame decoded\n"); 

				if (!cur_video_dst)
					cur_video_dst = video_frame->pkt_dts;

				if (textureBuffer)
				{
					uint8_t* ptr = textureBuffer;
					if (ptr)
					{
						// Copy Y plane
						for (int i = 0; i < height; i++)
							memcpy(ptr + i*width, video_frame->data[0] + i * video_frame->linesize[0], width);
						// Copy U and V plane
						for (int i = 0; i < height2; i++)
						{
							memcpy(ptr + (i + height) * width, video_frame->data[1] + i * video_frame->linesize[1], width2);
							memcpy(ptr + (i + height) * width + width2, video_frame->data[2] + i * video_frame->linesize[2], width2);
						}
					}
				}

				if (d3d_device)
				{
					uint8_t* ptr = buffer;
					if (ptr)
					{
						// Copy Y plane
						for (int i = 0; i < height; i++)
							memcpy(ptr + i*width, video_frame->data[0] + i * video_frame->linesize[0], width);
						// Copy U and V plane
						for (int i = 0; i < height2; i++)
						{
							memcpy(ptr + (i + height) * width, video_frame->data[1] + i * video_frame->linesize[1], width2);
							memcpy(ptr + (i + height) * width + width2, video_frame->data[2] + i * video_frame->linesize[2], width2);
						}
					}

					D3D11_TEXTURE2D_DESC tdesc{ 0 };
					tdesc.Width = width;
					tdesc.Height = height * 1.5f;
					tdesc.ArraySize = 1;
					tdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
					tdesc.CPUAccessFlags = 0;
					tdesc.Format = DXGI_FORMAT_R8_UNORM;
					tdesc.MipLevels = 1;
					tdesc.MiscFlags = 0;
					tdesc.SampleDesc.Count = 1;
					tdesc.SampleDesc.Quality = 0;
					tdesc.Usage = D3D11_USAGE_DEFAULT;
					D3D11_SUBRESOURCE_DATA tsrd{ 0 };
					tsrd.pSysMem = buffer;
					tsrd.SysMemPitch = width;
					HRESULT res = d3d_device->CreateTexture2D(&tdesc, &tsrd, &texture.tex);
					assert(res == S_OK);

					D3D11_SHADER_RESOURCE_VIEW_DESC srvd;
					ZeroMemory(&srvd, sizeof(srvd));
					srvd.Format = tdesc.Format;
					srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
					srvd.Texture2D.MipLevels = 1;
					res = d3d_device->CreateShaderResourceView(texture.tex, &srvd, &texture.view);
					assert(res == S_OK);
				}
			}
		}
		else if (packet.stream_index == audio_stream_index)
		{
			avcodec_decode_audio4(audio_context, audio_frame, &decoded_audio, &packet);
			if (decoded_audio)
			{
				last_audio_dst = audio_frame->pkt_dts;

				int lineSize = 0;
				int dataSize = av_samples_get_buffer_size(&lineSize, audio_context->channels,
					audio_frame->nb_samples, AV_SAMPLE_FMT_S16, 1);

				uint8_t* buffer = (uint8_t*)av_mallocz(dataSize);
				int samples = avresample_convert(audio_resample, &buffer, lineSize, audio_frame->nb_samples,
					audio_frame->data, audio_frame->linesize[8], audio_frame->nb_samples);


				ALuint albuffer;
				alGenBuffers(1, &albuffer);

				alBufferData(albuffer, AL_FORMAT_STEREO16, buffer, dataSize, audio_frame->sample_rate);
				alSourceQueueBuffers(audio_source, 1, &albuffer);

				audio_buffer_queue.push_front(albuffer);

				//printf("Decoded audio %d buffer %d samples, queue size %d\n",
				//    dataSize, samples, audio_buffer_queue.size());
			}
		}

		int val = 0;

		alGetSourcei(audio_source, AL_BUFFERS_PROCESSED, &val);
		while (val--)
		{
			alSourceUnqueueBuffers(audio_source, 1, &audio_buffer_queue.back());
			alDeleteBuffers(1, &audio_buffer_queue.back());
			audio_buffer_queue.pop_back();
		}

		alGetSourcei(audio_source, AL_BUFFERS_QUEUED, &val);
		audio_queue_size = val;

		alGetSourcei(audio_source, AL_SOURCE_STATE, &val);
		if (audio_play && val != AL_PLAYING)
			alSourcePlay(audio_source);


		av_free_packet(&packet);
	}
	return texture;
}

void VideoPlayer::close()
{
	if (format)
	{
		if (video_stream_index >= 0)
		{
			sws_freeContext(video_convert_ctx);
			av_frame_free(&video_frame);
			avcodec_close(video_context);
		}
		if (audio_stream_index >= 0)
		{
			avresample_free(&audio_resample);
			av_frame_free(&audio_frame);
			avcodec_close(audio_context);
		}
		avformat_close_input(&format);
	}
	format = nullptr;
}

void VideoPlayer::rewind()
{
    if (format) {
        //av_seek_frame(format, video_stream_index, 0, AVSEEK_FLAG_BACKWARD);
        close();
        load(path.c_str(), disable_audio);
    }
}

void VideoPlayer::getNextFrame()
{
	//decodeFrame(TODO, TODO);
}
