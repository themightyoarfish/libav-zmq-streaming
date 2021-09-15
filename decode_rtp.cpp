#include <chrono>
#include <iomanip>
#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
}

using namespace std::chrono;

int main(int argc, char **argv) {
  AVFormatContext *fmt_ctx = avformat_alloc_context();
  fmt_ctx->flags |= (AVFMT_FLAG_NOBUFFER | AVFMT_FLAG_DISCARD_CORRUPT |
                     AVFMT_FLAG_FLUSH_PACKETS);
  av_opt_set(fmt_ctx, "protocol_whitelist", "file,rtp,udp", 0);
  av_opt_set_int(fmt_ctx, "fpsprobesize", 0, 0);
  av_opt_set_int(fmt_ctx, "probesize", 32, 0);
  av_opt_set_int(fmt_ctx, "analyzeduration", 0, 0);

  auto src_filename = "test.sdp";
  /* open input file, and allocate format context */
  if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
    std::cout << "Could not open source " << src_filename << std::endl;
    exit(1);
  }
  std::cout << "Number of streams: " << fmt_ctx->nb_streams << std::endl;
  if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
    std::cout << "Could not find stream info." << std::endl;
    exit(1);
  } else {

    AVPacket packet;
    av_init_packet(&packet);
    AVCodec *codec = NULL;
    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
      std::cout << "Could not open codec." << std::endl;
    }

    AVCodecContext *dec_ctx = avcodec_alloc_context3(codec);
    av_opt_set(dec_ctx, "avioflags", "direct", 0);
    av_opt_set(fmt_ctx, "sync", "ext", 0);

    if (avcodec_open2(dec_ctx, codec, nullptr) < 0) {
      std::cout << "Could not open context" << std::endl;
      exit(1);
    }
    dec_ctx->delay = 0;
    dec_ctx->thread_count = 1;
    while (av_read_frame(fmt_ctx, &packet) >= 0) {
      std::cout << "Got frame at " << std::setprecision(5) << std::fixed
                << duration_cast<milliseconds>(
                       system_clock::now().time_since_epoch())
                           .count() /
                       1000.0
                << std::endl;
    }
  }
}
