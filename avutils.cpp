#include <chrono>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <thread>
#include <vector>

#include "avutils.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

namespace avutils {

std::string av_strerror2(int errnum) {
  std::vector<char> v(KB);
  av_strerror(errnum, v.data(), v.size());
  return std::string(v.begin(), v.end());
}

int initialize_avformat_context(AVFormatContext*& fctx,
                                AVOutputFormat* format,
                                const char* out_file) {
  return avformat_alloc_output_context2(&fctx, format, format->name, out_file);
}

void set_codec_params(AVCodecContext*& codec_ctx,
                      double width,
                      double height,
                      int fps,
                      int target_bitrate,
                      int gop_size) {
  const AVRational dst_fps = {fps, 1};

  codec_ctx->codec_tag = 0;
  if (target_bitrate > 0) {
    codec_ctx->bit_rate = target_bitrate;
    codec_ctx->rc_max_rate = target_bitrate;
    codec_ctx->rc_min_rate = target_bitrate;
    codec_ctx->bit_rate_tolerance = 0;
  }
  codec_ctx->thread_count = 1;
  codec_ctx->codec_id     = AV_CODEC_ID_VP9;
  codec_ctx->codec_type   = AVMEDIA_TYPE_VIDEO;
  codec_ctx->width        = width;
  codec_ctx->height       = height;
  codec_ctx->gop_size     = gop_size;
  codec_ctx->pix_fmt      = AV_PIX_FMT_YUV420P;
  codec_ctx->framerate    = dst_fps;
  codec_ctx->time_base    = av_inv_q(dst_fps);
  /* codec_ctx->thread_type = FF_THREAD_SLICE; */
}

int initialize_codec_stream(AVStream*& stream,
                            AVCodecContext*& codec_ctx,
                            AVCodec*& codec) {
  AVDictionary* codec_options = nullptr;
  /* av_dict_set(&codec_options, "profile", "high", 0); */
  /* av_dict_set(&codec_options, "preset", "ultrafast", 0); */
  /* av_dict_set(&codec_options, "tune", "zerolatency", 0); */
  /* av_dict_set_int(&codec_options, "aud", 1, 0); */
  av_dict_set(&codec_options, "deadline", "realtime", 0);
  av_dict_set(&codec_options, "quality", "realtime", 0);
  av_dict_set_int(&codec_options, "speed", 8, 0);
  av_dict_set_int(&codec_options, "row-mt", 1, 0);
  av_dict_set_int(&codec_options, "lag-in-frames", 0, 0);
  av_dict_set_int(&codec_options, "tile-columns", 5, 0);
  av_dict_set_int(&codec_options, "frame-parallel", 0, 0);

  // open video encoder
  int ret = avcodec_open2(codec_ctx, codec, &codec_options);

  if (codec_ctx->extradata_size > 0) {
    /* std::cout << "Extradata present in AVCodecContext" << std::endl; */
  } else {
    /* std::cout << "No Extradata present in AVFormatContext" << std::endl; */
  }

  bool all_found       = true;
  AVDictionaryEntry* e = nullptr;
  while ((e = av_dict_get(codec_options, "", e, AV_DICT_IGNORE_SUFFIX))) {
    /* std::cout << "Did not find option " << e->key << ": " << e->value */
    /*           << std::endl; */
    all_found = false;
  }
  if (all_found) {
    /* std::cout << "All codec options found." << std::endl; */
  }

  ret = avcodec_parameters_from_context(stream->codecpar, codec_ctx);
  return ret;
}

SwsContext* initialize_sample_scaler(AVCodecContext* codec_ctx,
                                     double width,
                                     double height) {
  SwsContext* swsctx = sws_getContext(width, height, AV_PIX_FMT_RGB24, width,
                                      height, codec_ctx->pix_fmt, SWS_BICUBIC,
                                      nullptr, nullptr, nullptr);
  return swsctx;
}

AVFrame*
allocate_frame_buffer(AVCodecContext* codec_ctx, double width, double height) {
  AVFrame* frame = av_frame_alloc();

  std::uint8_t* framebuf = new uint8_t[av_image_get_buffer_size(
      codec_ctx->pix_fmt, width, height, 1)];
  av_image_fill_arrays(frame->data, frame->linesize, framebuf,
                       codec_ctx->pix_fmt, width, height, 1);
  frame->width  = width;
  frame->height = height;
  frame->format = static_cast<int>(codec_ctx->pix_fmt);
  frame->pts    = 0;

  return frame;
}

int write_frame(AVCodecContext* codec_ctx,
                AVFormatContext* fmt_ctx,
                AVFrame* frame) {
  /* av_init_packet(&pkt); */

  int ret = avcodec_send_frame(codec_ctx, frame);
  if (ret < 0) {
    return ret;
  }

  while (ret >= 0 && ret != AVERROR(EAGAIN)) {
    AVPacket pkt = {0};
    ret          = avcodec_receive_packet(codec_ctx, &pkt);
    av_write_frame(fmt_ctx, &pkt);
    if (ret == AVERROR(EAGAIN)) {
      // this signals encoder needs new input, so we are done sending
      return 0;
    } else if (ret < 0) {
      // legitimate problem
      return ret;
    }
  }

  return 0;
}

void generatePattern(cv::Mat& image, unsigned char i) {
  image.setTo(cv::Scalar(255, 255, 255));
  float perc_height = 1.0 * i / 255;
  float perc_width  = 1.0 * i / 255;
  image.row(perc_height * image.rows).setTo(cv::Scalar(0, 0, 0));
  image.col(perc_width * image.cols).setTo(cv::Scalar(0, 0, 0));
}

cv::Mat avframeYUV402p2Mat(const AVFrame* frame) {
  std::vector<int> sizes = {frame->height, frame->width};
  std::vector<size_t> steps{static_cast<size_t>(frame->linesize[0])};

  const auto height      = frame->height;
  const auto width       = frame->width;
  const auto actual_size = cv::Size(width, height);
  void* y                = frame->data[0];
  void* u                = frame->data[1];
  void* v                = frame->data[2];

  cv::Mat y_mat(height, width, CV_8UC1, y, frame->linesize[0]);
  cv::Mat u_mat(height / 2, width / 2, CV_8UC1, u, frame->linesize[1]);
  cv::Mat v_mat(height / 2, width / 2, CV_8UC1, v, frame->linesize[2]);
  cv::Mat u_resized, v_resized;
  cv::resize(u_mat, u_resized, actual_size, 0, 0,
             cv::INTER_NEAREST);  // repeat u values 4 times
  cv::resize(v_mat, v_resized, actual_size, 0, 0,
             cv::INTER_NEAREST);  // repeat v values 4 times

  cv::Mat yuv;

  std::vector<cv::Mat> yuv_channels = {y_mat, u_resized, v_resized};
  cv::merge(yuv_channels, yuv);

  cv::Mat image;
  cv::cvtColor(yuv, image, cv::COLOR_YUV2RGB);
  return image;
}
}  // namespace avutils
