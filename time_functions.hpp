#ifndef TIMEUTILS_HPP_SQ9GVNJU
#define TIMEUTILS_HPP_SQ9GVNJU
#include <chrono>
#include <iomanip>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <time.h>

using namespace std::chrono;

static double current_millis() {
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
             .count() /
         1000.0;
}

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

static std::string format_timepoint_iso8601(const system_clock::time_point &t,
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

void stamp_image(cv::Mat &image,
                 system_clock::time_point t = system_clock::now(),
                 float ypos = 0.2) {
  auto stamp = format_timepoint_iso8601(t);
  cv::putText(image, stamp, cv::Point(10, ypos * image.rows),
              cv::FONT_HERSHEY_SIMPLEX, 2, cv::Scalar(255, 255, 255), 2);
}

#endif /* end of include guard: TIMEUTILS_HPP_SQ9GVNJU */
