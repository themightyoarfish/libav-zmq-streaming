#include "avutils.hpp"
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <opencv2/highgui.hpp>
#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
}

using namespace std::chrono;

class RTPReceiver {
private:
  AVFormatContext *fmt_ctx;
  AVCodecContext *dec_ctx;
  AVCodec *codec;
  AVFrame *current_frame;
  AVPacket *current_packet;

  std::atomic<bool> stop;
  std::atomic<bool> pause;
  std::thread runner;

public:
  RTPReceiver(const std::string &sdp_path = "test.sdp") {
    stop.store(false);

    /* av_log_set_level(AV_LOG_TRACE); */
    fmt_ctx = avformat_alloc_context();
    fmt_ctx->flags |= (AVFMT_FLAG_NOBUFFER | AVFMT_FLAG_DISCARD_CORRUPT |
                       AVFMT_FLAG_FLUSH_PACKETS);
    av_opt_set(fmt_ctx, "protocol_whitelist", "file,rtp,udp", 0);
    av_opt_set_int(fmt_ctx, "fpsprobesize", 0, 0);
    av_opt_set_int(fmt_ctx, "probesize", 32, 0);
    av_opt_set_int(fmt_ctx, "analyzeduration", 0, 0);
    // do not use over lossy network, fucks it up
    /* fmt_ctx->max_delay = 0; */

    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, sdp_path.c_str(), NULL, NULL) < 0) {
      throw std::invalid_argument("Could not open SDP path");
    }

    current_packet = new AVPacket;
    av_init_packet(current_packet);
    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
      throw std::invalid_argument("Could not find decoder");
    }

    dec_ctx = avcodec_alloc_context3(codec);

    // does nothing, unfortunately
    dec_ctx->thread_count = 1;
    dec_ctx->codec_id = AV_CODEC_ID_H264;
    dec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    dec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    dec_ctx->delay = 0;
    /* dec_ctx->thread_type = FF_THREAD_SLICE; */

    if (avcodec_open2(dec_ctx, codec, nullptr) < 0) {
      throw std::invalid_argument("Could not open context");
    }
    current_frame = av_frame_alloc();

    runner = std::thread([&]() {
      while (!stop.load()) {
        while (!pause.load() && av_read_frame(fmt_ctx, current_packet) >= 0) {
          int success = avcodec_send_packet(dec_ctx, current_packet);
          av_packet_unref(current_packet);
          if (success != 0) {
            std::cout << "Could not send packet: "
                      << avutils::av_strerror2(success) << std::endl;
            continue;
          }
          success = avcodec_receive_frame(dec_ctx, current_frame);
          if (success == 0) {
            std::cout << "Got frame at " << std::setprecision(5) << std::fixed
                      << duration_cast<milliseconds>(
                             system_clock::now().time_since_epoch())
                                 .count() /
                             1000.0
                      << std::endl;
            /* cv::Mat image = avutils::avframeYUV402p2Mat(current_frame); */
            /* cv::imshow("decoded", image); */
            /* cv::waitKey(2); */

            av_frame_unref(current_frame);
          } else {
            std::cout << "Did not get frame " << avutils::av_strerror2(success)
                      << std::endl;
          }
        }
      }
    });
  }

  void setStop() { stop.store(true); }
  void setPause() { pause.store(true); }
  void setUnPause() { pause.store(false); }

  ~RTPReceiver() {
    pause.store(true);
    stop.store(true);
    runner.join();
    avformat_close_input(&fmt_ctx);
    avcodec_free_context(&dec_ctx);
    av_frame_free(&current_frame);
    av_packet_unref(current_packet);
    delete current_packet;
  }
};

int main(int argc, char **argv) {
  RTPReceiver receiver;
  std::this_thread::sleep_until(
      std::chrono::system_clock::now() +
      std::chrono::hours(std::numeric_limits<int>::max()));
}
