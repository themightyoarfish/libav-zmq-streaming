#include <chrono>
#include <iostream>
#include <opencv2/core.hpp>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <zmq.hpp>

using namespace std;
constexpr int KB = 1024;

class AVTransmitter {

  zmq::socket_t socket;
  zmq::context_t ctx;

public:
  AVTransmitter(const string &host, const unsigned int port) {
    socket = zmq::socket_t(ctx, zmq::socket_type::pub);
    const auto bind_str = string("tcp://") + host + ":" + to_string(port);
    socket.bind(bind_str);
    std::cout << "Bound socket to " << bind_str << std::endl;
  }

  static int custom_io_write(void *opaque, uint8_t *buffer,
                             int32_t buffer_size) {

    AVTransmitter *self = static_cast<AVTransmitter *>(opaque);
    self->custom_io_write(buffer, buffer_size);
  }

  int custom_io_write(uint8_t *buffer, int32_t buffer_size) {
    zmq::message_t msg(buffer_size);
    memcpy(msg.data(), buffer, buffer_size);
    socket.send(zmq::message_t("packet"), zmq::send_flags::sndmore);
    socket.send(msg, zmq::send_flags::sndmore);
    return 0;
  }

  void frame_ended() { socket.send(zmq::message_t()); }

  AVIOContext *getContext() {
    /* int ret = avio_open2(&fctx->pb, output, AVIO_FLAG_WRITE, nullptr,
     * nullptr); */
    constexpr int avio_buffer_size = 4 * KB;

    unsigned char *avio_buffer =
        static_cast<unsigned char *>(av_malloc(avio_buffer_size));

    AVIOContext *custom_io =
        avio_alloc_context(avio_buffer, avio_buffer_size, 1, this, nullptr,
                           &AVTransmitter::custom_io_write, nullptr);

    if (!custom_io) {
      std::cout << "Could not open output IO context!" << std::endl;
      return nullptr;
    }
    return custom_io;
  }
};

void initialize_avformat_context(AVFormatContext *&fctx,
                                 const char *format_name) {
  int ret =
      avformat_alloc_output_context2(&fctx, nullptr, format_name, nullptr);
  if (ret < 0) {
    std::cout << "Could not allocate output format context!" << std::endl;
    exit(1);
  }
}

void initialize_io_context(AVFormatContext *&fctx, const char *output) {
  if (!(fctx->oformat->flags & AVFMT_NOFILE)) {
    /* int ret = avio_open2(&fctx->pb, output, AVIO_FLAG_WRITE, nullptr,
     * nullptr); */
    int avio_buffer_size = 4 * KB;

    unsigned char *avio_buffer =
        static_cast<unsigned char *>(av_malloc(avio_buffer_size));

    AVIOContext *custom_io =
        avio_alloc_context(avio_buffer, avio_buffer_size, 1, &transmitter,
                           nullptr, &AVTransmitter::custom_io_write, nullptr);

    if (!custom_io) {
      std::cout << "Could not open output IO context!" << std::endl;
      exit(1);
    }

    fctx->pb = custom_io;
  }
}

void set_codec_params(AVFormatContext *&fctx, AVCodecContext *&codec_ctx,
                      double width, double height, int fps) {
  const AVRational dst_fps = {fps, 1};

  codec_ctx->codec_tag = 0;
  codec_ctx->codec_id = AV_CODEC_ID_H264;
  codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
  codec_ctx->width = width;
  codec_ctx->height = height;
  codec_ctx->gop_size = 12;
  codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  codec_ctx->framerate = dst_fps;
  codec_ctx->time_base = av_inv_q(dst_fps);
  if (fctx->oformat->flags & AVFMT_GLOBALHEADER) {
    codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }
}

void initialize_codec_stream(AVStream *&stream, AVCodecContext *&codec_ctx,
                             AVCodec *&codec) {
  int ret = avcodec_parameters_from_context(stream->codecpar, codec_ctx);
  if (ret < 0) {
    std::cout << "Could not initialize stream codec parameters!" << std::endl;
    exit(1);
  }

  AVDictionary *codec_options = nullptr;
  av_dict_set(&codec_options, "profile", "high", 0);
  av_dict_set(&codec_options, "preset", "ultrafast", 0);
  av_dict_set(&codec_options, "tune", "zerolatency", 0);

  // open video encoder
  ret = avcodec_open2(codec_ctx, codec, &codec_options);
  if (ret < 0) {
    std::cout << "Could not open video encoder!" << std::endl;
    exit(1);
  }
}

