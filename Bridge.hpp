#ifndef OSS_BRIDGE_HPP
#define OSS_BRIDGE_HPP

#include <ctime>
#include <cstddef>
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

struct SerialConnectionException : public std::exception {
	const char * what () const throw () {
    	return "Serial connection exception";
    }
};

class Bridge {
    public:
        Bridge(boost::asio::io_context &io_context, int udp_remote_port, int udp_local_port);
        ~Bridge();

        void open_serial_port(const std::string& serial_port_name);
        void run_serial();
        void run_udp();
        void disconnect_serial();
        void disconnect_udp();

        void set_status_udp_request_message(const std::vector<std::byte>& match);
        void set_status_down_udp_response(const std::vector<std::byte>& response);
        void set_status_up_udp_response(const std::vector<std::byte>& response);

        bool is_status_request_message_set();

    private:
        void do_read_udp();
        void read_udp_complete(const boost::system::error_code &error, std::size_t bytes_transferred);

        void write_udp(const std::vector<std::byte>& data);
        void do_write_udp();
        void write_udp_complete(const boost::system::error_code &error, size_t bytes_transferred);

        void do_read_serial();
        void read_serial_complete(const boost::system::error_code &error, std::size_t bytes_transferred);

        void write_serial(const std::vector<std::byte>& data);
        void write_serial_complete(const boost::system::error_code &error, size_t bytes_transferred);

        void udp_send_serial_status();

        bool is_status_request_message_received();
        static bool message_found(const std::vector<std::byte>& message, const std::vector<std::byte>& buffer);

        udp::endpoint remote_endpoint_;
        udp::socket socket_;

        boost::asio::serial_port serial_port_;

        std::vector<std::byte> serial_recv_buffer_;
        std::vector<std::byte> udp_send_buffer_;
        std::vector<std::byte> udp_recv_buffer_;
        std::vector<std::byte> udp_send_queue_;

        std::mutex udp_send_buffer_mutex_;
        std::mutex udp_send_queue_mutex_;

        slip::SLIPInputStream slip_encoded_udp_buffer_;

        std::vector<std::byte> status_request_message_;
        std::vector<std::byte> status_down_response_;
        std::vector<std::byte> status_up_response_;

        int read_serial_count_, read_udp_count_;
};

#endif // OSS_BRIDGE_HPP