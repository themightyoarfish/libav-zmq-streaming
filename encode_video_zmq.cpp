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
#include <vector>

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
    transmitter.encode_frame(image);
    auto toc = chrono::system_clock::now();
    ms += chrono::duration_cast<chrono::milliseconds>(toc - tic).count();
    /* cout << "fps = " << 1 / ((ms / 1000.0) / (i + 1)) << endl; */
    std::this_thread::sleep_for(
        std::chrono::milliseconds(static_cast<int>(1000.0 / fps)));
  }
  return 0;
}
