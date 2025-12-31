#include "islip_wrr.h"
#include <iostream>
#include <algorithm>

using namespace std;

islip_wrr::islip_wrr(int input_num, int output_num, int iterations, vector<vector<int>> weights)
    : m_input_num(input_num), m_output_num(output_num), m_iterations(iterations), m_weights(weights)
{
    m_g_ptr.resize(m_output_num, 0);
    m_a_ptr.resize(m_input_num, 0);
    m_voq.resize(m_input_num, vector<int>(m_output_num, 0));
    m_current_quota = m_weights;
    m_current_quota_input = m_weights;

    m_grants.resize(m_output_num, vector<int>(m_input_num, 0));
    m_accepts.resize(m_input_num, vector<int>(m_output_num, 0));
    m_input_occupied.resize(m_input_num, false);
    m_output_occupied.resize(m_output_num, false);
}

void islip_wrr::request(int input, int output) {
    m_voq[input][output]++;
}

void islip_wrr::do_grant() {
    for (int j = 0; j < m_output_num; j++) {
        if (m_output_occupied[j]) continue;

        int start_i = m_g_ptr[j];
        int i = start_i;
        bool found = false;

        // --- 阶段 1: 寻找有包且有配额的输入 ---
        do {
            if (!m_input_occupied[i] && m_voq[i][j] > 0 && m_current_quota[i][j] > 0) {
                m_grants[j][i] = 1;
                found = true;
                break;
            }
            i = (i + 1) % m_input_num;
        } while (i != start_i);

        // --- 阶段 2: 若阶段 1 失败，说明当前活跃输入的配额全用完了，重置并重扫 ---
        if (!found) {
            bool has_traffic = false;
            for (int k = 0; k < m_input_num; k++) {
                m_current_quota[k][j] = m_weights[k][j]; // 重置该输出对应的所有配额
                if (m_voq[k][j] > 0) {
                    has_traffic = true;
                }
            }
            
            if (has_traffic) {
                i = start_i;
                do {
                    if (!m_input_occupied[i] && m_voq[i][j] > 0) {
                        m_grants[j][i] = 1;
                        break;
                    }
                    i = (i + 1) % m_input_num;
                } while (i != start_i);
            }
        }
    }
}

void islip_wrr::do_accept() {
    for (int i = 0; i < m_input_num; i++) {
        if (m_input_occupied[i]) continue;

        int start_j = m_a_ptr[i];
        int j = start_j;
        do {
            if (!m_output_occupied[j] && m_grants[j][i] == 1) {
                m_accepts[i][j] = 1;
                break;
            }
            j = (j + 1) % m_output_num;
        } while (j != start_j);
    }
}

void islip_wrr::update_pointers() {
    for (int i = 0; i < m_input_num; i++) {
        for (int j = 0; j < m_output_num; j++) {
            if (m_accepts[i][j] == 1) {
                m_voq[i][j]--;
                m_current_quota[i][j]--; 
                
                m_input_occupied[i] = true;
                m_output_occupied[j] = true;
                sch_result.emplace_back(make_pair(i, j));

                // 只有在第一次迭代时更新指针
                if (m_iter_cnt == 0) {
                    m_g_ptr[j] = (i + 1) % m_input_num;
                    // 关键修改：只有配额用完或队列空了，才将指针移向下一个
                    //if (m_current_quota[i][j] <= 0 || m_voq[i][j] <= 0) {
                    //    m_g_ptr[j] = (i + 1) % m_input_num;
                    //} else {
                    //    m_g_ptr[j] = i; // 保持指针位置，实现连续权重分配
                    //}
                    
                    // Accept 指针依然采用轮询，确保输入侧的公平
                    m_a_ptr[i] = (j + 1) % m_output_num;
                }
            }
        }
    }
}

void islip_wrr::arbitration() {
    reset_arbiter();

    for (m_iter_cnt = 0; m_iter_cnt < m_iterations; m_iter_cnt++) {
        do_grant();
        do_accept();
        update_pointers();
        reset_iteration();
    }
}

void islip_wrr::reset_arbiter() {
    m_iter_cnt = 0;
    sch_result.clear();
    fill(m_input_occupied.begin(), m_input_occupied.end(), false);
    fill(m_output_occupied.begin(), m_output_occupied.end(), false);
    for(int j=0; j<m_output_num; j++) fill(m_grants[j].begin(), m_grants[j].end(), 0);
    for(int i=0; i<m_input_num; i++) fill(m_accepts[i].begin(), m_accepts[i].end(), 0);
}

void islip_wrr::reset_iteration() {
    for(int j=0; j<m_output_num; j++) fill(m_grants[j].begin(), m_grants[j].end(), 0);
    for(int i=0; i<m_input_num; i++) fill(m_accepts[i].begin(), m_accepts[i].end(), 0);
}

vector<pair<int, int>> islip_wrr::get_sch_result() {
    return sch_result;
}