SwsContext *initialize_sample_scaler(AVCodecContext *codec_ctx, double width,
                                     double height) {
  SwsContext *swsctx = sws_getContext(width, height, AV_PIX_FMT_BGR24, width,
                                      height, codec_ctx->pix_fmt, SWS_BICUBIC,
                                      nullptr, nullptr, nullptr);
  if (!swsctx) {
    std::cout << "Could not initialize sample scaler!" << std::endl;
    exit(1);
  }

  return swsctx;
}

AVFrame *allocate_frame_buffer(AVCodecContext *codec_ctx, double width,
                               double height) {
  AVFrame *frame = av_frame_alloc();

  std::vector<uint8_t> framebuf(
      av_image_get_buffer_size(codec_ctx->pix_fmt, width, height, 1));
  av_image_fill_arrays(frame->data, frame->linesize, framebuf.data(),
                       codec_ctx->pix_fmt, width, height, 1);
  frame->width = width;
  frame->height = height;
  frame->format = static_cast<int>(codec_ctx->pix_fmt);

  return frame;
}

AVPacket write_frame(AVCodecContext *codec_ctx, AVFormatContext *fmt_ctx,
                     AVFrame *frame) {
  AVPacket pkt = {0};
  av_init_packet(&pkt);

  int ret = avcodec_send_frame(codec_ctx, frame);
  if (ret < 0) {
    std::cout << "Error sending frame to codec context!" << std::endl;
    exit(1);
  }

  ret = avcodec_receive_packet(codec_ctx, &pkt);
  if (ret < 0) {
    std::cout << "Error receiving packet from codec context!" << std::endl;
    exit(1);
  }

  av_interleaved_write_frame(fmt_ctx, &pkt);
  /* av_packet_unref(&pkt); */
  return pkt;
}

void stream_video(double width, double height, int fps) {
  av_register_all();
  avformat_network_init();

  const char *output = "file.mkv";
  int ret;
  std::vector<uint8_t> imgbuf(height * width * 3 + 16);
  cv::Mat image(height, width, CV_8UC3, imgbuf.data(), width * 3);
  AVFormatContext *ofmt_ctx = nullptr;
  AVCodec *out_codec = nullptr;
  AVStream *out_stream = nullptr;
  AVCodecContext *out_codec_ctx = nullptr;
  AVTransmitter transmitter("*", 15001);

  initialize_avformat_context(ofmt_ctx, "h264");
  ofmt_ctx->pb = transmitter.getContext();

  out_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
  out_stream = avformat_new_stream(ofmt_ctx, out_codec);
  out_codec_ctx = avcodec_alloc_context3(out_codec);

  set_codec_params(ofmt_ctx, out_codec_ctx, width, height, fps);
  initialize_codec_stream(out_stream, out_codec_ctx, out_codec);

  out_stream->codecpar->extradata = out_codec_ctx->extradata;
  out_stream->codecpar->extradata_size = out_codec_ctx->extradata_size;

  av_dump_format(ofmt_ctx, 0, output, 1);

  auto *swsctx = initialize_sample_scaler(out_codec_ctx, width, height);
  auto *frame = allocate_frame_buffer(out_codec_ctx, width, height);

  int cur_size;
  uint8_t *cur_ptr;

  ret = avformat_write_header(ofmt_ctx, nullptr);
  if (ret < 0) {
    std::cout << "Could not write header!" << std::endl;
    exit(1);
  }

  const int stride[] = {static_cast<int>(image.step[0])};

  int ms = 0;

  constexpr int n_frames = 100;
  for (int i = 0; i < n_frames; ++i) {

    cv::randu(image, 0, 255);
    sws_scale(swsctx, &image.data, stride, 0, image.rows, frame->data,
              frame->linesize);
    frame->pts +=
        av_rescale_q(1, out_codec_ctx->time_base, out_stream->time_base);
    auto tic = std::chrono::system_clock::now();
    AVPacket pkt = write_frame(out_codec_ctx, ofmt_ctx, frame);
    std::cout << "wrote frame" << std::endl;
    auto toc = std::chrono::system_clock::now();
    ms += std::chrono::duration_cast<std::chrono::milliseconds>(toc - tic)
              .count();
    av_packet_unref(&pkt);
  }
  std::cout << "Avg encoding time " << ms * 1.0 / n_frames << std::endl;

  av_write_trailer(ofmt_ctx);

  av_frame_free(&frame);
  avcodec_close(out_codec_ctx);
  avio_close(ofmt_ctx->pb);
  avformat_free_context(ofmt_ctx);
}

int main() {
  /* av_log_set_level(AV_LOG_DEBUG); */
  double width = 1280, height = 720;
  int fps = 30;

  stream_video(width, height, fps);

  return 0;
}
