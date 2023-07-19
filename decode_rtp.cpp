#include <atomic>
#include <boost/thread/sync_bounded_queue.hpp>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <opencv2/highgui.hpp>
#include <thread>

#include "avutils.hpp"
#include "rtp_receiver.hpp"
#include "time_functions.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
}

using namespace std::chrono;


int main(int argc, char** argv) {
  /* av_log_set_level(AV_LOG_TRACE); */
  auto window_name = argc > 1 ? argv[1] : "test.sdp";
  RTPReceiver receiver(argc > 1 ? argv[1] : "test.sdp");
  cv::namedWindow(window_name, cv::WINDOW_GUI_NORMAL);
  while (true) {
    cv::Mat image = receiver.get();
    if (!image.empty()) {
      auto image_displayed = system_clock::now();
      /* stamp_image(image, image_displayed, 0.8); */
      std::cout << "Image display started: "
                << format_timepoint_iso8601(image_displayed) << std::endl;
      cv::imshow(window_name, image);
      std::cout << "Image displayed: "
                << format_timepoint_iso8601(system_clock::now()) << std::endl;
      cv::waitKey(1);
    }
  }
}
