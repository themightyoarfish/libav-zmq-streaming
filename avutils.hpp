#ifndef AVUTILS_HPP_L0JIDQTW
#define AVUTILS_HPP_L0JIDQTW

/* #include <opencv2/core.hpp> */
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

constexpr int KB = 1024;

namespace avutils {

std::string av_strerror2(int errnum);

int initialize_avformat_context(AVFormatContext *&fctx,
                                AVOutputFormat *format = nullptr,
                                const char *out_file = nullptr);

void set_codec_params(AVCodecContext *&codec_ctx, double width, double height,
                      int fps, int target_bitrate = 0, int gop_size = 12);

int initialize_codec_stream(AVStream *&stream, AVCodecContext *&codec_ctx,
                            AVCodec *&codec);

SwsContext *initialize_sample_scaler(AVCodecContext *codec_ctx, double width,
                                     double height);

AVFrame *allocate_frame_buffer(AVCodecContext *codec_ctx, double width,
                               double height);

int write_frame(AVCodecContext *codec_ctx, AVFormatContext *fmt_ctx,
                AVFrame *frame);

} // namespace avutils
#endif
