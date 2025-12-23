#ifndef PEQ_H
#define PEQ_H

#include <map>
#include <memory>
#include <systemc>

template <typename T>
class peq {
public:
    peq() = default;
    ~peq() = default;
    
    /**
     * @brief 将载荷推入队列，并指定相对当前时间的延迟
     * @param delay 相对延迟时间 (Relative delay)
     * @param trans 载荷对象
     */
    // Accept an lvalue reference to allow callers to pass a named
    // `std::unique_ptr` (e.g. `p`) without having to write `std::move(p)`.
    // The function will take ownership by moving the pointer into the queue.
    void push_delayed_payload(const sc_core::sc_time& delay, std::unique_ptr<T>& pld) {
        sc_core::sc_time absolute_time = delay + sc_core::sc_time_stamp();
        m_peq.insert(std::make_pair(absolute_time, std::move(pld)));
    }

    /**
     * @brief 弹出当前仿真时刻已经到期（Expired）的载荷
     * @return 成功则返回载荷指针，若无到期载荷则返回 nullptr
     */
    std::unique_ptr<T> pop_expired_payload() {
        if (m_peq.empty()) {
            return nullptr;
        }

        sc_core::sc_time now = sc_core::sc_time_stamp();
        if (m_peq.begin()->first > now) {
            return nullptr;
        } else {
            // 获取迭代器
            auto it = m_peq.begin();
            // 使用 move 获取智能指针，减少一次计数操作
            std::unique_ptr<T> trans = std::move(it->second);
            m_peq.erase(m_peq.begin());
            return trans;
        }
    }

    // 获取当前队列中在途 (In-flight) 的载荷数量
    std::size_t get_inflight_count() const {
        return m_peq.size();
    }

private:
    std::multimap<sc_core::sc_time, std::unique_ptr<T>> m_peq;
};

#endif // PEQ_H