#include "islip_wrr.h"

islip_wrr::islip_wrr(int input_num, int output_num, int iterations, vector<vector<int>> weights)
    : m_input_num(input_num), m_output_num(output_num), m_iterations(iterations), m_weights(weights)
{
    m_g_ptr.resize(m_output_num, 0);
    m_a_ptr.resize(m_input_num, 0);
    m_voq.resize(m_input_num, vector<int>(m_output_num, 0));
    m_current_quota = m_weights; // 初始配额等于权重

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

        // 尝试寻找有 VOQ 且有配额的输入
        do {
            if (!m_input_occupied[i] && m_voq[i][j] > 0 && m_current_quota[i][j] > 0) {
                m_grants[j][i] = 1;
                found = true;
                break;
            }
            i = (i + 1) % m_input_num;
        } while (i != start_i);

        // 如果没找到，说明可能配额都用完了，重置该输出端口的配额并重新扫描一次
        if (!found) {
            for (int k = 0; k < m_input_num; k++) m_current_quota[k][j] = m_weights[k][j];
            
            i = start_i;
            do {
                if (!m_input_occupied[i] && m_voq[i][j] > 0) { // 此时只要有包就给 grant
                    m_grants[j][i] = 1;
                    found = true;
                    break;
                }
                i = (i + 1) % m_input_num;
            } while (i != start_i);
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
                m_current_quota[i][j]--; // 关键：消耗配额
                
                m_input_occupied[i] = true;
                m_output_occupied[j] = true;

                if (m_iter_cnt == 0) {
                    m_g_ptr[j] = (i + 1) % m_input_num;
                    m_a_ptr[i] = (j + 1) % m_output_num;
                }
                cout << "Scheduled: In " << i << " -> Out " << j << " (Quota left: " << m_current_quota[i][j] << ")" << " (iSLIP_wrr Success)" << endl;
            }
        }
    }
}

void islip_wrr::arbitration() {
    reset_iteration();

    for (m_iter_cnt = 0; m_iter_cnt < m_iterations; m_iter_cnt++) {
        do_grant();
        do_accept();
        update_pointers();
    }
}

void islip_wrr::reset_iteration() {
    m_iter_cnt = 0;
    fill(m_input_occupied.begin(), m_input_occupied.end(), false);
    fill(m_output_occupied.begin(), m_output_occupied.end(), false);
    for(int j=0; j<m_output_num; j++) fill(m_grants[j].begin(), m_grants[j].end(), 0);
    for(int i=0; i<m_input_num; i++) fill(m_accepts[i].begin(), m_accepts[i].end(), 0);
}