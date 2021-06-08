#include "Bridge.hpp"

Bridge::Bridge(boost::asio::io_context &io_context, int udp_remote_port, int udp_local_port)
    : remote_endpoint_(udp::endpoint(udp::v4(), udp_remote_port)),
      socket_(io_context, udp::endpoint(udp::v4(), udp_local_port)),
      serial_port_{io_context} {
    udp_recv_buffer_.resize(4096);
    serial_recv_buffer_.resize(4096);
}

void
Bridge::open_serial_port(const std::string& serial_port_name) {
    serial_port_.open(serial_port_name);
}

void 
Bridge::disconnect_serial() {
    serial_port_.cancel();
    serial_port_.close();
}

void 
Bridge::run_serial() {
    do_read_serial();
}

void 
Bridge::run_udp() {
    do_read_udp();
}

void
Bridge::do_read_udp() {
    socket_.async_receive_from(
        boost::asio::buffer(udp_recv_buffer_), remote_endpoint_,
        boost::bind(&Bridge::read_udp_complete, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
}

void
Bridge::read_udp_complete(const boost::system::error_code &error, std::size_t bytes_transferred) {
    if (!error) {
        auto encoded = slip::encode(udp_recv_buffer_.cbegin(), udp_recv_buffer_.cbegin() + bytes_transferred);
        udp_recv_buffer_.erase(udp_recv_buffer_.begin(), udp_recv_buffer_.begin() + bytes_transferred);

        do_write_serial(encoded);
    } else {
        std::cerr << "ERROR " << error.value() << " during UDP read: " << error.message() << std::endl;
        // continue reading from UDP even on error
        do_read_udp();
    }
}

void
Bridge::do_write_udp() {
    auto [status, data] = slip_encoded_udp_buffer_.get_next_frame();

    if (status != slip::SLIPInputStream::FrameStatus::VALID) {
        do_read_serial();
        return;
    }

    socket_.async_send_to(boost::asio::buffer(data),
                            remote_endpoint_,
                            boost::bind(&Bridge::write_udp_complete, this,
                                        boost::asio::placeholders::error,
                                        boost::asio::placeholders::bytes_transferred));
}

void
Bridge::write_udp_complete(const boost::system::error_code &error, std::size_t /* bytes_transferred */) {
    if (error) {
        std::cerr << "ERROR " << error.value() << " during UDP write: " << error.message() << std::endl;
    }
    // continue reading from serial even on UDP write error
    do_read_serial();
}

void
Bridge::do_read_serial() {
    serial_port_.async_read_some(boost::asio::buffer(serial_recv_buffer_),
                                    boost::bind(&Bridge::read_serial_complete, this,
                                                boost::asio::placeholders::error,
                                                boost::asio::placeholders::bytes_transferred));
}

void
Bridge::read_serial_complete(const boost::system::error_code &error, std::size_t bytes_transferred) {
    if (!error) {
        std::vector<std::byte> received(serial_recv_buffer_.begin(), serial_recv_buffer_.begin() + bytes_transferred);
        serial_recv_buffer_.erase(serial_recv_buffer_.begin(), serial_recv_buffer_.begin() + bytes_transferred);

        slip_encoded_udp_buffer_.append(received);

        do_write_udp();
    } else {
        std::cerr << "ERROR " << error.value() << " during serial read: " << error.message() << std::endl;
        // Throw if serial error happened to exit IOContext. Caller should handle exception.
        throw SerialConnectionException();
    }
}

void
Bridge::do_write_serial(const std::vector<std::byte>& data) {
    boost::asio::async_write(serial_port_,
                                boost::asio::buffer(data),
                                boost::bind(&Bridge::write_serial_complete, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
}

void
Bridge::write_serial_complete(const boost::system::error_code &error, std::size_t /* bytes_transferred */) {
    if (!error) {
        do_read_udp();
    } else {
        std::cerr << "ERROR " << error.value() << " during serial write: " << error.message() << std::endl;
        // Throw if serial error happened to exit IOContext. Caller should handle exception.
        throw SerialConnectionException();
    }
}