#include "avreceiver.hpp"
#include "avtransmitter.hpp"
#include "avutils.hpp"
#include "time_functions.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <thread>
#include <vector>

using namespace std;

int main(int argc, char *argv[]) {
  /* av_log_set_level(AV_LOG_TRACE); */

  std::cout << "Libav version: " << av_version_info() << std::endl;

  std::string directory;
  std::string ext;
  std::string rtp_rcv_host;
  unsigned int rtp_rcv_port;
  bool loop;

  if (argc > 4) {
    directory = argv[1];
    ext = argv[2];
    rtp_rcv_host = argv[3];
    rtp_rcv_port = std::atoi(argv[4]);
    loop = std::string(argv[5]) == std::string("true");
  } else {
    std::cout << "Usage: " << argv[0]
              << " <directory> <ext> <host> <port> <true/false>" << std::endl;
    return 1;
  }
  constexpr int fps = 30;
  constexpr int budget_ms = 1000.0 / fps;
  AVTransmitter transmitter(rtp_rcv_host, rtp_rcv_port, fps, 6, 5e6);

  const string glob_expr = directory + "*." + ext;
  std::cout << "Globbing: " << glob_expr << std::endl;
  vector<string> filenames;
  cv::glob(glob_expr, filenames);
  std::cout << "Found " << filenames.size()
            << " images (will read all before encoding)" << std::endl;
  sort(filenames.begin(), filenames.end());
  const int n_frames = filenames.size();

  vector<cv::Mat> images;
  for (int i = 0; i < n_frames; ++i) {
    images.push_back(cv::imread(filenames[i]));
  }

  constexpr bool put_text = true;
  constexpr bool print_timings = true;
  bool has_sdp = false;

  std::cout << std::setprecision(5) << std::fixed;
  int ms = 0;
  const auto begin = chrono::system_clock::now();
  int n_runs = 0;
  for (int i = 0; i < n_frames || loop; ++i) {
    if (i == n_frames) {
      ++n_runs;
      i = 0;
    }
    auto tic = chrono::system_clock::now();
    auto desired_end_time =
        begin + milliseconds(budget_ms * (i + 1 + (n_frames * n_runs)));
    cv::Mat &image = images[i];
    if (put_text) {
      stamp_image(image, tic, 0.1);
    }
    std::cout << "Begin encode at "
              << format_timepoint_iso8601(system_clock::now()) << std::endl;
    transmitter.encode_frame(image);
    std::cout << "Finish encode at "
              << format_timepoint_iso8601(system_clock::now()) << std::endl;
    if (!has_sdp) {
      has_sdp = true;
      std::ofstream ofs("test.sdp");
      ofs << transmitter.get_sdp();
    }
    const auto toc = chrono::system_clock::now();

    const auto remaining = desired_end_time - toc;
    const auto elapsed_encoding =
        chrono::duration_cast<chrono::milliseconds>(toc - tic).count();
    if (remaining.count() > 0) {
      int sleep = duration_cast<milliseconds>(remaining).count();
      if (print_timings) {
        std::cout << "Elapsed for encoding " << elapsed_encoding << std::endl;
        std::cout << "Sleep for " << sleep << std::endl;
      }
      std::this_thread::sleep_for(milliseconds(sleep));
    }
    const auto toc2 = chrono::system_clock::now();
    const auto actually_slept =
        chrono::duration_cast<chrono::milliseconds>(toc2 - toc).count();
    if (print_timings) {
      std::cout << "actually slept " << actually_slept << std::endl;
      const auto elapsed_total =
          chrono::duration_cast<chrono::milliseconds>(toc2 - tic).count();
      ms += elapsed_total;
      cout << "fps = " << (i + 1 + (n_runs * n_frames)) * 1000.0 / ms << endl;
    }
  }
  return 0;
}
