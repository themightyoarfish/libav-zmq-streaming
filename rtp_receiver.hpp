#pragma once

#include <atomic>
#include <boost/thread/sync_bounded_queue.hpp>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <opencv2/highgui.hpp>
#include <thread>

#include "avutils.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
}

class RTPReceiver{

private:
  AVFormatContext* fmt_ctx;
  AVCodecContext* dec_ctx;
  const AVCodec* codec;
  AVFrame* current_frame;
  AVPacket* current_packet;
  SwsContext* sws_ctx    = nullptr;
  AVPixelFormat dst_fmt_ = AV_PIX_FMT_BGRA;
  boost::sync_bounded_queue<cv::Mat> queue;

  std::atomic<bool> stop;
  std::atomic<bool> pause;
  std::thread runner;

  static int should_interrupt(void *opaque) {
    return opaque != nullptr &&
           static_cast<RTPReceiver *>(opaque)->stop.load();
  }

public:
  RTPReceiver(const std::string &sdp_path);

  cv::Mat get();

  void setStop();

  void setPause();

  void setUnPause();

  ~RTPReceiver();
};
