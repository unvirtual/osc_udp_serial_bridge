#include "Bridge.hpp"
#include <libusbp.hpp>

const uint16_t vendor_id = 0x16c0;
const uint16_t product_id = 0x0483;
const uint8_t interface_number = 0;
const bool composite = true;

boost::posix_time::seconds first_interval(0);
boost::posix_time::seconds interval(1);

boost::asio::io_context io_context;
boost::asio::deadline_timer serial_device_check_timer(io_context, first_interval);


void handle_serial_device(Bridge* bridge, const boost::system::error_code& ec) {
    libusbp::device device = libusbp::find_device_with_vid_pid(vendor_id, product_id);

    if (device) {
        libusbp::serial_port port(device, interface_number, composite);
        std::string port_name = port.get_name();
        bridge->open_serial_port(port_name);
        bridge->run_serial();
        std::cout << "Connected serial device " << std::endl;
    } else {
        serial_device_check_timer.expires_at(serial_device_check_timer.expires_at() + interval);
        serial_device_check_timer.async_wait(boost::bind(handle_serial_device, bridge, boost::asio::placeholders::error));
    }
}

int main(int argc, char* argv[]) {
    int udp_input_port = 8888;
    int udp_output_port = 8889;
    std::string serial_port = "";

    if(argc > 1) {
        serial_port = argv[1];
    }

    while(true) {
        try {
            std::cout << "Starting up ..." << std::endl;
            Bridge bridge(io_context, udp_input_port, udp_output_port);
            if(serial_port.size() == 0) {
                serial_device_check_timer.async_wait(boost::bind(handle_serial_device, &bridge, boost::asio::placeholders::error));
            } else {
                std::cout << "opening " << serial_port << std::endl;
                bridge.open_serial_port(serial_port);
                bridge.run_serial();
            }
            bridge.run_udp();

            io_context.run();
            break;
        } catch (SerialConnectionException& e) {
            std::cerr << "Error in Bridge: " << e.what() << std::endl;
            std::cerr << "Trying reconnect ... " << std::endl;
        } catch (std::exception &e) {
            std::cerr << "Exiting: Error in IOContext " << e.what() << std::endl;
            return 0;
        }
    }
    return 0;
}
