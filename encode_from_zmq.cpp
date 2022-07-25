
#include <boost/thread/sync_bounded_queue.hpp>
#include <chrono>
#include <csignal>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <thread>

#include "avtransmitter.hpp"
#include "time_functions.hpp"

using std::string;
using namespace std::chrono;

class VideoStreamMonitor {
private:
  AVTransmitter* transmitter;
  std::thread encoder;
  std::atomic<bool> stop;
  typedef boost::sync_bounded_queue<cv::Mat*> Queue;
  Queue queue;

  /**
   * @brief factor by which central crop from digital-zoom operation needs to be
   * resized. For value: 0, crop is returned as it is. For val: 1, crop is
   * resized to original frame size.
   */
  double output_scale_factor;

  /**
   * @brief Enable or disable image zooming functionality. If true, images can
   * be zoomed in or out via TUI.
   */
  bool do_zoom;
  bool has_printed_sdp;

public:
  VideoStreamMonitor(const std::string& host,
                     unsigned int port,
                     unsigned int fps,
                     unsigned int bitrate,
                     double output_scale_factor,
                     bool do_zoom);

  ~VideoStreamMonitor();

  void observe(cv::Mat* data);
};

VideoStreamMonitor::VideoStreamMonitor(const std::string& host,
                                       unsigned int port,
                                       unsigned int fps,
                                       unsigned int bitrate,
                                       double output_scale_factor,
                                       bool do_zoom) :
    transmitter(new AVTransmitter(host, port, fps, 1, bitrate)),
    queue(1),
    output_scale_factor(output_scale_factor),
    do_zoom(do_zoom),
    has_printed_sdp(false) {
  stop.store(false);
  av_log_set_level(AV_LOG_QUIET);
  encoder = std::thread([&]() {
    while (!stop.load()) {
      std::cout << "Running while loop in Thread" << std::endl;
      cv::Mat* data           = nullptr;
      const auto queue_status = queue.try_pull_front(data);
      // keep the zoom_factor value in range [0.2, 1.0]
      double zoom_factor =
          this->do_zoom == true ? std::min(std::max(0.2, 0.5), 1.0) : 1.0;
      if (queue_status == boost::concurrent::queue_op_status::empty) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        continue;
      }
      cv::Mat image = cv::Mat::zeros(256, 512, CV_8UC1);

      if (this->do_zoom) {
        cv::resize(image, image,
                   cv::Size(int(image.cols * zoom_factor),
                            int(image.rows * zoom_factor)),
                   cv::INTER_LINEAR);
      }
      std::cout << "Begin encode " << std::setprecision(5) << std::fixed
                << duration_cast<milliseconds>(
                       system_clock::now().time_since_epoch())
                           .count() /
                       1000.0
                << std::endl;
      auto tic = current_millis();
      transmitter->encode_frame(image);
      std::cout << "Took " << 1000 * (current_millis() - tic) << std::endl;
      std::cout << "Encoded at " << std::setprecision(5) << std::fixed
                << duration_cast<milliseconds>(
                       system_clock::now().time_since_epoch())
                           .count() /
                       1000.0
                << std::endl;

      if (!has_printed_sdp) {
        has_printed_sdp = true;
        std::string sdp = transmitter->get_sdp();
        size_t index    = 0;
        while (true) {
          // Locate the substring to replace.
          index = sdp.find("\r\n", index);
          if (index == std::string::npos) {
            break;
          }
          // Make the replacement.
          sdp.replace(index, 2, ";");
          // Advance index forward so the next iteration doesn't pick it up as
          // well.
          index += 2;
        }
      }

      // data->releaseLock();
    }
  });
}

VideoStreamMonitor::~VideoStreamMonitor() {
  stop.store(true);
  encoder.join();
  queue.close();
  delete transmitter;
}

void VideoStreamMonitor::observe(cv::Mat* data) {
  if (!queue.full()) {
    // data->getLock();
    queue.push_back(data);
  }
}

int main(int argc, char* argv[]) {
  std::cout << "Starting main" << std::endl;
  auto streamer =
      new VideoStreamMonitor("localhost", 8000, 20, 100000, 1, false);
  for (int i = 0; i > 5; i++) {
    streamer->observe(new cv::Mat(256, 512, CV_8UC1));
  }
}
