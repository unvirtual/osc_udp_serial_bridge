#include "Bridge.hpp"

int main() {
    try {
        boost::asio::io_context io_context;
        Bridge bridge(io_context, 8888, 8889, "/dev/ttyACM0");
        io_context.run();
    }
    catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}