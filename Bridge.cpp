#include "Bridge.hpp"

Bridge::Bridge(boost::asio::io_context &io_context, int udp_remote_port, int udp_local_port)
    : remote_endpoint_(udp::endpoint(udp::v4(), udp_remote_port)),
      socket_(io_context, udp::endpoint(udp::v4(), udp_local_port)),
      serial_port_{io_context},
      serial_connected_{false},
      n_serial_writes_(0) {
    udp_recv_buffer_.resize(4096);
    serial_recv_buffer_.resize(4096);
}

void
Bridge::open_serial_port(const std::string& serial_port_name) {
    serial_port_.open(serial_port_name);
    serial_connected_ = true;
}

void 
Bridge::disconnect_serial() {
    serial_port_.cancel();
    serial_port_.close();
    serial_connected_ = false;
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
        {
            std::scoped_lock serial_write_queue_lock(serial_write_queue_mutex_);
            std::copy(encoded.begin(), encoded.end(), std::back_inserter(serial_write_queue_));
        }
        if(serial_connected_) {
            do_write_serial();
        }
        do_read_udp();
    } else {
        std::cout << "ERROR read udp: " << error.message() << std::endl;
    }
}

void
Bridge::do_write_udp() {
    std::scoped_lock udp_write_buffer_lock(udp_write_buffer_mutex_);
    if (udp_write_buffer_.size() != 0) {
        return;
    }

    std::scoped_lock slip_encoder_lock(slip_encoder_mutex_);
    if (slip_encoded_udp_buffer_.data_size() == 0) {
        return;
    }

    auto [status, data] = slip_encoded_udp_buffer_.get_next_frame();

    if (status != slip::SLIPInputStream::FrameStatus::VALID) {
        return;
    }

    std::copy(data.begin(), data.end(), std::back_inserter(udp_write_buffer_));

    socket_.async_send_to(boost::asio::buffer(udp_write_buffer_),
                            remote_endpoint_,
                            boost::bind(&Bridge::write_udp_complete, this,
                                        boost::asio::placeholders::error,
                                        boost::asio::placeholders::bytes_transferred));
}

void
Bridge::write_udp_complete(const boost::system::error_code &error, size_t bytes_transferred) {
    if (error) {
        std::cout << "ERROR write udp: " << error.message() << std::endl;
    }
    if (!error) {
        {
            std::scoped_lock udp_write_buffer_lock(udp_write_buffer_mutex_);
            udp_write_buffer_.clear();
        }
        do_write_udp();
    }
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
        std::vector<std::byte> received;
        std::copy(serial_recv_buffer_.begin(), serial_recv_buffer_.begin() + bytes_transferred, std::back_inserter(received));

        { 
            std::scoped_lock slip_encoder_lock(slip_encoder_mutex_);
            slip_encoded_udp_buffer_.append(received);
        }

        do_write_udp();
        do_read_serial();
    } else {
        std::cout << "ERROR read serial: " << error.message() << std::endl;
        throw(0);
    }
}

void
Bridge::do_write_serial() {
    std::scoped_lock serial_write_buffer_lock(serial_write_buffer_mutex_);
    if (serial_write_buffer_.size() != 0) {
        return;
    }

    std::scoped_lock serial_write_queue_lock(serial_write_queue_mutex_);
    if (serial_write_queue_.size() == 0) {
        return;
    }

    std::copy(serial_write_queue_.begin(), serial_write_queue_.end(),
                std::back_inserter(serial_write_buffer_));
    serial_write_queue_.clear();

    boost::asio::async_write(serial_port_,
                                boost::asio::buffer(serial_write_buffer_),
                                boost::bind(&Bridge::write_serial_complete, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
}

void
Bridge::write_serial_complete(const boost::system::error_code &error, size_t bytes_transferred) {
    --n_serial_writes_;
    if (!error) {
        {
            std::scoped_lock serial_write_buffer_lock(serial_write_buffer_mutex_);
            serial_write_buffer_.clear();
        }
        do_write_serial();
    } else {
        std::cout << "ERROR DURING SERIAL WRITE " << error.message() << std::endl;
    }
}