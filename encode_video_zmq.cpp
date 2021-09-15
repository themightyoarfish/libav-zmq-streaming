#include "avreceiver.hpp"
#include "avtransmitter.hpp"
#include "avutils.hpp"
#include <chrono>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <thread>
#include <time.h>
#include <vector>

using namespace std::chrono;

static int _get_millis_from_tp(const system_clock::time_point &t) {
  auto duration_ms = duration_cast<milliseconds>(t.time_since_epoch());
  return duration_ms.count() % 1000;
}

std::tm _tm_from_tp(const system_clock::time_point &t) {
  std::tm calendar_time_utc{};
  const std::time_t as_time_t = system_clock::to_time_t(t);
  auto return_value = gmtime_r(&as_time_t, &calendar_time_utc);
  if (!return_value) {
    throw std::runtime_error(
        "Could not convert TimePoint to UTC calendar time.");
  } else {
    return *return_value;
  }
}
std::string format_timepoint_iso8601(const system_clock::time_point &t,
                                     bool add_zone = true,
                                     bool add_millis = true) {
  const std::tm tm = _tm_from_tp(t);

  std::stringstream ss;
  ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
  if (add_millis) {
    ss << "." << std::setw(3) << std::setfill('0') << _get_millis_from_tp(t);
  }
  if (add_zone) {
    ss << "Z";
  }
  return ss.str();
}

using namespace std;

int main(int argc, char *argv[]) {
  /* av_log_set_level(AV_LOG_DEBUG); */
  std::cout << "Libav version: " << av_version_info() << std::endl;

  std::string directory;
  std::string ext;
  std::string rtp_rcv_host;
  unsigned int rtp_rcv_port;

  if (argc > 3) {
    directory = argv[1];
    ext = argv[2];
    rtp_rcv_host = argv[3];
    rtp_rcv_port = std::atoi(argv[4]);
  } else {
    std::cout << "Usage: " << argv[0] << " <directory> <ext> <host> <port>"
              << std::endl;
    return 1;
  }
  constexpr int fps = 10;
  AVTransmitter transmitter(rtp_rcv_host, rtp_rcv_port, fps);

  const string glob_expr = directory + "*." + ext;
  std::cout << "Globbing: " << glob_expr << std::endl;
  vector<string> filenames;
  cv::glob(glob_expr, filenames);
  std::cout << "Found " << filenames.size() << " images" << std::endl;
  sort(filenames.begin(), filenames.end());

  const int n_frames = filenames.size();
  int ms = 0;
  for (int i = 0; i < n_frames; ++i) {

    cv::Mat image = cv::imread(filenames[i]);
    auto tic = chrono::system_clock::now();
    auto stamp = format_timepoint_iso8601(tic);
    cv::putText(image, stamp, cv::Point(10, 20), cv::FONT_HERSHEY_SIMPLEX, 1,
                cv::Scalar(0, 0, 255), 2);
    std::cout << "Begin encode at " << std::setprecision(5) << std::fixed
              << duration_cast<milliseconds>(
                     system_clock::now().time_since_epoch())
                         .count() /
                     1000.0
              << std::endl;
    transmitter.encode_frame(image);
    auto toc = chrono::system_clock::now();
    ms += chrono::duration_cast<chrono::milliseconds>(toc - tic).count();
    /* cout << "fps = " << 1 / ((ms / 1000.0) / (i + 1)) << endl; */
    std::this_thread::sleep_for(milliseconds(static_cast<int>(1000.0 / fps)));
  }
  return 0;
}
