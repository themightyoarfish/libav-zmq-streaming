#include <Poco/JWT/Signer.h>
#include <Poco/JWT/Token.h>
#include <arpa/inet.h>
#include <atomic>
#include <boost/thread/sync_bounded_queue.hpp>
#include <chrono>
#include <csignal>
#include <future>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <rtc/peerconnection.hpp>
#include <rtc/rtp.hpp>
#include <rtc/websocket.hpp>
#include <sys/socket.h>
#include <sys/types.h>
#include <tclap/CmdLine.h>
#include <thread>
#include <zmq.hpp>

#include "avtransmitter.hpp"
#include "time_functions.hpp"


using namespace std;
using namespace nlohmann;
using namespace std::chrono;
using namespace TCLAP;

void connect_socket(zmq::socket_t& socket,
                    const string& host,
                    int port,
                    const string& user,
                    const string& pw,
                    const string& topic);
int get_free_port();

struct SenderConfig {
  string topic;
  int udp_port;
  string udp_hostname;
  string mediaserver_host;
  int mediaserver_port;
  uint16_t begin_port = rtc::Configuration().portRangeBegin;
  uint16_t end_port   = rtc::Configuration().portRangeEnd;
  string pw;
};
constexpr int SSRC = 42;
using SOCKET       = int;

using PortRange = pair<uint16_t, uint16_t>;

namespace auth {

static Poco::JWT::Token get_dummy_token(const std::string& topic) {
  Poco::JWT::Token t;
  t.setType("JWT");
  t.setIssuer("psiori");
  t.setSubject("psiori-streaming");
  t.payload().set("topic", topic);
  return t;
}
static std::string
get_signed_serialized_token(const std::string& key, const std::string& topic) {
  Poco::JWT::Signer signer(key);
  auto token = get_dummy_token(topic);
  return signer.sign(token, Poco::JWT::Signer::ALGO_HS256);
}

}  // namespace auth

class Sender {
  shared_ptr<rtc::PeerConnection> pc_;
  shared_ptr<rtc::Track> video_track_;

  const string hostname_;  // hostname to bind to for receiving rtp packets

  int udp_listen_port_;  // port to expect udp rtp on

  string topic_;
  std::shared_ptr<rtc::WebSocket> ws_;
  string mediaserver_host_;
  int mediaserver_port_;
  PortRange port_range_;

public:
  ~Sender() {
    std::cout << *this << "destroying" << std::endl;
  }
  friend ostream& operator<<(ostream& os, const Sender& s) {
    os << "Sender on topic " << s.topic_ << " with rtp udp port "
       << s.udp_listen_port_ << ": ";
    return os;
  }

  void checkOpen() {
    std::cout << *this << "track open? " << video_track_->isOpen() << std::endl;
  }

  Sender(const Sender& other) = delete;
  Sender(Sender&& other)      = delete;

