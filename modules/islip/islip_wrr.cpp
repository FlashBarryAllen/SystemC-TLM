#include "islip_wrr.h"
#include <iostream>
#include <algorithm>

using namespace std;

islip_wrr::islip_wrr(int input_num, int output_num, int iterations, vector<vector<int>> weights)
    : m_input_num(input_num), m_output_num(output_num), m_iterations(iterations), m_weights(weights)
{
    // 初始化轮询指针（从0开始）
    m_g_ptr.resize(m_output_num, 0);
    m_a_ptr.resize(m_input_num, 0);
    
    // 初始化VOQ队列
    m_voq.resize(m_input_num, vector<int>(m_output_num, 0));
    
    // WRR配额初始化：按权重赋值
    m_current_quota = m_weights;
    
    // 计算每个输出端口的总权重（用于配额重置）
    m_total_weight_out.resize(m_output_num, 0);
    for (int out = 0; out < m_output_num; out++) {
        int sum = 0;
        for (int in = 0; in < m_input_num; in++) {
            sum += m_weights[in][out];
        }
        m_total_weight_out[out] = sum;
    }
    
    // 仲裁过程变量初始化
    m_grants.resize(m_output_num, vector<int>(m_input_num, 0));
    m_accepts.resize(m_input_num, vector<int>(m_output_num, 0));
    m_input_occupied.resize(m_input_num, false);
    m_output_occupied.resize(m_output_num, false);
    m_iter_cnt = 0;
}

// WRR核心：为输出端口选择优先级最高的输入（剩余配额/权重 比例最小）
int islip_wrr::select_wrr_input(int out) {
    int best_in = -1;
    double min_ratio = 1e9;
    int start_i = m_g_ptr[out]; // 从当前轮询指针开始，保证轮询公平性
    
    // 第一轮：找剩余配额>0且有包的输入（优先消耗配额）
    for (int step = 0; step < m_input_num; step++) {
        int i = (start_i + step) % m_input_num;
        if (m_input_occupied[i] || m_voq[i][out] == 0) continue;
        
        // 剩余配额>0时，优先级 = 已消耗配额 / 权重 = (权重-剩余配额)/权重
        if (m_current_quota[i][out] > 0) {
            double ratio = (double)(m_weights[i][out] - m_current_quota[i][out]) / m_weights[i][out];
            if (ratio < min_ratio) {
                min_ratio = ratio;
                best_in = i;
            }
        }
    }
    
    // 第二轮：仅重置当前输入的配额（而非全局），精细化控制
    if (best_in == -1) {
        bool has_traffic = false;
        for (int in = 0; in < m_input_num; in++) {
            if (m_voq[in][out] > 0) {
                has_traffic = true;
                m_current_quota[in][out] = m_weights[in][out]; // 仅重置有流量的输入
            }
        }
        if (has_traffic) {
            // 重新选择（此时配额已重置）
            for (int step = 0; step < m_input_num; step++) {
                int i = (start_i + step) % m_input_num;
                if (!m_input_occupied[i] && m_voq[i][out] > 0) {
                    best_in = i;
                    break;
                }
            }
        }
    }
    
    return best_in;
}

