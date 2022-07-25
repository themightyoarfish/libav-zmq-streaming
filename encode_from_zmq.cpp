
#include <boost/thread/sync_bounded_queue.hpp>

#include "avtransmitter.hpp"

class VideoStreamMonitor {
private:
  AVTransmitter* transmitter;
  Timer timer;
  std::thread encoder;
  std::atomic<bool> stop;
  typedef boost::sync_bounded_queue<SensorData*> Queue;
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
  VideoStreamMonitor(Agent* agent,
                     const std::string& host,
                     unsigned int port,
                     unsigned int fps,
                     unsigned int bitrate,
                     double output_scale_factor,
                     bool do_zoom,
                     const ConfigNode& config);

  ~VideoStreamMonitor() final;

  void observe(SensorData* data) final;
}

VideoStreamMonitor::VideoStreamMonitor(Agent* agent,
                                       const std::string& host,
                                       unsigned int port,
                                       unsigned int fps,
                                       unsigned int bitrate,
                                       double output_scale_factor,
                                       bool do_zoom,
                                       const ConfigNode& config) :
    SensorObserver(agent, config),
    transmitter(new AVTransmitter(host, port, fps, 1, bitrate)),
    queue(1),
    output_scale_factor(output_scale_factor),
    do_zoom(do_zoom),
    has_printed_sdp(false) {
  stop.store(false);
  av_log_set_level(AV_LOG_QUIET);
  encoder = std::thread([&]() {
    while (!stop.load()) {
      SensorData* data        = nullptr;
      const auto queue_status = queue.try_pull_front(data);
      // keep the zoom_factor value in range [0.2, 1.0]
      double zoom_factor = this->do_zoom == true
                               ? std::min(std::max(0.2, AC_WM.zoom_factor), 1.0)
                               : 1.0;
      if (queue_status == boost::concurrent::queue_op_status::empty) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        continue;
      }
      const RectData* rect_data = dynamic_cast<const RectData*>(data);
      cv::Mat image;
      if (rect_data) {
        const CameraData* cam_data = rect_data->asCameraDataWithDetectionBoxes(
            /* scale */ zoom_factor);
        image = cam_data->getImage();
        delete cam_data;
      } else {
        // probably plain camera data
        const CameraData* cam_data = static_cast<const CameraData*>(data);
        image                      = cam_data->getImage();
      }
      if (this->do_zoom) {
        image = utils::zoom(image, zoom_factor, this->output_scale_factor);
      }
      LOG_DEBUG(Logger::SENSOR,
                "VideoStreamMonitor encoding data of size "
                    << image.size << " from topic " << data->getTopic()
                    << " at time "
                    << utils::format_timepoint_iso8601(data->getTime())
                    << ". Took " << timer.ToString());

      timer.tick();
      transmitter->encode_frame(image);
      timer.tock();

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
        LOG_ALWAYS(Logger::SENSOR, "VideoStreamMonitor SDP file is " << sdp);
      }

      data->releaseLock();
    }
  });
}

VideoStreamMonitor::~VideoStreamMonitor() {
  stop.store(true);
  encoder.join();
  queue.close();
  delete transmitter;
}

void VideoStreamMonitor::observe(SensorData* data) {
  if (!queue.full()) {
    data->getLock();
    queue.push_back(data);
  }
}
