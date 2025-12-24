#include "islip.h"
#include <iostream>
#include <algorithm>
#include <vector>

// 构造函数增加 priority_num 参数，默认为 1
islip::islip(int input_num, int output_num, int priority_num)
{
    m_num_port = std::max(input_num, output_num);
    m_num_prio = priority_num; // 新增：记录优先级层级数
    
    // 初始化指针：现在的指针数量是 端口数 * 优先级数
    // 索引方式：p * m_num_port + port_index
    m_gi.resize(m_num_prio * m_num_port, 0);
    m_ai.resize(m_num_prio * m_num_port, 0);
    
    // 初始化 VOQ：大小变为 Input * Output * Priority
    m_ql.resize(m_num_port * m_num_port * m_num_prio, 0);

    // 握手信号矩阵大小不变 (因为每次只调度一个优先级)
    m_accept.resize(m_num_port * m_num_port);
    m_request.resize(m_num_port * m_num_port);
    m_grant.resize(m_num_port * m_num_port);

    // Speedup 计数器
    m_input_match_count.resize(m_num_port);
    m_output_match_count.resize(m_num_port);

    init_priority_ptr();
}

islip::~islip() {}

void islip::init_priority_ptr()
{
    // 对每个优先级的每个端口指针进行初始化
    std::fill(m_ai.begin(), m_ai.end(), 0);
    std::fill(m_gi.begin(), m_gi.end(), 0);
}

void islip::init()
{
    std::fill(m_accept.begin(), m_accept.end(), false);
    std::fill(m_request.begin(), m_request.end(), false);
    std::fill(m_grant.begin(), m_grant.end(), false);
    std::fill(m_ql.begin(), m_ql.end(), 0);
    sch_result.clear();
}

// 修改 set_ql：增加 priority 参数 (0 是最高优先级)
void islip::set_ql(int i, int j, int prio)
{
    if (prio >= m_num_prio) return;
    // 索引计算：(i * num_out + j) * num_prio + prio
    int index = (i * m_num_port + j) * m_num_prio + prio;
    m_ql.at(index)++;
}

/**
 * @brief 支持 Speedup + Priority 的核心调度逻辑
 * @param max_iterations 迭代次数
 * @param speedup 加速比
 */
void islip::islip_sch(int max_iterations, int speedup)
{
    // 1. 初始化本周期的配额状态 (所有优先级共享这个配额)
    m_input_match_count.assign(m_num_port, 0);
    m_output_match_count.assign(m_num_port, 0);
    sch_result.clear();

    // 2. 创建影子队列
    m_shadow_ql = m_ql;

    // --- 外层循环：严格优先级调度 (Strict Priority) ---
    // 先调度 Prio 0 (高)，再调度 Prio 1 (低)...
    for (int p = 0; p < m_num_prio; ++p)
    {
        // --- 内层循环：标准的 iSLIP 迭代 ---
        for (m_current_iter = 0; m_current_iter < max_iterations; ++m_current_iter)
        {
            // 每一轮迭代重置握手信号
            std::fill(m_request.begin(), m_request.end(), false);
            std::fill(m_grant.begin(), m_grant.end(), false);
            std::fill(m_accept.begin(), m_accept.end(), false);

            // 传入当前正在处理的优先级 p
            send_request(speedup, p);      
            do_grant(speedup, p);          
            do_accept(speedup, p);         
            update_priority_ptr(p); 
        }
    }
}

void islip::send_request(int speedup, int prio)
{
    for (int i = 0; i < m_num_port; i++)
    {
        // 检查配额：如果高优先级已经把 Speedup 用完了，低优先级就无法发送请求
        if (m_input_match_count[i] >= speedup) continue;

        for (int j = 0; j < m_num_port; j++)
        {
            // 检查影子队列中 当前优先级 是否有包
            int ql_index = (i * m_num_port + j) * m_num_prio + prio;

            if (m_output_match_count[j] < speedup && m_shadow_ql.at(ql_index) > 0)
            {
                m_request.at(i * m_num_port + j) = true;
            }
        }
    }
}

void islip::do_grant(int speedup, int prio)
{
    for (int j = 0; j < m_num_port; j++)
    {
        if (m_output_match_count[j] >= speedup) continue;

        // 获取当前优先级 prio 对应的 Grant 指针
        int ptr_idx = prio * m_num_port + j;
        int start_i = m_gi.at(ptr_idx);
        
        int i = start_i;
        do 
        {
            if (m_request.at(i * m_num_port + j))
            {
                m_grant.at(i * m_num_port + j) = true;
                break; 
            }
            i = (i + 1) % m_num_port;
        } while (i != start_i);
    }
}

void islip::do_accept(int speedup, int prio)
{
    for (int i = 0; i < m_num_port; i++)
    {
        if (m_input_match_count[i] >= speedup) continue;

        // 获取当前优先级 prio 对应的 Accept 指针
        int ptr_idx = prio * m_num_port + i;
        int start_j = m_ai.at(ptr_idx);

        int j = start_j;
        do 
        {
            if (m_grant.at(i * m_num_port + j))
            {
                m_accept.at(i * m_num_port + j) = true;
                
                // 更新配额
                m_input_match_count[i]++;
                m_output_match_count[j]++;
                
                // 扣减影子队列 (注意指定优先级)
                int ql_index = (i * m_num_port + j) * m_num_prio + prio;
                m_shadow_ql.at(ql_index)--; 
                break;
            }
            j = (j + 1) % m_num_port;
        } while (j != start_j);
    }
}

void islip::update_priority_ptr(int prio)
{
    for (int i = 0; i < m_num_port; i++)
    {
        for (int j = 0; j < m_num_port; j++)
        {
            if (m_accept.at(i * m_num_port + j))
            {
                // iSLIP 规则：仅在第一轮迭代更新指针
                if (m_current_iter == 0)
                {
                    // 只更新当前优先级层级的指针，互不干扰
                    m_gi.at(prio * m_num_port + j) = (i + 1) % m_num_port;
                    m_ai.at(prio * m_num_port + i) = (j + 1) % m_num_port;
                }
                
                // 记录匹配结果 (可以考虑将 prio 也存入 result，视需求而定)
                sch_result.emplace_back(std::make_pair(i, j));
            }
        }
    }
}