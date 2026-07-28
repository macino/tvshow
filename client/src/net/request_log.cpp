#include "tvshow/net/request_log.hpp"

#include <sstream>

namespace tvshow::net {

std::string format_request_log_entry(const RequestLogEntry& entry) {
    std::ostringstream out;
    out << entry.method << ' ';
    if (entry.status == 0) {
        out << "ERR";
    } else {
        out << entry.status;
    }
    out << ' ' << entry.bytes << "B " << entry.elapsed_ms << "ms " << entry.url;
    return out.str();
}

}  // namespace tvshow::net
