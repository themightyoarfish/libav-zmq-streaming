#include "avutils.hpp"
#include "timeutils.hpp"
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
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
  SwsContext *sws_ctx = nullptr;
  AVPixelFormat dst_fmt_ = AV_PIX_FMT_RGBA;

  std::atomic<bool> stop;
  std::atomic<bool> pause;
  std::mutex mtx;
  cv::Mat out;
  std::thread runner;

  static int should_interrupt(void *opaque) {
    return opaque != nullptr && static_cast<RTPReceiver *>(opaque)->stop.load();
  }

public:
  cv::Mat get() {
    std::lock_guard<std::mutex> lock(mtx);
    return out.clone();
  }
  RTPReceiver(const std::string &sdp_path = "test.sdp") {
    stop.store(false);
    pause.store(false);

    /* av_log_set_level(AV_LOG_TRACE); */
    fmt_ctx = avformat_alloc_context();
    fmt_ctx->flags |= (AVFMT_FLAG_NOBUFFER | AVFMT_FLAG_DISCARD_CORRUPT |
                       AVFMT_FLAG_FLUSH_PACKETS);
    av_opt_set(fmt_ctx, "protocol_whitelist", "file,rtp,udp", 0);
    av_opt_set_int(fmt_ctx, "fpsprobesize", 0, 0);
    av_opt_set_int(fmt_ctx, "probesize", 32, 0);
    av_opt_set_int(fmt_ctx, "analyzeduration", 0, 0);
    // do not use over lossy network, fucks it up
    fmt_ctx->max_delay = 0;

    fmt_ctx->interrupt_callback.opaque = (void *)this;
    fmt_ctx->interrupt_callback.callback = &RTPReceiver::should_interrupt;

    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, sdp_path.c_str(), NULL, NULL) < 0) {
      throw std::invalid_argument("Could not open SDP path " + sdp_path);
    }

    current_packet = new AVPacket;
    codec = avcodec_find_decoder(AV_CODEC_ID_VP9);
    if (!codec) {
      throw std::invalid_argument("Could not find decoder");
    }

    dec_ctx = avcodec_alloc_context3(codec);

    dec_ctx->thread_count = 1;
    dec_ctx->codec_id = AV_CODEC_ID_VP9;
    dec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    dec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    dec_ctx->delay = 0;
    /* dec_ctx->thread_type = FF_THREAD_SLICE; */
    std::cout << std::setprecision(5) << std::fixed << std::endl;

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
            if (!sws_ctx) {
              sws_ctx =
                  sws_getContext(current_frame->width, current_frame->height,
                                 AV_PIX_FMT_YUV420P, current_frame->width,
                                 current_frame->height, dst_fmt_, SWS_BILINEAR,
                                 NULL, NULL, NULL);
            }
            std::cout << "Got frame at " << current_millis() << std::endl;

            AVFrame *rgb_frame = av_frame_alloc();
            rgb_frame->width = current_frame->width;
            rgb_frame->height = current_frame->height;
            rgb_frame->format = dst_fmt_;
            rgb_frame->linesize[0] = rgb_frame->width * 4;
            rgb_frame->data[0] =
                new uint8_t[rgb_frame->width * rgb_frame->height * 4 + 16];

            int slice_h = sws_scale(
                sws_ctx, current_frame->data, current_frame->linesize, 0,
                rgb_frame->height, rgb_frame->data, rgb_frame->linesize);
            std::vector<int> sizes{rgb_frame->height, rgb_frame->width};
            std::vector<size_t> steps{
                static_cast<size_t>(rgb_frame->linesize[0])};
            {
              std::lock_guard<std::mutex> lock(mtx);

              out = cv::Mat(sizes, CV_8UC4, rgb_frame->data[0], &steps[0])
                        .clone();
              stamp_image(out, system_clock::now(), 0.4);
            }
            av_frame_free(&rgb_frame);
            av_frame_unref(current_frame);
          } else {
            std::cout << "Did not get frame " << avutils::av_strerror2(success)
                      << std::endl;
          }
        }
        std::cout << "Exited recv loop" << std::endl;
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
  /* av_log_set_level(AV_LOG_TRACE); */
  RTPReceiver receiver;
  while (true) {
    cv::Mat image = receiver.get();
    if (!image.empty()) {
      stamp_image(image, system_clock::now(), 0.6);
      cv::imshow("", image);
      cv::waitKey(1);
    }
  }
}