// 输入端口选择输出时的WRR优先级：剩余配额/权重 比例最小
int islip_wrr::select_wrr_output(int in) {
    int best_out = -1;
    double min_ratio = 1e9;
    int start_out = m_a_ptr[in]; // 从当前轮询指针开始，保证公平性
    
    // 第一轮：找有授予且有包的输出（优先消耗配额）
    for (int step = 0; step < m_output_num; step++) {
        int out = (start_out + step) % m_output_num;
        if (m_output_occupied[out] || m_voq[in][out] == 0 || m_grants[out][in] != 1) {
            continue;
        }
        
        // 剩余配额>0时，优先级 = (权重-剩余配额)/权重
        if (m_current_quota[in][out] > 0) {
            double ratio = (double)(m_weights[in][out] - m_current_quota[in][out]) / m_weights[in][out];
            if (ratio < min_ratio) {
                min_ratio = ratio;
                best_out = out;
            }
        }
    }
    
    // 第二轮：若没有剩余配额，重置该输入的所有配额后重新选择
    if (best_out == -1) {
        bool has_traffic = false;
        for (int out = 0; out < m_output_num; out++) {
            if (m_voq[in][out] > 0 && m_grants[out][in] == 1) {
                has_traffic = true;
                break;
            }
        }
        if (has_traffic) {
            // 重置该输入的所有输出配额
            for (int out = 0; out < m_output_num; out++) {
                m_current_quota[in][out] = m_weights[in][out];
            }
            // 重新选择输出
            for (int step = 0; step < m_output_num; step++) {
                int out = (start_out + step) % m_output_num;
                if (!m_output_occupied[out] && m_voq[in][out] > 0 && m_grants[out][in] == 1) {
                    best_out = out;
                    break;
                }
            }
        }
    }
    
    return best_out;
}

void islip_wrr::request(int input, int output) {
    if (input >= 0 && input < m_input_num && output >= 0 && output < m_output_num) {
        m_voq[input][output]++;
    }
}

void islip_wrr::reset_arbiter() {
    m_iter_cnt = 0;
    sch_result.clear();
    fill(m_input_occupied.begin(), m_input_occupied.end(), false);
    fill(m_output_occupied.begin(), m_output_occupied.end(), false);
    for (auto& row : m_grants) fill(row.begin(), row.end(), 0);
    for (auto& row : m_accepts) fill(row.begin(), row.end(), 0);
}

void islip_wrr::reset_iteration() {
    for (auto& row : m_grants) fill(row.begin(), row.end(), 0);
    for (auto& row : m_accepts) fill(row.begin(), row.end(), 0);
}

void islip_wrr::arbitration() {
    reset_arbiter(); // 重置仲裁器状态
    
    // 优化：多输出场景下迭代次数设为1（避免多轮迭代破坏权重）
    int actual_iter = min(m_iterations, 1); 
    for (m_iter_cnt = 0; m_iter_cnt < actual_iter; m_iter_cnt++) {
        // 1. 授予阶段：输出端口按WRR选择输入
        for (int out = 0; out < m_output_num; out++) {
            if (m_output_occupied[out]) continue;
            
            int selected_in = select_wrr_input(out);
            if (selected_in != -1) {
                m_grants[out][selected_in] = 1; // 授予该输入
            }
        }
        
        // 2. 接受阶段：输入端口按WRR选择输出（核心修改：替代原轮询逻辑）
        for (int in = 0; in < m_input_num; in++) {
            if (m_input_occupied[in]) continue;
            
            // 多输出场景：输入优先选择权重比例最优的输出
            int selected_out = select_wrr_output(in);
            if (selected_out != -1 && m_grants[selected_out][in] == 1) {
                m_accepts[in][selected_out] = 1; // 接受该输出
            }
        }
        
        // 3. 更新状态：消耗配额、更新指针、标记占用
        for (int in = 0; in < m_input_num; in++) {
            for (int out = 0; out < m_output_num; out++) {
                if (m_accepts[in][out] == 1) {
                    // 消耗VOQ包和配额
                    m_voq[in][out]--;
                    m_current_quota[in][out]--;
                    
                    // 标记输入/输出为已占用
                    m_input_occupied[in] = true;
                    m_output_occupied[out] = true;
                    
                    // 记录调度结果
                    sch_result.emplace_back(in, out);
                    
                    // 更新轮询指针（下一次从当前输入/输出的下一个开始）
                    m_g_ptr[out] = (in + 1) % m_input_num;
                    m_a_ptr[in] = (out + 1) % m_output_num;
                }
            }
        }
        
        reset_iteration(); // 重置本轮迭代的授予/接受状态
    }
}

vector<pair<int, int>> islip_wrr::get_sch_result() {
    return sch_result;
}