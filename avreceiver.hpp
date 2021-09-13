#ifndef AVRECEIVER_HPP_SHCTCYOW
#define AVRECEIVER_HPP_SHCTCYOW

#include <zmq.hpp>
#include "avutils.hpp"

class AVReceiver {
  zmq::socket_t socket;
  zmq::context_t ctx;
  AVCodecContext *dec_ctx;
  AVCodecParserContext *parser;
  int successes = 0;
  size_t bytes_received = 0;

public:
  AVReceiver(const std::string &host, const unsigned int port);

  int decode(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt);

  void receive();

  ~AVReceiver();
};

#endif /* end of include guard: AVRECEIVER_HPP_SHCTCYOW */
