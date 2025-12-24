#include "islip.h"
#include <iostream>
#include <algorithm>

islip::islip(int input_num, int output_num)
{
    m_num_port = std::max(input_num, output_num);
    
    // 初始化指针和矩阵
    m_gi.resize(m_num_port);
    m_ai.resize(m_num_port);
    m_ql.resize(m_num_port * m_num_port);
    m_accept.resize(m_num_port * m_num_port);
    m_request.resize(m_num_port * m_num_port);
    m_grant.resize(m_num_port * m_num_port);

    // Speedup=1 状态记录
    m_input_occupied.resize(m_num_port);
    m_output_occupied.resize(m_num_port);

    init_priority_ptr();
}

islip::~islip() {}

void islip::init_priority_ptr()
{
    for (int i = 0; i < m_num_port; i++)
    {
        m_ai.at(i) = 0; // 初始指向端口0
        m_gi.at(i) = 0;
    }
}

void islip::init()
{
    // 重置所有内部信号
    std::fill(m_accept.begin(), m_accept.end(), false);
    std::fill(m_request.begin(), m_request.end(), false);
    std::fill(m_grant.begin(), m_grant.end(), false);
    std::fill(m_ql.begin(), m_ql.end(), false);

    sch_result.clear();
}

void islip::set_ql(int i, int j)
{
    m_ql.at(i * m_num_port + j) = true;
}

/**
 * @brief 核心调度逻辑封装
 * @param max_iterations 迭代次数。Speedup=1 时，多次迭代用于寻找最大匹配
 */
void islip::islip_sch(int max_iterations)
{
    // 1. 本时钟周期开始，清空所有端口的占用状态 (Speedup=1)
    m_input_occupied.assign(m_num_port, false);
    m_output_occupied.assign(m_num_port, false);
    sch_result.clear();

    // 2. 开始迭代循环
    for (m_current_iter = 0; m_current_iter < max_iterations; ++m_current_iter)
    {
        // 每一轮迭代前，清理上一轮产生的 Request/Grant/Accept 信号
        // 但保留 m_input_occupied 状态，确保已配对的不再参加后续迭代
        std::fill(m_request.begin(), m_request.end(), false);
        std::fill(m_grant.begin(), m_grant.end(), false);
        std::fill(m_accept.begin(), m_accept.end(), false);

        send_request();       // 第一阶段
        do_grant();           // 第二阶段
        do_accept();          // 第三阶段
        update_priority_ptr(); // 第四阶段
    }
}

void islip::send_request()
{
    for (int i = 0; i < m_num_port; i++)
    {
        // 如果输入端口 i 在之前的迭代中已匹配，这轮不再发请求
        if (m_input_occupied[i]) continue;

        for (int j = 0; j < m_num_port; j++)
        {
            // 只有当输出 j 也没被占用，且 VOQ 中有数据时，才发请求
            if (!m_output_occupied[j] && m_ql.at(i * m_num_port + j))
            {
                m_request.at(i * m_num_port + j) = true;
            }
        }
    }
}

void islip::do_grant()
{
    for (int j = 0; j < m_num_port; j++)
    {
        // 已经配对的输出端口不再响应任何请求
        if (m_output_occupied[j]) continue;

        int start_i = m_gi.at(j);
        int i = start_i;
        do 
        {
            if (m_request.at(i * m_num_port + j))
            {
                m_grant.at(i * m_num_port + j) = true;
                break; // 输出 j 授权给第一个扫到的输入 i
            }
            i = (i + 1) % m_num_port;
        } while (i != start_i);
    }
}

void islip::do_accept()
{
    for (int i = 0; i < m_num_port; i++)
    {
        // 已经配对的输入端口不再接受新的授权
        if (m_input_occupied[i]) continue;

        int start_j = m_ai.at(i);
        int j = start_j;
        do 
        {
            if (m_grant.at(i * m_num_port + j))
            {
                m_accept.at(i * m_num_port + j) = true;
                
                // 关键：一旦接受，立即标记该输入和输出为“已占用”
                // 这保证了本时钟周期内物理链路不再被分配给他人 (Speedup=1)
                m_input_occupied[i] = true;
                m_output_occupied[j] = true;
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
                // iSLIP 指针更新规则：
                // 只有在第一轮迭代 (Iteration 0) 匹配成功的才移动指针
                // 这样可以打破同步，保持调度的公平性和高吞吐
                if (m_current_iter == 0)
                {
                    m_gi.at(j) = (i + 1) % m_num_port;
                    m_ai.at(i) = (j + 1) % m_num_port;
                }
                
                // 每一轮匹配到的结果都要记录，最终输出给 Crossbar
                sch_result.emplace_back(std::make_pair(i, j));
            }
        }
    }
}