#ifndef AVTRANSMITTER_HPP_A9X5A3XE
#define AVTRANSMITTER_HPP_A9X5A3XE

#include "avutils.hpp"
#include <opencv2/core.hpp>
#include <vector>

/**
 * @brief   A class wrapping all encoding functionality for RTP encoding
 */
class AVTransmitter {

  std::vector<std::uint8_t>
      imgbuf; ///< buffer which incoming data gets copied to

  // format, codec, streams and stuff
  AVFormatContext *ofmt_ctx = nullptr;
  AVCodec *out_codec = nullptr;
  AVStream *out_stream = nullptr;
  AVCodecContext *out_codec_ctx = nullptr;
  SwsContext *swsctx = nullptr;

  cv::Mat canvas_; ///< kinda superfluous copy of input data, backed by imgbuf

  // image sizes, determined by first input.
  unsigned int height_;
  unsigned int width_;

  // stream fps, might do nothing
  unsigned int fps_;

  // current encoded frame
  AVFrame *frame_ = nullptr;

  // sdp string to give receivers
  std::string sdp_;

  // stream params
  unsigned int gop_size_;
  unsigned int target_bitrate_;

  bool first_time_ = true;

  /**
   * @brief Function to invoke when a frame is fully transmitted. currently does
   * nothing, but for x264, we want to write some magic sauce here to tell
   * receiver a frame is complete
   */
  void frame_ended();

public:
  AVTransmitter(const std::string &host, const unsigned int port,
                unsigned int fps, unsigned int gop_size = 10,
                unsigned int target_bitrate = 4e6);

  /**
   * @brief Send an image to the stream
   *
   * @param image
   */
  void encode_frame(const cv::Mat &image);

  /**
   * @brief Get the sdp file as string
   *
   * @return    SDP string
   */
  std::string get_sdp() const;

  ~AVTransmitter();
};

#endif /* end of include guard: AVTRANSMITTER_HPP_A9X5A3XE */
