#ifndef ISLIP_H
#define ISLIP_H

#include <stdlib.h>
#include <vector>
#include <deque>
#include <utility> // for std::pair

class islip
{
public:
    // 构造函数增加 priority_num 参数，默认值为 1
    islip(int input_num, int output_num, int priority_num = 1);
    ~islip();

public:
    void init();
    void init_priority_ptr();
    
    // 设置队列长度：增加 prio 参数 (0为最高优先级)
    void set_ql(int i, int j, int prio = 0);
    
    // 主调度函数
    void islip_sch(int max_iterations = 1, int speedup = 1, bool is_islip_mode = true);

    // 内部四个阶段函数：都需要感知当前正在调度哪个优先级
    void send_request(int speedup, int prio);
    void do_grant(int speedup, int prio, bool is_islip_mode = true);
    void do_accept(int speedup, int prio);
    void update_priority_ptr(int prio, bool is_islip_mode = true);

public:
    int m_num_port;
    int m_num_prio; // 新增：优先级数量

    // 仲裁指针：大小变为 m_num_prio * m_num_port
    // 索引方式：prio * m_num_port + port_index
    // 这样保证不同优先级的指针是独立的，互不干扰
    std::vector<int>  m_gi;
    std::vector<int>  m_ai;

    // VOQ队列状态：逻辑大小为 Input * Output * Priority
    // 索引方式：(input * m_num_port + output) * m_num_prio + prio
    std::vector<int>  m_ql;

    // 握手信号矩阵 (大小通常为 Input * Output，每次迭代复用)
    std::vector<bool> m_accept;
    std::vector<bool> m_request;
    std::vector<bool> m_grant;

    // 调度结果：存储 (Input, Output) 对
    std::vector<std::pair<int, int>> sch_result;

    // 影子队列：用于迭代过程中的状态追踪
    std::vector<int> m_shadow_ql;

    // Speedup 配额计数器
    std::vector<int> m_input_match_count;
    std::vector<int> m_output_match_count;
    
    int m_current_iter;
};

#endif // ISLIP_H