  Sender(const SenderConfig& sender_cfg) :
      hostname_(sender_cfg.udp_hostname),
      udp_listen_port_(sender_cfg.udp_port),
      topic_(sender_cfg.topic),
      mediaserver_host_(sender_cfg.mediaserver_host),
      mediaserver_port_(sender_cfg.mediaserver_port),
      port_range_(PortRange(sender_cfg.begin_port, sender_cfg.end_port)) {
    const auto jwt_token =
        auth::get_signed_serialized_token(sender_cfg.pw, sender_cfg.topic);
    ws_ = std::make_shared<rtc::WebSocket>();
    std::promise<void> wsPromise;

    ws_->onOpen([this, &wsPromise]() { wsPromise.set_value(); });

    const string url = "ws://" + mediaserver_host_ + ":" +
                       std::to_string(mediaserver_port_) + "/register_client";
    std::cout << *this << "opening websocket to " << url << std::endl;
    ws_->open(url);

    std::cout << *this << "Waiting for signaling to be connected..."
              << std::endl;
    wsPromise.get_future().wait();
    std::cout << *this << "Signaling ready" << std::endl;

    ws_->onMessage([this](auto data) {
      if (!std::holds_alternative<std::string>(data)) {
        return;
      }

      const json message = json::parse(std::get<std::string>(data));

      if (message.find("status") != message.end() &&
          message["status"].get<std::string>() != "ok") {
        std::cout << *this << "Got error status "
                  << message["status"].get<std::string>() << std::endl;
        return;
      }

      auto it = message.find("type");
      if (it == message.end()) {
        return;
      }

      rtc::Configuration config;
      config.portRangeBegin = port_range_.first;
      config.portRangeEnd   = port_range_.second;
      this->pc_             = std::make_shared<rtc::PeerConnection>(config);

      this->pc_->onGatheringStateChange(
          [this](rtc::PeerConnection::GatheringState state) {
            std::cout << "client Gathering State: " << state << std::endl;
            if (state == rtc::PeerConnection::GatheringState::Complete) {
              auto description = this->pc_->localDescription();
              if (!description) {
                std::cout << *this << "No Description" << std::endl;
              } else {
                json message = {{"method", "answer"},
                                {"topic", this->topic_},
                                {"direction", "send"},
                                {"sdp",
                                 {{"sdp", std::string(*description)},
                                  {"type", description->typeString()}}}};

                this->ws_->send(message.dump());
              }
            }
          });

      pc_->onStateChange([this](rtc::PeerConnection::State state) {
        std::cout << *this << "State: " << state << std::endl;
        if (state == rtc::PeerConnection::State::Connected) {
          std::cout << *this << "connected." << std::endl;
        }
      });

      pc_->onTrack([this](shared_ptr<rtc::Track> track) {
        std::cout << *this << "onTrack()" << std::endl;
        rtc::Description::Media media = track->description();
        media.addSSRC(SSRC, "video-send");
        track->setDescription(media);
        this->video_track_ = track;
        std::cout << *this << "added track" << std::endl;
        this->video_track_->onClosed([this]() {
          std::cout << *this << "video track closed" << std::endl;
        });
        this->video_track_->onError([this](string error) {
          std::cout << *this << "video track error: " << error << std::endl;
        });

        this->video_track_->onOpen([this]() {
          std::cout << *this << "track now open!" << std::endl;

          SOCKET sock;
          int err;
          struct addrinfo hints = {}, *addrs;
          char port_str[16]     = {};

          hints.ai_family   = AF_INET;
          hints.ai_socktype = SOCK_DGRAM;
          hints.ai_protocol = 0;

          sprintf(port_str, "%d", udp_listen_port_);

          err = getaddrinfo(this->hostname_.c_str(), port_str, &hints, &addrs);
          if (err != 0) {
            std::cout << *this << "Could not resolve " << hostname_ << ": "
                      << gai_strerror(err) << std::endl;
            throw std::runtime_error("Unable to resolve " + hostname_);
          }

          for (struct addrinfo* addr = addrs; addr != NULL;
               addr                  = addr->ai_next) {
            sock =
                socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
            if (sock == -1) {
              err = errno;
              break;
            }

            std::cout << *this << "Binding to " << this->hostname_ << ":"
                      << udp_listen_port_ << std::endl;

            if (::bind(sock, addr->ai_addr, addr->ai_addrlen) == 0) {
              break;  // BOUND, leave loop
            }

            err = errno;

            close(sock);
            sock = -1;
          }

          freeaddrinfo(addrs);

          if (sock == -1) {
            throw std::runtime_error("Failed to bind UDP socket on " +
                                     this->hostname_ + ":" +
                                     to_string(udp_listen_port_));
            abort();
          }

          constexpr int rcvBufSize = 212992;
          setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
                     reinterpret_cast<const char*>(&rcvBufSize),
                     sizeof(rcvBufSize));

          constexpr int BUFFER_SIZE = 2048;
          char buffer[BUFFER_SIZE];
          int len;
          while ((len = recv(sock, buffer, BUFFER_SIZE, 0)) >= 0) {
            if (len < sizeof(rtc::RtpHeader) || !this->video_track_->isOpen()) {
              std::cout
                  << *this
                  << "Packet smaller than header, or track not open anymore."
                  << std::endl;
              continue;
            }

            auto rtp = reinterpret_cast<rtc::RtpHeader*>(buffer);
            rtp->setSsrc(SSRC);

            try {
              std::cout << *this << "Sending rtp data to track" << std::endl;
              this->video_track_->send(
                  reinterpret_cast<const std::byte*>(buffer), len);
            } catch (const std::runtime_error& re) {
              std::cerr << *this
                        << "Runtime error in sender->onOpen: " << re.what()
                        << std::endl;
            } catch (const std::exception& ex) {
              std::cerr << *this
                        << "Error occurred in sender->onOpen: " << ex.what()
                        << std::endl;
            } catch (...) {
              // catch any other errors (that we have no information about)
              std::cerr << *this
                        << "Unknown failure occurred in sender->onOpen."
                        << std::endl;
            }
          }
          std::cerr << *this << "left loop due to empty packet." << std::endl
                    << std::flush;
          abort();
        });
      });

      auto type = it->get<std::string>();
      auto sdp  = message["sdp"].get<std::string>();
      rtc::Description remote_offer(sdp, rtc::Description::Type::Offer);
      this->pc_->setRemoteDescription(remote_offer);
    });

