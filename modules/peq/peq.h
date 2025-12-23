#ifndef PEQ_H
#define PEQ_H

#include <map>
#include <memory>

template <typename T>
class peq {
public:
    peq() = default;
    ~peq() = default;
    
    void delay(const sc_core::sc_time& t, const T& trans) {
        sc_core::sc_time delay_time = t + sc_core::sc_time_stamp();
        std::shared_ptr<T> pld = std::make_shared<T>(trans);
        m_peq.insert(std::make_pair(delay_time, pld));
    }

    std::shared_ptr<T> get_next_event() {
        if (m_peq.empty()) {
            return nullptr;
        }

        sc_core::sc_time now = sc_core::sc_time_stamp();
        if (m_peq.begin()->first > now) {
            return nullptr;
        } else {
            auto trans = m_peq.begin()->second;
            m_peq.erase(m_peq.begin());
            return trans;
        }
    }

    int size() const {
        return m_peq.size();
    }
private:
    std::multimap<sc_core::sc_time,  std::shared_ptr<T>> m_peq;
};

#endif // PEQ_H