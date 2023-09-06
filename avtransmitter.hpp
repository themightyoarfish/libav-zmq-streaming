#ifndef AVTRANSMITTER_HPP_A9X5A3XE
#define AVTRANSMITTER_HPP_A9X5A3XE

#include <opencv2/core.hpp>
#include <vector>

#include "avutils.hpp"

class AVTransmitter {
  std::vector<std::uint8_t> imgbuf;
  AVFormatContext* ofmt_ctx     = nullptr;
  AVCodec* out_codec      = nullptr;
  AVStream* out_stream          = nullptr;
  AVCodecContext* out_codec_ctx = nullptr;
  SwsContext* swsctx            = nullptr;
  cv::Mat canvas_;
  unsigned int height_;
  unsigned int width_;
  unsigned int fps_;
  AVFrame* frame_ = nullptr;
  std::string sdp_;
  unsigned int gop_size_;
  unsigned int target_bitrate_;
  bool first_time_ = true;

public:
  AVTransmitter(const std::string& host,
                const unsigned int port,
                unsigned int fps,
                unsigned int gop_size       = 10,
                unsigned int target_bitrate = 4e6);

  void encode_frame(const cv::Mat& image);

  ~AVTransmitter();

  void frame_ended();

  std::string get_sdp() const;
};

#endif /* end of include guard: AVTRANSMITTER_HPP_A9X5A3XE */
