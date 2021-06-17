#include "Bridge.hpp"

Bridge::Bridge(boost::asio::io_context &io_context, int udp_remote_port, int udp_local_port)
    : remote_endpoint_(udp::endpoint(udp::v4(), udp_remote_port)),
      socket_(io_context, udp::endpoint(udp::v4(), udp_local_port)),
      serial_port_{io_context}, read_serial_count_(0), read_udp_count_(0) {
    udp_recv_buffer_.resize(4096*10);
    serial_recv_buffer_.resize(4096*10);
}

Bridge::~Bridge() {
    disconnect_serial();
    disconnect_udp();
}

void
Bridge::open_serial_port(const std::string& serial_port_name) {
    serial_port_.open(serial_port_name);
    serial_port_.set_option( boost::asio::serial_port_base::baud_rate( 2000000 ));
}

void 
Bridge::disconnect_serial() {
    if(serial_port_.is_open()) {
        serial_port_.cancel();
        serial_port_.close();
    }
}

void 
Bridge::disconnect_udp() {
    if(socket_.is_open()) {
        std::cout << "Socket closed" << std::endl;
        socket_.close();
    }
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
Bridge::udp_send_serial_status() {
    if(serial_port_.is_open()) {
        if(status_up_response_.size() > 0)
            write_udp(status_up_response_);
    } else {
        if(status_down_response_.size() > 0)
            write_udp(status_down_response_);
    }
}

void Bridge::set_status_udp_request_message(const std::vector<std::byte> &match) {
    status_request_message_ = match;
}

void Bridge::set_status_down_udp_response(const std::vector<std::byte> &response) {
    status_down_response_ = response;
}

void Bridge::set_status_up_udp_response(const std::vector<std::byte> &response) {
    status_up_response_ = response;
}

bool
Bridge::is_status_request_message_set() {
    return status_request_message_.size() > 0;
}

bool
Bridge::is_status_request_message_received() {
    return is_status_request_message_set() && message_found(status_request_message_, udp_recv_buffer_);
}

bool
Bridge::message_found(const std::vector<std::byte>& message, const std::vector<std::byte>& buffer) {
    return std::search(buffer.begin(), buffer.end(), message.begin(), message.end()) != buffer.end();
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
        if(is_status_request_message_received() || !serial_port_.is_open()) {
            udp_send_serial_status();
            do_read_udp();
        } else {
            auto encoded = slip::encode(udp_recv_buffer_.cbegin(), udp_recv_buffer_.cbegin() + bytes_transferred);

            write_serial(encoded);
            do_read_udp();
        }
    } else {
        std::cerr << "ERROR " << error.value() << " during UDP read: " << error.message() << std::endl;
        // continue reading from UDP even on error
        do_read_udp();
    }
}

void
Bridge::write_udp(const std::vector<std::byte>& data) {
    {
        std::scoped_lock udp_send_queue_lock(udp_send_queue_mutex_);
        std::copy(data.cbegin(), data.cend(), std::back_inserter(udp_send_queue_));
    }
    do_write_udp();
}

void
Bridge::do_write_udp() {
    // serial reads as well as udp reads can trigger an async_send_to on the socket, a mutex protected separate buffer is required
    
    std::scoped_lock udp_send_buffer_lock(udp_send_buffer_mutex_);
    if(udp_send_buffer_.size() != 0) {
        return;
    }

    std::scoped_lock udp_send_queue_lock(udp_send_queue_mutex_);
    if(udp_send_queue_.size() == 0) {
        return;
    }

    std::copy(udp_send_queue_.begin(), udp_send_queue_.end(), std::back_inserter(udp_send_buffer_));
    udp_send_queue_.clear();

    socket_.async_send_to(boost::asio::buffer(udp_send_buffer_),
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
    {
        std::scoped_lock udp_send_buffer_lock(udp_send_buffer_mutex_);
        udp_send_buffer_.clear();
    }
    // write until udp queue is empty
    do_write_udp();

    // continue reading from serial even on UDP write error, but only if serial connection is open and if 
    // no other async read is in place
    if(serial_port_.is_open() && read_serial_count_ == 0) {
        do_read_serial();
    }
}

void
Bridge::do_read_serial() {
    read_serial_count_++;
    serial_port_.async_read_some(boost::asio::buffer(serial_recv_buffer_),
                                    boost::bind(&Bridge::read_serial_complete, this,
                                                boost::asio::placeholders::error,
                                                boost::asio::placeholders::bytes_transferred));
}

void
Bridge::read_serial_complete(const boost::system::error_code &error, std::size_t bytes_transferred) {
    read_serial_count_--;
    if (!error) {
        std::vector<std::byte> received(serial_recv_buffer_.begin(), serial_recv_buffer_.begin() + bytes_transferred);
        if(received[0] == slip::SLIP_END) {
            slip_encoded_udp_buffer_.append(received);

            auto [status, data] = slip_encoded_udp_buffer_.get_next_frame();
            if (status != slip::SLIPInputStream::FrameStatus::VALID) {
                do_read_serial();
                return;
            }

            write_udp(data);

        } else {
            do_read_serial();
            return;
        }
    } else {
        std::cerr << "ERROR " << error.value() << " during serial read: " << error.message() << std::endl;
        // Throw if serial error happened to exit IOContext. Caller should handle exception.
        throw SerialConnectionException();
    }
}

void
Bridge::write_serial(const std::vector<std::byte>& data) {
    {
        std::scoped_lock serial_send_queue_lock(udp_send_queue_mutex_);
        std::copy(data.cbegin(), data.cend(), std::back_inserter(serial_send_queue_));
    }
    do_write_serial();
}

void
Bridge::do_write_serial() {
    std::scoped_lock serial_send_buffer_lock(serial_send_buffer_mutex_);
    if(serial_send_buffer_.size() != 0) {
        return;
    }

    std::scoped_lock serial_send_queue_lock(serial_send_queue_mutex_);
    if(serial_send_queue_.size() == 0) {
        return;
    }

    std::copy(serial_send_queue_.begin(), serial_send_queue_.end(), std::back_inserter(serial_send_buffer_));
    serial_send_queue_.clear();

    boost::asio::async_write(serial_port_,
                             boost::asio::buffer(serial_send_buffer_),
                             boost::bind(&Bridge::write_serial_complete, this,
                                         boost::asio::placeholders::error,
                                         boost::asio::placeholders::bytes_transferred));
}

void
Bridge::write_serial_complete(const boost::system::error_code &error, std::size_t bytes_transferred) {
    if (!error) {
        {
            std::scoped_lock serial_send_buffer_lock(serial_send_buffer_mutex_);
            serial_send_buffer_.clear();
        }
        do_write_serial();
    } else {
        std::cerr << "ERROR " << error.value() << " during serial write: " << error.message() << std::endl;
        // Throw if serial error happened to exit IOContext. Caller should handle exception.
        throw SerialConnectionException();
    }
}