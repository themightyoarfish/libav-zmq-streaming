#include "avreceiver.hpp"
int main(int argc, char *argv[])
{
    AVReceiver receiver("localhost", 15001);
    while (true) {
        receiver.receive();
    }
    return 0;
}