    json announcement = {{"topic", sender_cfg.topic},
                         {"token", jwt_token},
                         {"method", "announce"},
                         {"direction", "send"}};
    ws_->send(announcement.dump());
  }
};


class VideoStreamMonitor {
private:
  AVTransmitter transmitter;
  std::thread encoder;
  std::atomic<bool> stop;
  typedef boost::sync_bounded_queue<cv::Mat> Queue;
  Queue queue;


  bool has_printed_sdp;

public:
  VideoStreamMonitor(const string& host,
                     unsigned int port,
                     unsigned int fps,
                     unsigned int bitrate);

  ~VideoStreamMonitor();

  void process(cv::Mat data);
};

VideoStreamMonitor::VideoStreamMonitor(const string& host,
                                       unsigned int port,
                                       unsigned int fps,
                                       unsigned int bitrate) :
    transmitter(host, port, fps, 1, bitrate), queue(1), has_printed_sdp(false) {
  stop.store(false);
  av_log_set_level(AV_LOG_QUIET);
  encoder = std::thread([&]() {
    while (!stop.load()) {
      // const auto queue_status = queue.try_pull_front(image);
      //  keep the zoom_factor value in range [0.2, 1.0]
      cv::Mat image = queue.pull_front();
      // if (queue_status == boost::concurrent::queue_op_status::empty) {
      //   std::this_thread::sleep_for(std::chrono::milliseconds(5));
      //   continue;
      // }
      auto tic = current_millis();
      transmitter.encode_frame(image);

      if (!has_printed_sdp) {
        has_printed_sdp = true;
        string sdp      = transmitter.get_sdp();
        size_t index    = 0;
        std::cout << "SDP file is: \r\n" << sdp << std::endl;
        std::cout << "------------\r\nSDP file ended" << std::endl;
      }
    }
  });
}

VideoStreamMonitor::~VideoStreamMonitor() {
  stop.store(true);
  encoder.join();
  queue.close();
}

void VideoStreamMonitor::process(cv::Mat data) {
  if (!queue.full()) {
    queue.push_back(data);
  }
}

