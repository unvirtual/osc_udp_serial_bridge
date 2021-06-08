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

enum SerialState {
    CONNECTED,
    DISCONNECTED
};

SerialState serial_state = DISCONNECTED;

void handle_serial_device(Bridge* bridge, const boost::system::error_code& ec) {
    libusbp::device device = libusbp::find_device_with_vid_pid(vendor_id, product_id);

    if (!device && serial_state == CONNECTED) {
        bridge->disconnect_serial();
        serial_state = DISCONNECTED;
    } else if(device && serial_state == DISCONNECTED) {
        libusbp::serial_port port(device, interface_number, composite);
        std::string port_name = port.get_name();
        bridge->open_serial_port(port_name);
        bridge->run_serial();
        serial_state = CONNECTED;
    }
    serial_device_check_timer.expires_at(serial_device_check_timer.expires_at() + interval);
    serial_device_check_timer.async_wait(boost::bind(handle_serial_device, bridge, boost::asio::placeholders::error));
}


int main() {
    try {
        Bridge bridge(io_context, 8888, 8889);
        serial_device_check_timer.async_wait(boost::bind(handle_serial_device, &bridge, boost::asio::placeholders::error));
        bridge.run_udp();

        io_context.run();
    } catch (std::exception &e) {
        std::cerr << "Error in IOContext " << e.what() << std::endl;
    }

    return 0;
}
