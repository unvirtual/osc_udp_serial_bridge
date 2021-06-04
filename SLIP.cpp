#include "SLIP.hpp"

#include <iostream>
#include <algorithm>
#include <vector>

namespace slip {

SLIPInputStream::SLIPInputStream() { }

void 
SLIPInputStream::append(const std::vector<std::byte>& bytes) {
    std::copy(bytes.begin(), bytes.end(), std::back_inserter(bytes_));
}

size_t 
SLIPInputStream::data_size() {
    return bytes_.size();
}

std::pair<SLIPInputStream::FrameStatus, std::vector<std::byte>> 
SLIPInputStream::get_next_frame() {
    std::vector<std::byte> result;
    auto [frame_status, range] = find_frame_range();

    if(frame_status == VALID) {
        result = decode(range.first, range.second);
    }

    if(frame_status != INCOMPLETE && frame_status != EMPTY) {
        bytes_.erase(range.first, range.second);
    } 

    return std::pair { frame_status, result };
}

std::pair<SLIPInputStream::FrameStatus, SLIPInputStream::ByteDequeRange> 
SLIPInputStream::find_frame_range() {
    ByteDeque::const_iterator input_end = bytes_.end();

    ByteDeque::const_iterator start = input_end;
    ByteDeque::const_iterator end = input_end;

    FrameStatus valid = VALID;

    if (bytes_.size() == 0) {
        return std::pair { EMPTY, std::pair { start, end } };
    } else if (bytes_.size() <= 1) {
        return std::pair { INCOMPLETE, std::pair { start, end } };
    } 

    if(*(bytes_.begin()) != SLIP_END) {
        valid = INVALID;
    } 

    start = bytes_.begin();

    // END b1 ... bn END END b1 ... bn END ... --> VALID
    // b1 ... bn END END b1 ... bn END ... --> INVALID
    // END END b1 ... bn END END b1 ... bn END ... --> INVALID
    // END b1 ... bn --> INCOMPLETE
    // b1 ... bn --> INVALID

    // search for SLIP_END
    for(ByteDeque::const_iterator it = bytes_.begin() + 1; it != bytes_.end(); ++it) {
        if(*it == SLIP_END) {
            if (it == bytes_.begin() + 1) {
                end = bytes_.begin();
                valid = INVALID;
                break;
            }
            end = it;
            break;
        }
    }

    if (valid == VALID && end == bytes_.end()) {
        valid = INCOMPLETE;
    }

    // ensure that end iterator is exclusive
    if (end != bytes_.end()) {
        end++;
    }

    return std::pair { valid, std::pair { start, end } };
}

} // namespace slip