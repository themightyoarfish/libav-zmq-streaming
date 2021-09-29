#ifndef AVTRANSMITTER_HPP_A9X5A3XE
#define AVTRANSMITTER_HPP_A9X5A3XE

#include "avutils.hpp"
#include <opencv2/core.hpp>
#include <vector>

class AVTransmitter {

  std::vector<std::uint8_t> imgbuf;
  AVFormatContext *ofmt_ctx = nullptr;
  AVCodec *out_codec = nullptr;
  AVStream *out_stream = nullptr;
  AVCodecContext *out_codec_ctx = nullptr;
  SwsContext *swsctx = nullptr;
  cv::Mat canvas_;
  unsigned int height_;
  unsigned int width_;
  unsigned int fps_;
  AVFrame *frame_ = nullptr;
  std::string sdp_;

public:
  AVTransmitter(const std::string &host, const unsigned int port,
                unsigned int fps);

  void encode_frame(const cv::Mat &image);

  ~AVTransmitter();

  void frame_ended();

  std::string get_sdp() const;
};

#endif /* end of include guard: AVTRANSMITTER_HPP_A9X5A3XE */
