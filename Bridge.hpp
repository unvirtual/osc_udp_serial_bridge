#ifndef OSS_BRIDGE_HPP
#define OSS_BRIDGE_HPP

#include <ctime>
#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/io/ios_state.hpp>
#include "SLIP.hpp"

using boost::asio::ip::udp;

class Bridge {
    public:
        Bridge(boost::asio::io_context &io_context, int udp_remote_port, int udp_local_port);
        void open_serial_port(const std::string& serial_port_name);
        void disconnect_serial();
        void run_serial();
        void run_udp();

    private:
        void do_read_udp();
        void read_udp_complete(const boost::system::error_code &error, std::size_t bytes_transferred);
        void do_write_udp();
        void write_udp_complete(const boost::system::error_code &error, size_t bytes_transferred);

        void do_read_serial();
        void read_serial_complete(const boost::system::error_code &error, std::size_t bytes_transferred);
        void do_write_serial();
        void write_serial_complete(const boost::system::error_code &error, size_t bytes_transferred);

        udp::socket socket_;
        udp::endpoint remote_endpoint_;

        boost::asio::serial_port serial_port_;

        std::vector<std::byte> serial_recv_buffer_;
        std::vector<std::byte> serial_write_buffer_;
        std::vector<std::byte> serial_write_queue_;

        std::vector<std::byte> udp_recv_buffer_;
        std::vector<std::byte> udp_write_buffer_;
        slip::SLIPInputStream slip_encoded_udp_buffer_;

        std::mutex udp_write_buffer_mutex_;
        std::mutex slip_encoder_mutex_;
        std::mutex serial_write_buffer_mutex_;
        std::mutex serial_write_queue_mutex_;

        bool serial_connected_;
        int n_serial_writes_;
};

#endif // OSS_BRIDGE_HPP