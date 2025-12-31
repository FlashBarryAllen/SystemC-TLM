#include "lsu.h"

LSU::LSU(int load_latency, int store_latency) 
    : m_load_latency(load_latency), m_store_latency(store_latency) {}

LSU::~LSU() {}

void LSU::init_memory(uint64_t addr, uint32_t data) {
    m_memory[addr] = data;
}

void LSU::issue_request(int id, MemOp op, uint64_t addr, uint32_t data) {
    int latency = (op == MemOp::LOAD) ? m_load_latency : m_store_latency;
    m_inflight_queue.emplace_back(id, op, addr, data, latency);
    
    // 如果是 Store，立即更新 Store Buffer 以便后续 Load Forwarding
    if (op == MemOp::STORE) {
        m_store_buffer[addr] = data;
    }
}

uint32_t LSU::read_logic(uint64_t addr) {
    // 1. 优先从 Store Buffer 获取数据 (Forwarding)
    if (m_store_buffer.count(addr)) {
        std::cout << "[LSU] Forwarding: Load addr 0x" << std::hex << addr << " from SB" << std::endl;
        return m_store_buffer[addr];
    }
    // 2. 否则从内存获取
    return m_memory.count(addr) ? m_memory[addr] : 0xDEADBEEF;
}

void LSU::tick() {
    auto it = m_inflight_queue.begin();
    while (it != m_inflight_queue.end()) {
        it->remaining_cycles--;

        if (it->remaining_cycles <= 0) {
            if (it->op == MemOp::LOAD) {
                uint32_t val = read_logic(it->addr);
                m_finished_loads.push_back({it->inst_id, val});
            } 
            else if (it->op == MemOp::STORE) {
                // Store 真正写入内存 (模拟 Commit 阶段)
                m_memory[it->addr] = it->data;
                // 写入内存后，可以选择从 SB 中移除或保留（取决于实现）
                // 这里我们模拟 Commit 时才真正生效
                std::cout << "[LSU] Commit: Store ID " << it->inst_id << " to Memory" << std::endl;
            }
            it = m_inflight_queue.erase(it); // 请求处理完成，移除
        } else {
            ++it;
        }
    }
}

bool LSU::has_completed_load(int &id, uint32_t &data) {
    if (!m_finished_loads.empty()) {
        auto res = m_finished_loads.back();
        id = res.first;
        data = res.second;
        m_finished_loads.pop_back();
        return true;
    }
    return false;
}

void LSU::dump_status() {
    std::cout << "--- LSU Store Buffer ---" << std::endl;
    for(auto const& [addr, val] : m_store_buffer) {
        std::cout << "Addr: 0x" << std::hex << addr << " Data: " << std::dec << val << std::endl;
    }
    std::cout << "------------------------" << std::endl;
}