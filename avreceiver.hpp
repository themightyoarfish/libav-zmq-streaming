#ifndef AVRECEIVER_HPP_SHCTCYOW
#define AVRECEIVER_HPP_SHCTCYOW

#include "avutils.hpp"
#include <zmq.hpp>

/**
 * @brief   A class which receives encoded video packets over zmq and decodes
 * them to opencv images which are displayed
 * @warning This is more or less test code to proof of concept.
 */
class AVReceiver {
  zmq::socket_t socket; ///< receiver socket
  zmq::context_t ctx;
  AVCodecContext *dec_ctx;      ///< decoding context
  AVCodecParserContext *parser; ///< parser

  // keep some stats for printing
  int successes = 0;
  size_t bytes_received = 0;

public:
  /**
   * @brief ctor
   *
   * @param host    Interface to bind to
   * @param port    Port to bind to
   */
  AVReceiver(const std::string &host, const unsigned int port);

  /**
   * @brief Decode packets into frames (potentially). Each invocation might lead
   * to a new frame being ready, require more data, or error.
   *
   * @param dec_ctx Decoding context
   * @param frame   Frame to fill
   * @param pkt New packet to decode
   *
   * @return    0 if new frame is complete, < 0 otherwise.
   */
  int decode(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt);

  /**
   * @brief Run the receive loop
   */
  void receive();

  ~AVReceiver();
};

#endif /* end of include guard: AVRECEIVER_HPP_SHCTCYOW */
