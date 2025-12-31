#ifndef LSU_H
#define LSU_H

#include <iostream>
#include <vector>
#include <map>
#include <stdint.h>
#include <iomanip>

// 访存操作类型
enum class MemOp { LOAD, STORE, IDLE };

// 指令包结构
struct LSRequest {
    int inst_id;
    MemOp op;
    uint64_t addr;
    uint32_t data;
    int remaining_cycles; // 模拟访存延迟
    bool is_done;

    LSRequest(int id, MemOp o, uint64_t a, uint32_t d, int lat)
        : inst_id(id), op(o), addr(a), data(d), remaining_cycles(lat), is_done(false) {}
};

class LSU {
public:
    LSU(int load_latency = 2, int store_latency = 1);
    ~LSU();

    // 向上层接口：发起请求
    void issue_request(int id, MemOp op, uint64_t addr, uint32_t data = 0);

    // 时钟驱动函数：每周期调用一次
    void tick();

    // 检查是否有已完成的 Load 数据返回
    bool has_completed_load(int &id, uint32_t &data);

    // 模拟内存初始化
    void init_memory(uint64_t addr, uint32_t data);

    // 打印当前系统状态
    void dump_status();

private:
    int m_load_latency;
    int m_store_latency;

    // 模拟物理内存
    std::map<uint64_t, uint32_t> m_memory;

    // Store Buffer (SB): 模拟已执行但未正式写入内存的 Store
    // 关键功能：用于 Store-to-Load Forwarding
    std::map<uint64_t, uint32_t> m_store_buffer;

    // 正在处理中的请求队列
    std::vector<LSRequest> m_inflight_queue;

    // 已完成待写回的数据
    std::vector<std::pair<int, uint32_t>> m_finished_loads;

    // 内部处理函数
    uint32_t read_logic(uint64_t addr);
    void write_logic(uint64_t addr, uint32_t data);
};

#endif