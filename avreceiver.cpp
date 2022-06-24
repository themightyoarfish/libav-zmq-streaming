#include "avreceiver.hpp"
#include "avutils.hpp"
#include <chrono>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <thread>
#include <vector>
#include <zmq.hpp>

AVReceiver::AVReceiver(const std::string &host, const unsigned int port)
    : ctx(1) {
  socket = zmq::socket_t(ctx, zmq::socket_type::sub);
  const auto connect_str =
      std::string("tcp://") + host + ":" + std::to_string(port);
  socket.set(zmq::sockopt::subscribe, "");
  socket.set(zmq::sockopt::rcvhwm, 2);
  socket.connect(connect_str);
  std::cout << "Connected socket to " << connect_str << std::endl;
  const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_VP9);
  if (!codec) {
    throw std::runtime_error("Could not find decoder");
  }
  dec_ctx = avcodec_alloc_context3(codec);
  if (!dec_ctx) {
    throw std::runtime_error("Could not allocate decoder context");
  }
  parser = av_parser_init(codec->id);
  if (!parser) {
    throw std::runtime_error("Could not init parser");
  }
  /* const AVRational dst_fps = {fps, 1}; */
  dec_ctx->codec_tag = 0;
  dec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
  int res = avcodec_open2(dec_ctx, codec, nullptr);
  if (res < 0) {
    throw std::runtime_error("Could not open decoder context: " +
                             avutils::av_strerror2(res));
  }
}

int AVReceiver::decode(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt) {
  int ret;

  ret = avcodec_send_packet(dec_ctx, pkt);
  if (ret < 0) {
    std::cerr << "Error sending packet for decoding: "
              << avutils::av_strerror2(ret) << std::endl;
    return ret;
  } else {
    /* std::cout << "Sent packet of size " << pkt->size << " bytes" <<
     * std::endl; */
  }

  while (ret >= 0) {
    ret = avcodec_receive_frame(dec_ctx, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      /* std::cerr << "Decoder shits bed: " << avutils::av_strerror2(ret) <<
       * std::endl;
       */
      return ret;
    } else if (ret < 0) {
      std::cerr << "Error during decoding: " << avutils::av_strerror2(ret)
                << std::endl;
      return ret;
    } else {
      // Ok to return? might be more than one frame ready? add callback for
      // this
      return 0;
    }
  }
  return 0;
}

void AVReceiver::receive() {
  bool more = true;
  int num_pkts = 0;
  zmq::message_t incoming;
  std::vector<std::uint8_t> buffer;
  AVFrame *frame = av_frame_alloc();
  AVPacket *pkt = av_packet_alloc();
  while (more) {
    socket.recv(incoming, zmq::recv_flags::none);
    bytes_received += incoming.size();
    ++num_pkts;
    /* std::cout << "Received packet of size " << incoming.size() <<
     * std::endl; */
    if (incoming.to_string().find("packet") != std::string::npos) {
      continue;
    } else {
      static int i = 0;
      buffer.resize(incoming.size() + AV_INPUT_BUFFER_PADDING_SIZE);
      memcpy(&buffer[0], incoming.data(), incoming.size());
      more = socket.get(zmq::sockopt::rcvmore) > 0;
      /* std::cout << "Attempt to parse " << incoming.size() << " bytes" */
      /*           << std::endl; */
      int in_len = incoming.size();
      while (in_len) {
        int result = av_parser_parse2(
            this->parser, this->dec_ctx, &pkt->data, &pkt->size,
            static_cast<std::uint8_t *>(&buffer[0]), incoming.size(),
            AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        in_len -= result;
        if (result < 0) {
          std::cerr << "Parsing packet failed" << std::endl;
        } else {
          if (pkt->size > 0) {
            /* std::cout << "Decoding packet of size " << pkt->size <<
             * std::endl; */
            result = this->decode(dec_ctx, frame, pkt);
            if (result < 0) {
              std::cerr << "Could not decode: " << avutils::av_strerror2(result)
                        << std::endl;
            } else {
              using namespace std::chrono;
              std::cout << std::fixed << std::setprecision(2)
                        << duration_cast<milliseconds>(
                               system_clock::now().time_since_epoch())
                                   .count() /
                               1000.0
                        << std::endl;
              /* std::cout << "Decoded" << std::endl; */
              ++successes;
              std::vector<int> sizes = {frame->height, frame->width};
              std::vector<size_t> steps{
                  static_cast<size_t>(frame->linesize[0])};

              const auto height = frame->height;
              const auto width = frame->width;
              const auto actual_size = cv::Size(width, height);
              void *y = frame->data[0];
              void *u = frame->data[1];
              void *v = frame->data[2];

              cv::Mat y_mat(height, width, CV_8UC1, y, frame->linesize[0]);
              cv::Mat u_mat(height / 2, width / 2, CV_8UC1, u,
                            frame->linesize[1]);
              cv::Mat v_mat(height / 2, width / 2, CV_8UC1, v,
                            frame->linesize[2]);
              cv::Mat u_resized, v_resized;
              cv::resize(u_mat, u_resized, actual_size, 0, 0,
                         cv::INTER_NEAREST); // repeat u values 4 times
              cv::resize(v_mat, v_resized, actual_size, 0, 0,
                         cv::INTER_NEAREST); // repeat v values 4 times

              cv::Mat yuv;

              std::vector<cv::Mat> yuv_channels = {y_mat, u_resized, v_resized};
              cv::merge(yuv_channels, yuv);

              cv::Mat image;
              cv::cvtColor(yuv, image, cv::COLOR_YUV2BGR);

              /* cv::Mat image(sizes, type, frame->data[0], &steps[0]); */
              /* cv::Mat image_converted; */
              /* cv::cvtColor(image, image_converted, cv::COLOR_YUV2BGR); */
              cv::imshow("decoded", image);
              cv::waitKey(20);
            }
          }
        }
      }
    }
  }
  av_frame_free(&frame);
  av_packet_free(&pkt);
  /* std::cout << "Received " << num_pkts << std::endl; */
}

AVReceiver::~AVReceiver() {
  socket.close();
  av_parser_close(parser);
  avcodec_free_context(&dec_ctx);
  std::cout << "Decoded " << successes << " frames." << std::endl;
  std::cout << "Received " << bytes_received / KB << " KB" << std::endl;
}
