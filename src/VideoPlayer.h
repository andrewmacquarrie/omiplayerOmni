#pragma once

#define VP_IGNORE_AUDIO false
#define VP_PBOS 16

struct FrameTexture
{
	ID3D11Texture2D* tex;
	ID3D11ShaderResourceView* view;
	FrameTexture() : tex(nullptr), view(nullptr) {}
	FrameTexture(ID3D11Texture2D* t, ID3D11ShaderResourceView* v) : tex(t), view(v) {}
};

class VideoPlayer
{
public:
	std::string      path;
	AVFormatContext* format;
    bool             disable_audio;

	AVCodec*        video_decoder;
	AVCodecContext* video_context;
	AVStream*       video_stream;
	AVStream		video_queue;
	SwsContext*     video_convert_ctx;
	AVFrame*        video_frame;
	int             video_stream_index;
	//GLuint          video_tex;
	//GLuint          video_pbos[VP_PBOS];
	int             video_current_pbo;
	int rgb_size;
	int width;
	int height;
	int width2;
	int height2;

	AVCodec*        audio_decoder;
	AVCodecContext* audio_context;
	AVStream*       audio_stream;
	AVFrame*        audio_frame;
	int             audio_stream_index;
	bool            audio_play;
	ALuint          audio_source;
	AVAudioResampleContext* audio_resample;
	std::deque<ALuint> audio_buffer_queue;

	int time_decode;
	int time_convert;
	int time_upload;

	VideoPlayer();
	~VideoPlayer();

	bool load(const char* filename, bool ignore_audio = false);
	FrameTexture decodeFrame(ID3D11Device* dev, uint8_t* textureBuffer = nullptr);
	void getNextFrame();
	void rewind();
	void close();

	std::function<void(void)> onComplete;
	std::function<void(void)> onLoad;
	std::function<void(AVFrame*)> onFrameDecoded;
};
