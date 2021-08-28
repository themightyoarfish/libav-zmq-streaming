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

  AVTransmitter transmitter("*", 15001, 15);

  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <directory> <format>" << std::endl;
    std::exit(1);
  }
  const string directory = string(argv[1]) + "/";
  const string ext = argv[2];
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
  }
  return 0;
}
