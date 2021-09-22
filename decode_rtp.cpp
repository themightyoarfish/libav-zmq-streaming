#include "avutils.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
}

using namespace std::chrono;

int main(int argc, char **argv) {
  /* av_log_set_level(AV_LOG_TRACE); */
  AVFormatContext *fmt_ctx = avformat_alloc_context();
  fmt_ctx->flags |= (AVFMT_FLAG_NOBUFFER | AVFMT_FLAG_DISCARD_CORRUPT |
                     AVFMT_FLAG_FLUSH_PACKETS);
  av_opt_set(fmt_ctx, "protocol_whitelist", "file,rtp,udp", 0);
  av_opt_set_int(fmt_ctx, "fpsprobesize", 0, 0);
  av_opt_set_int(fmt_ctx, "probesize", 32, 0);
  av_opt_set_int(fmt_ctx, "analyzeduration", 0, 0);
  // do not use over lossy network, fucks it up
  /* fmt_ctx->max_delay = 0; */

  auto src_filename = "test.sdp";
  /* open input file, and allocate format context */
  if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
    std::cout << "Could not open source " << src_filename << std::endl;
    exit(1);
  }
  // can set this to suppress warning that it cant be guessed from stream
  fmt_ctx->streams[0]->r_frame_rate.num = 20;
  std::cout << "Number of streams: " << fmt_ctx->nb_streams << std::endl;

  AVPacket packet;
  av_init_packet(&packet);
  AVCodec *codec = NULL;
  codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  if (!codec) {
    std::cout << "Could not open codec." << std::endl;
  }

  AVCodecContext *dec_ctx = avcodec_alloc_context3(codec);
  const AVRational dst_fps = {20, 1};

  dec_ctx->codec_tag = 0;
  dec_ctx->bit_rate = 2e6;
  // does nothing, unfortunately
  dec_ctx->thread_count = 1;
  dec_ctx->codec_id = AV_CODEC_ID_H264;
  dec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
  dec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  dec_ctx->framerate = dst_fps;
  dec_ctx->time_base = av_inv_q(dst_fps);
  dec_ctx->delay = 0;
  /* dec_ctx->thread_count = 1; */
  /* dec_ctx->thread_type = FF_THREAD_SLICE; */

  if (avcodec_open2(dec_ctx, codec, nullptr) < 0) {
    std::cout << "Could not open context" << std::endl;
    exit(1);
  }
  AVFrame *frame = av_frame_alloc();
  while (av_read_frame(fmt_ctx, &packet) >= 0) {
    int success = avcodec_send_packet(dec_ctx, &packet);
    if (success != 0) {
      std::cout << "Could not send packet: " << avutils::av_strerror2(success)
                << std::endl;
      continue;
    }
    success = avcodec_receive_frame(dec_ctx, frame);
    if (success == 0) {
      std::cout << "Got frame at " << std::setprecision(5) << std::fixed
                << duration_cast<milliseconds>(
                       system_clock::now().time_since_epoch())
                           .count() /
                       1000.0
                << std::endl;
      /* cv::Mat image = avutils::avframeYUV402p2Mat(frame); */
      /* cv::imshow("decoded", image); */
      /* cv::waitKey(2); */
    } else {
      std::cout << "Did not get frame " << avutils::av_strerror2(success)
                << std::endl;
    }
  }
}
