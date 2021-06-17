#include "Bridge.hpp"
#include <libusbp.hpp>

const uint16_t vendor_id = 0x16c0;
const uint16_t product_id = 0x0483;
const uint8_t interface_number = 0;
const bool composite = true;

boost::posix_time::seconds first_interval(1);
boost::posix_time::seconds interval(1);

boost::asio::io_context io_context;
boost::asio::deadline_timer serial_device_check_timer(io_context, first_interval);

void handle_serial_device(Bridge* bridge, const boost::system::error_code& ec) {
    libusbp::device device = libusbp::find_device_with_vid_pid(vendor_id, product_id);

    if(ec) {
        if(ec.value() == 125)
            return;
        throw "Timer error";
    }
    if (device) {
        libusbp::serial_port port(device, interface_number, composite);
        std::string port_name = port.get_name();
        std::cout << "Device found on serial port " << port_name << std::endl;
        bridge->open_serial_port(port_name);
        bridge->run_serial();
    } else {
        serial_device_check_timer.expires_from_now(interval);
        serial_device_check_timer.async_wait(boost::bind(handle_serial_device, bridge, boost::asio::placeholders::error));
    }
}

std::vector<std::byte> string_to_vector_byte(const std::string& str) {
    std::vector<std::byte> res(str.size());
    std::transform(str.cbegin(), str.cend(), res.begin(), [](char c){ return std::byte(c); });
    return res;
}

int main(int argc, char* argv[]) {
    int udp_input_port = 8888;
    int udp_output_port = 8889;
    std::string serial_port = "";

    if(argc > 1) {
        serial_port = argv[1];
    }
    std::cout << "Starting up ..." << std::endl;

    while(true) {
        try {
            std::cout << "Connecting device ..." << std::endl;
            const std::string status_request_message("/grid/status\0\0\0\0,\0\0\0", 20);
            const std::string status_up_message("/grid/status\0\0\0\0,s\0\0available\0\0\0", 32);
            const std::string status_down_message("/grid/status\0\0\0\0,s\0\0not_available\0\0\0", 36);

            Bridge bridge(io_context, udp_input_port, udp_output_port);
            bridge.set_status_udp_request_message(string_to_vector_byte(status_request_message));
            bridge.set_status_down_udp_response(string_to_vector_byte(status_down_message));
            bridge.set_status_up_udp_response(string_to_vector_byte(status_up_message));

            if(serial_port.size() == 0) {
                std::cout << "Looking for device with vendorID:productID " << vendor_id << ":" << product_id << std::endl; 
                serial_device_check_timer.expires_from_now(interval);
                serial_device_check_timer.async_wait(boost::bind(handle_serial_device, &bridge, boost::asio::placeholders::error));
            } else {
                bridge.open_serial_port(serial_port);
                bridge.run_serial();
            }
            bridge.run_udp();

            io_context.run();
            break;
        } catch (SerialConnectionException& e) {
            std::cerr << " Serial disconnected ... " << std::endl;
        } catch (std::exception &e) {
            std::cerr << "Exiting: Error in IOContext " << e.what() << std::endl;
            return 0;
        }
    }
    return 0;
}
