#include "avreceiver.hpp"
int main(int argc, char *argv[]) {
  const std::string host = argc > 1 ? argv[1] : "localhost";
  AVReceiver receiver(host, 15001);
  while (true) {
    receiver.receive();
  }
  return 0;
}