int main(int argc, char* argv[]) {
  CmdLine cmdline("Recieve zmq (from KAI-core), send via RTP");

  ValueArg<string> zmq_host("H", "zmq-host", "host of incoming zmq messages", false,
                            "127.0.0.1", "Host as String", cmdline);

  ValueArg<int> zmq_port("", "zmq-port", "port of incoming zmq messages", true,
                         0, "Port as Integer", cmdline);
  ValueArg<string> zmq_user("u", "zmq-user", "user for zmq authentication", true,
                            "", "user as string", cmdline);
  ValueArg<string> zmq_pw("", "zmq-password", "password for zmq authentication",
                          false, "psiori", "password as string", cmdline);
  ValueArg<string> mediaserver_pw(
      "", "mediaserver-password", "password for mediaserver authentication",
      false, "secretpassword", "password as string", cmdline);
  ValueArg<string> topic("t", "topic", "topic of the stream", true,
                         "", "topic as string", cmdline);

  ValueArg<string> mediaserver_host("m", "mediaserver", "mediaserver IP", false,
                                    "127.0.0.1", "Ip address", cmdline);
  ValueArg<int> mediaserver_ws_port(
      "", "stream-port", "WebSocket port on the mediaserver for setup", false,
      8080, "Port", cmdline);
  ValueArg<int> rtp_fps(
      "f", "fps", "fps of stream", false, 20, "fps as Integer", cmdline);
  ValueArg<long> rtp_bitrate("b", "bitrate", "bitrate of stream", false, 4e6,
                             "Bitrate as Integer", cmdline);
  SwitchArg bgr("", "bgr", "switch channels before sending", cmdline, true);
  cmdline.parse(argc, argv);

  zmq::context_t ctx(1);
  zmq::socket_t socket(ctx, ZMQ_SUB);


  connect_socket(socket, zmq_host.getValue(), zmq_port.getValue(),
                 zmq_user.getValue(), zmq_pw.getValue(), "");

  const int rtp_port = get_free_port();

  SenderConfig cfg{.topic            = topic.getValue(),
                   .udp_port         = rtp_port,
                   .udp_hostname     = "127.0.0.1",
                   .mediaserver_host = mediaserver_host.getValue(),
                   .mediaserver_port = mediaserver_ws_port.getValue(),
                   .begin_port       = rtc::Configuration().portRangeBegin,
                   .end_port         = rtc::Configuration().portRangeEnd,
                   .pw               = mediaserver_pw.getValue()};

  Sender rtp2webrtc_bridge(cfg);


  VideoStreamMonitor streamer(
      "127.0.0.1", rtp_port, rtp_fps.getValue(), rtp_bitrate.getValue());

  zmq::message_t recv_topic;
  zmq::message_t request;
  zmq::recv_result_t result;
  auto cycle_tic = current_millis();
  while (true) {
    result = socket.recv(recv_topic, zmq::recv_flags::none);
    std::cout << "Got topic, waiting for zmq result" << std::endl;
    result                = socket.recv(request, zmq::recv_flags::none);
    const string topicStr = recv_topic.to_string();
    std::cout << "Received zmq message on topic " << topicStr << std::endl;
    if (topicStr.find("camera|") != string::npos) {
      int64_t more = socket.get(zmq::sockopt::rcvmore);
      uchar* ptr   = reinterpret_cast<uchar*>(request.data());
      std::vector<uchar> data(ptr, ptr + request.size());
      auto tic      = current_millis();
      cv::Mat image = cv::imdecode(data, -1);
      std::cout << "cv::imdecode Took " << 1000 * (current_millis() - tic)
                << std::endl;
      if (bgr.getValue()) {
        cv::cvtColor(image, image, cv::COLOR_BGR2RGB);
      }
      streamer.process(image);
      if (more == 1) {
        // ROI data, but we dont need them, but they are still received here so
        // they dont show up in the next cycle...
        socket.recv(request, zmq::recv_flags::none);
      }
    } else {
      std::cout << "Skipped non-camera message" << std::endl;
    }
    std::cout << "Cycle-Frequency: " << 1 / (current_millis() - cycle_tic)
              << " Hz" << std::endl;
    cycle_tic = current_millis();
  }
}

void connect_socket(zmq::socket_t& socket,
                    const string& host,
                    int port,
                    const string& user,
                    const string& pw,
                    const string& topic) {
  socket.set(zmq::sockopt::plain_username, user);
  socket.set(zmq::sockopt::plain_password, pw);
  socket.set(zmq::sockopt::rcvhwm, 2);
  socket.set(zmq::sockopt::subscribe, topic);
  const auto connect_str = string("tcp://") + host + ":" + std::to_string(port);
  socket.connect(connect_str);
  std::cout << "Connected zmq socket to " << connect_str
            << ", on topic: " << topic << std::endl;
}

int get_free_port() {
  return 6000;
}
