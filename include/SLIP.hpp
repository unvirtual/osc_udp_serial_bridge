#ifndef OSCSS_SLIP_HPP
#define OSCSS_SLIP_HPP

#include <cstddef>
#include <vector>
#include <deque>
#include <type_traits>
#include <iterator>

namespace slip {

const std::byte SLIP_END {192};
const std::byte SLIP_ESC {219};
const std::byte SLIP_ESC_END {220};
const std::byte SLIP_ESC_ESC {221};

template<typename Iterator>
typename std::enable_if<std::is_same<typename Iterator::value_type, std::byte>::value, std::vector<std::byte> >::type
encode(Iterator begin, Iterator end) {
    std::vector<std::byte> result;

    if(begin == end) {
        return result;
    }

    result.push_back(SLIP_END);

    for(auto i = begin; i != end; ++i) {
        if(*i == SLIP_END) {
            result.push_back(SLIP_ESC);
            result.push_back(SLIP_ESC_END);
        } else if (*i == SLIP_ESC) {
            result.push_back(SLIP_ESC);
            result.push_back(SLIP_ESC_ESC);
        } else {
            result.push_back(*i);
        }
    }
    result.push_back(SLIP_END);

    return result;
}

template <typename Iterator>
typename std::enable_if<std::is_same<typename Iterator::value_type, std::byte>::value, std::vector<std::byte> >::type
decode(Iterator begin, Iterator end) {
    std::vector<std::byte> result;

    if(begin == end || (begin + 1) == end) {
        return result;
    }

    if(*begin != SLIP_END || *(end - 1) != SLIP_END) {
        return result;
    }

    for(auto i = begin + 1; i != end - 1; ++i) {
        if(*i == SLIP_ESC) {
            i++;

            if(*i == SLIP_ESC_END) {
                result.push_back(SLIP_END);
            } else if(*i == SLIP_ESC_ESC) {
                result.push_back(SLIP_ESC);
            }
        } else {
            result.push_back(*i);
        }
    }

    return result;
}

class SLIPInputStream {
    public:
        enum FrameStatus {
            EMPTY,
            INVALID,
            INCOMPLETE,
            VALID
        };

        SLIPInputStream();

        void append(const std::vector<std::byte>& bytes);
        size_t data_size();
        std::pair<FrameStatus, std::vector<std::byte>> get_next_frame();

    private:
        typedef std::deque<std::byte> ByteDeque;
        typedef std::pair<ByteDeque::const_iterator, ByteDeque::const_iterator> ByteDequeRange;

        ByteDeque bytes_;

        std::pair<FrameStatus, ByteDequeRange> find_frame_range();
};

} // namespace slip

#endif // OSCSS_SLIP_HPP