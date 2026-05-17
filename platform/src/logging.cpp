#include <hyperion/platform/logging.hpp>

namespace hyperion::platform {

logger::logger() {
    m_log_file.open("hyperion.log", std::ios::out | std::ios::app);
}

logger::~logger() {
    if (m_log_file.is_open()) {
        m_log_file.close();
    }
}

logger& logger::instance() {
    static logger inst;
    return inst;
}

} // namespace hyperion::platform
