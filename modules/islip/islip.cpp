#include "islip.h"
#include <iostream>
#include <algorithm>
#include <vector>

islip::islip(int input_num, int output_num)
{
    m_num_port = std::max(input_num, output_num);
    
    // 初始化指针
    m_gi.resize(m_num_port, 0);
    m_ai.resize(m_num_port, 0);
    
    // 初始化矩阵：m_ql 现在建议存储具体的包数量 (int)
    m_ql.resize(m_num_port * m_num_port, 0);
    m_accept.resize(m_num_port * m_num_port);
    m_request.resize(m_num_port * m_num_port);
    m_grant.resize(m_num_port * m_num_port);

    // Speedup 状态记录：记录本周期各端口已匹配次数
    m_input_match_count.resize(m_num_port);
    m_output_match_count.resize(m_num_port);

    init_priority_ptr();
}

islip::~islip() {}

void islip::init_priority_ptr()
{
    for (int i = 0; i < m_num_port; i++)
    {
        m_ai.at(i) = 0; 
        m_gi.at(i) = 0;
    }
}

void islip::init()
{
    std::fill(m_accept.begin(), m_accept.end(), false);
    std::fill(m_request.begin(), m_request.end(), false);
    std::fill(m_grant.begin(), m_grant.end(), false);
    std::fill(m_ql.begin(), m_ql.end(), 0); // 注意：这里重置为0个包
    sch_result.clear();
}

// 修改 set_ql 以支持增加包计数
void islip::set_ql(int i, int j)
{
    m_ql.at(i * m_num_port + j)++;
}

/**
 * @brief 支持 Speedup 的核心调度逻辑
 * @param max_iterations 迭代次数
 * @param speedup 加速比 (默认1)
 */
void islip::islip_sch(int max_iterations, int speedup)
{
    // 1. 初始化本周期的配额状态
    m_input_match_count.assign(m_num_port, 0);
    m_output_match_count.assign(m_num_port, 0);
    sch_result.clear();

    // 2. 创建影子队列，用于在本周期迭代内追踪剩余包数，不影响外部 m_ql
    m_shadow_ql = m_ql;

    // 3. 开始迭代循环
    for (m_current_iter = 0; m_current_iter < max_iterations; ++m_current_iter)
    {
        // 每一轮迭代重置握手信号
        std::fill(m_request.begin(), m_request.end(), false);
        std::fill(m_grant.begin(), m_grant.end(), false);
        std::fill(m_accept.begin(), m_accept.end(), false);

        // 内部流程封装
        send_request(speedup);       
        do_grant(speedup);           
        do_accept(speedup);          
        update_priority_ptr(); 
    }
}

void islip::send_request(int speedup)
{
    for (int i = 0; i < m_num_port; i++)
    {
        // 如果输入端口 i 的配额已用完，本周期不再发请求
        if (m_input_match_count[i] >= speedup) continue;

        for (int j = 0; j < m_num_port; j++)
        {
            // 如果输出端口 j 还没满，且 VOQ 中还有包
            if (m_output_match_count[j] < speedup && m_shadow_ql.at(i * m_num_port + j) > 0)
            {
                m_request.at(i * m_num_port + j) = true;
            }
        }
    }
}

void islip::do_grant(int speedup)
{
    for (int j = 0; j < m_num_port; j++)
    {
        if (m_output_match_count[j] >= speedup) continue;

        int start_i = m_gi.at(j);
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

void islip::do_accept(int speedup)
{
    for (int i = 0; i < m_num_port; i++)
    {
        if (m_input_match_count[i] >= speedup) continue;

        int start_j = m_ai.at(i);
        int j = start_j;
        do 
        {
            if (m_grant.at(i * m_num_port + j))
            {
                m_accept.at(i * m_num_port + j) = true;
                
                // 关键：增加配额计数，并扣减影子队列中的包
                m_input_match_count[i]++;
                m_output_match_count[j]++;
                m_shadow_ql.at(i * m_num_port + j)--; 
                break;
            }
            j = (j + 1) % m_num_port;
        } while (j != start_j);
    }
}

void islip::update_priority_ptr()
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
                    m_gi.at(j) = (i + 1) % m_num_port;
                    m_ai.at(i) = (j + 1) % m_num_port;
                }
                
                // 记录匹配结果
                sch_result.emplace_back(std::make_pair(i, j));
            }
        }
    }
}