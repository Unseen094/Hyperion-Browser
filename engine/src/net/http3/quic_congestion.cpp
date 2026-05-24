#include "hre/net/http3/quic_congestion.hpp"
#include <algorithm>
#include <cmath>

namespace hre::net::http3 {

congestion_controller::congestion_controller()
    : m_mss(1460)
    , m_congestion_window(INITIAL_WINDOW * 1460)
    , m_slow_start_threshold(UINT32_MAX)
    , m_bytes_in_flight(0)
{}

void congestion_controller::reset() {
    m_congestion_window = INITIAL_WINDOW * m_mss;
    m_slow_start_threshold = UINT32_MAX;
    m_bytes_in_flight = 0;
}

// === Reno ===

reno_congestion::reno_congestion() : congestion_controller() {}

void reno_congestion::on_packet_sent(uint32_t bytes) {
    m_bytes_in_flight += bytes;
}

void reno_congestion::on_packet_acked(uint32_t bytes, uint32_t /*rtt*/) {
    m_bytes_in_flight = (bytes > m_bytes_in_flight) ? 0 : m_bytes_in_flight - bytes;

    if (m_congestion_window < m_slow_start_threshold) {
        // Slow start
        m_congestion_window += bytes;
    } else {
        // Congestion avoidance
        m_congestion_window += (m_mss * bytes) / m_congestion_window;
    }
}

void reno_congestion::on_packet_lost(uint32_t bytes) {
    m_bytes_in_flight = (bytes > m_bytes_in_flight) ? 0 : m_bytes_in_flight - bytes;
    m_slow_start_threshold = m_congestion_window / 2;
    m_congestion_window = std::max(m_congestion_window / 2, MINIMUM_WINDOW * m_mss);
}

bool reno_congestion::can_send(uint32_t bytes_in_flight) const {
    return bytes_in_flight + m_mss <= m_congestion_window;
}

void reno_congestion::reset() { congestion_controller::reset(); }

// === Cubic ===

cubic_congestion::cubic_congestion()
    : congestion_controller()
    , m_cubic_c(0.4)
    , m_last_max_window(0)
{}

double cubic_congestion::cubic_root(double x) const {
    return (x < 0) ? -std::pow(-x, 1.0 / 3.0) : std::pow(x, 1.0 / 3.0);
}

void cubic_congestion::on_packet_sent(uint32_t bytes) {
    m_bytes_in_flight += bytes;
}

void cubic_congestion::on_packet_acked(uint32_t bytes, uint32_t rtt) {
    m_bytes_in_flight = (bytes > m_bytes_in_flight) ? 0 : m_bytes_in_flight - bytes;

    if (m_congestion_window < m_slow_start_threshold) {
        m_congestion_window += bytes;
        return;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_window_reduced_at).count() / 1000.0;

    double target = m_last_max_window * m_cubic_c *
                    std::pow(elapsed - cubic_root(m_last_max_window * m_cubic_c / 2.0), 3.0);

    uint32_t cubic_cwnd = std::max(static_cast<uint32_t>(target), MINIMUM_WINDOW * m_mss);

    if (cubic_cwnd > m_congestion_window) {
        m_congestion_window += std::min(bytes, cubic_cwnd - m_congestion_window);
    } else {
        m_congestion_window += m_mss / 100;
    }
}

void cubic_congestion::on_packet_lost(uint32_t bytes) {
    m_bytes_in_flight = (bytes > m_bytes_in_flight) ? 0 : m_bytes_in_flight - bytes;
    m_last_max_window = m_congestion_window;
    m_window_reduced_at = std::chrono::steady_clock::now();
    m_slow_start_threshold = static_cast<uint32_t>(m_congestion_window * 0.7);
    m_congestion_window = std::max(static_cast<uint32_t>(m_congestion_window * 0.7),
                                   MINIMUM_WINDOW * m_mss);
}

bool cubic_congestion::can_send(uint32_t bytes_in_flight) const {
    return bytes_in_flight + m_mss <= m_congestion_window;
}

void cubic_congestion::reset() { congestion_controller::reset(); m_last_max_window = 0; }

// === BBR ===

bbr_congestion::bbr_congestion()
    : congestion_controller()
    , m_bandwidth_estimate(0)
    , m_min_rtt(UINT32_MAX)
    , m_pacing_gain(2)
    , m_bbr_state(bbr_state::BBR_STARTUP)
{
    m_congestion_window = 2 * m_mss;
}

void bbr_congestion::on_packet_sent(uint32_t bytes) {
    m_bytes_in_flight += bytes;
}

void bbr_congestion::on_packet_acked(uint32_t bytes, uint32_t rtt) {
    m_bytes_in_flight = (bytes > m_bytes_in_flight) ? 0 : m_bytes_in_flight - bytes;
    update_bandwidth(bytes, rtt);
    update_min_rtt(rtt);
    pacing_rate_calc();

    switch (m_bbr_state) {
        case bbr_state::BBR_STARTUP:
            m_congestion_window += bytes;
            if (m_pacing_gain == 2 && m_bandwidth_estimate > 0) {
                m_bbr_state = bbr_state::BBR_DRAIN;
                m_pacing_gain = 1;
            }
            break;
        case bbr_state::BBR_DRAIN:
            m_congestion_window -= bytes / 2;
            if (m_bytes_in_flight <= m_congestion_window) {
                m_bbr_state = bbr_state::BBR_PROBE_BW;
                m_pacing_gain = 1;
            }
            break;
        case bbr_state::BBR_PROBE_BW:
            m_congestion_window += bytes / 4;
            break;
        case bbr_state::BBR_PROBE_RTT:
            if (m_congestion_window > MINIMUM_WINDOW * m_mss) {
                m_congestion_window -= m_mss;
            }
            break;
    }
}

void bbr_congestion::on_packet_lost(uint32_t bytes) {
    m_bytes_in_flight = (bytes > m_bytes_in_flight) ? 0 : m_bytes_in_flight - bytes;
    m_congestion_window = std::max(m_congestion_window / 2, MINIMUM_WINDOW * m_mss);
}

bool bbr_congestion::can_send(uint32_t bytes_in_flight) const {
    return bytes_in_flight + m_mss <= m_congestion_window;
}

void bbr_congestion::reset() {
    congestion_controller::reset();
    m_bandwidth_estimate = 0;
    m_min_rtt = UINT32_MAX;
    m_pacing_gain = 2;
    m_bbr_state = bbr_state::BBR_STARTUP;
}

void bbr_congestion::update_bandwidth(uint32_t delivered, uint32_t rtt) {
    if (rtt == 0) return;
    uint64_t bw = (static_cast<uint64_t>(delivered) * 8 * 1000) / rtt;
    m_bandwidth_estimate = std::max(m_bandwidth_estimate, bw);
}

void bbr_congestion::update_min_rtt(uint32_t rtt) {
    if (rtt < m_min_rtt && rtt > 0) {
        m_min_rtt = rtt;
    }
}

void bbr_congestion::pacing_rate_calc() {
    if (m_min_rtt == UINT32_MAX || m_min_rtt == 0) return;
    uint64_t pacing_rate = m_bandwidth_estimate * m_pacing_gain;
    m_congestion_window = static_cast<uint32_t>(
        std::max<uint64_t>(pacing_rate * m_min_rtt / 8000, MINIMUM_WINDOW * m_mss));
}

} // namespace hre::net::http3
