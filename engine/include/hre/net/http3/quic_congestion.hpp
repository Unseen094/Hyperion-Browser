#pragma once

#include <cstdint>
#include <chrono>

namespace hre::net::http3 {

class congestion_controller {
public:
    congestion_controller();
    virtual ~congestion_controller() = default;

    virtual void on_packet_sent(uint32_t bytes) = 0;
    virtual void on_packet_acked(uint32_t bytes, uint32_t rtt) = 0;
    virtual void on_packet_lost(uint32_t bytes) = 0;
    virtual uint32_t get_congestion_window() const = 0;
    virtual bool can_send(uint32_t bytes_in_flight) const = 0;
    virtual void reset() = 0;
    virtual const char* name() const = 0;

    void set_mss(uint32_t mss) { m_mss = mss; }
    uint32_t mss() const { return m_mss; }

protected:
    uint32_t m_mss;
    uint32_t m_congestion_window;
    uint32_t m_slow_start_threshold;
    uint32_t m_bytes_in_flight;

    static constexpr uint32_t INITIAL_WINDOW = 10; // 10 * MSS
    static constexpr uint32_t MINIMUM_WINDOW = 2;  // 2 * MSS
};

class reno_congestion : public congestion_controller {
public:
    reno_congestion();
    void on_packet_sent(uint32_t bytes) override;
    void on_packet_acked(uint32_t bytes, uint32_t rtt) override;
    void on_packet_lost(uint32_t bytes) override;
    uint32_t get_congestion_window() const override { return m_congestion_window; }
    bool can_send(uint32_t bytes_in_flight) const override;
    void reset() override;
    const char* name() const override { return "Reno"; }
};

class cubic_congestion : public congestion_controller {
public:
    cubic_congestion();
    void on_packet_sent(uint32_t bytes) override;
    void on_packet_acked(uint32_t bytes, uint32_t rtt) override;
    void on_packet_lost(uint32_t bytes) override;
    uint32_t get_congestion_window() const override { return m_congestion_window; }
    bool can_send(uint32_t bytes_in_flight) const override;
    void reset() override;
    const char* name() const override { return "Cubic"; }

private:
    double m_cubic_c;
    uint32_t m_last_max_window;
    std::chrono::steady_clock::time_point m_window_reduced_at;

    double cubic_root(double x) const;
};

class bbr_congestion : public congestion_controller {
public:
    bbr_congestion();
    void on_packet_sent(uint32_t bytes) override;
    void on_packet_acked(uint32_t bytes, uint32_t rtt) override;
    void on_packet_lost(uint32_t bytes) override;
    uint32_t get_congestion_window() const override { return m_congestion_window; }
    bool can_send(uint32_t bytes_in_flight) const override;
    void reset() override;
    const char* name() const override { return "BBR"; }

private:
    uint64_t m_bandwidth_estimate;
    uint32_t m_min_rtt;
    uint32_t m_pacing_gain;
    enum class bbr_state { BBR_STARTUP, BBR_DRAIN, BBR_PROBE_BW, BBR_PROBE_RTT };
    bbr_state m_bbr_state;

    void update_bandwidth(uint32_t delivered, uint32_t rtt);
    void update_min_rtt(uint32_t rtt);
    void pacing_rate_calc();
};

} // namespace hre::net::http3
