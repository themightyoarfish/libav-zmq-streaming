#ifndef AVUTILS_HPP_L0JIDQTW
#define AVUTILS_HPP_L0JIDQTW

#include <opencv2/core.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

constexpr int KB = 1024;

namespace avutils {

/**
 * @brief   Get string name of ffmpeg error code
 *
 * @param errnum    Error code
 *
 * @return  screen name
 */
std::string av_strerror2(int errnum);

/**
 * @brief   Initialize a format context to mux data to
 *
 * @param fctx  Context to initialize
 * @param format    Desired output format
 * @param out_file  File name for disk output formats
 *
 * @return  error code
 */
int initialize_avformat_context(AVFormatContext *&fctx,
                                AVOutputFormat *format = nullptr,
                                const char *out_file = nullptr);

/**
 * @brief   Parametrize a codec context
 *
 * @param codec_ctx encoding/decoding context
 * @param width width of the video
 * @param height    height of the video
 * @param fps   target fps of the stream
 * @param target_bitrate    desired bitrate
 * @param gop_size  group-of-picture parameter (not sure if VP9 actually uses this, but
 * h264 does)
 */
void set_codec_params(AVCodecContext *&codec_ctx, double width, double height,
                      int fps, int target_bitrate = 0, int gop_size = 12);

/**
 * @brief   Initialize an input or output stream. this sets all kinds of stream
 * parameters to minimize latency
 *
 * @param stream    usually output stream for encoding
 * @param codec_ctx codec context
 * @param codec codec used
 *
 * @return error code
 */
int initialize_codec_stream(AVStream *&stream, AVCodecContext *&codec_ctx,
                            AVCodec *&codec);

/**
 * @brief   Get a software scaling context that only does RGB conversion without
 * changing size
 *
 * @param codec_ctx encoding/decoding context
 * @param width input and output width
 * @param height input and output height
 *
 * @return pointer to sws context
 */
SwsContext *initialize_sample_scaler(AVCodecContext *codec_ctx, double width,
                                     double height);

/**
 * @brief   Create a new frame and allocate the `data` field. The data pointers and the
 * frame itself must be freed by the user with `av_freep()` and `av_frame_free()`.
 *
 * @param codec_ctx Codec context to get pixel format from
 * @param width Frame width
 * @param height    Frame height
 *
 * @return new frame
 */
AVFrame *allocate_frame_buffer(AVCodecContext *codec_ctx, double width,
                               double height);

/**
 * @brief   Send frame to encoding context and send resulting packet to format context.
 *
 * @param codec_ctx encoding ctx
 * @param fmt_ctx   output format ctx
 * @param frame Frame to send
 *
 * @return  0 on success, < 0 on error
 */
int write_frame(AVCodecContext *codec_ctx, AVFormatContext *fmt_ctx,
                AVFrame *frame);

/**
 * @brief   Generate dummy data in opencv mat
 *
 * @param image dst image
 * @param i
 */
void generatePattern(cv::Mat &image, unsigned char i);

/**
 * @brief   Convert frame in planar yuv 402 pixel format to opencv Mat (BGR8 interleaved)
 *
 * @param frame Input frame (decoded from stream)
 *
 * @return  Matrix with BGR8 data
 */
cv::Mat avframeYUV402p2Mat(const AVFrame *frame);

} // namespace avutils
#endif
