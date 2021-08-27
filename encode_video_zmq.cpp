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

void stream_video() {

  AVTransmitter transmitter("*", 15001, 15);

  std::vector<std::string> filenames;
  cv::glob("/Users/rasmus/Downloads/images/*.jpeg", filenames);
  std::sort(filenames.begin(), filenames.end());

  const int n_frames = filenames.size();
  int ms = 0;
  for (int i = 0; i < n_frames; ++i) {

    cv::Mat image = cv::imread(filenames[i]);
    auto tic = std::chrono::system_clock::now();
    transmitter.encode_frame(image);
    auto toc = std::chrono::system_clock::now();
    ms += std::chrono::duration_cast<std::chrono::milliseconds>(toc - tic)
              .count();
    /* std::cout << "fps = " << 1 / ((ms / 1000.0) / (i + 1)) << std::endl; */
  }
}

int main() {
  /* av_log_set_level(AV_LOG_DEBUG); */
  /* av_register_all(); */
  std::cout << "Libav version: " << av_version_info() << std::endl;

  stream_video();

  return 0;
}
