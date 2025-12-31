#include "islip_threshold.h"

islip_threshold::islip_threshold(int input_num, int output_num, int iterations, int voq_size, int threshold)
    : m_input_num(input_num), m_output_num(output_num), m_iterations(iterations), 
      m_voq_size(voq_size), m_threshold(threshold) 
{
    m_iter_cnt = 0;
    m_g_ptr.resize(m_output_num, 0);
    m_a_ptr.resize(m_input_num, 0);
    m_grants.resize(m_output_num, vector<int>(m_input_num, 0));
    m_accepts.resize(m_input_num, vector<int>(m_output_num, 0));
    m_input_occupied.resize(m_input_num, false);
    m_output_occupied.resize(m_output_num, false);
    m_voq.resize(m_input_num, vector<int>(m_output_num, 0));
}

islip_threshold::~islip_threshold() {}

void islip_threshold::request(int input, int output) {
    if (input >= 0 && input < m_input_num && output >= 0 && output < m_output_num) {
        if (m_voq[input][output] < m_voq_size) {
            m_voq[input][output]++;
        } else {
            cout << "[Drop] VOQ " << input << "->" << output << " full." << endl;
        }
    }
}

void islip_threshold::arbitration() {
    reset_arbiter();
    for (m_iter_cnt = 0; m_iter_cnt < m_iterations; m_iter_cnt++) {
        do_grant();
        do_accept();
        update_pointers();
        reset_iteration();
    }
}

// T-SLIP 核心修改：双优先级授权逻辑
void islip_threshold::do_grant() {
    for (int output = 0; output < m_output_num; output++) {
        if (m_output_occupied[output]) continue;

        int start_input = m_g_ptr[output];
        int input = start_input;
        bool found_high_priority = false;

        // 第一遍扫描：优先处理超过阈值的请求 (Critical Requests)
        do {
            if (!m_input_occupied[input] && m_voq[input][output] >= m_threshold) {
                m_grants[output][input] = 1;
                found_high_priority = true;
                break;
            }
            input = (input + 1) % m_input_num;
        } while (input != start_input);

        // 第二遍扫描：如果没有高优先级请求，则处理普通非空请求
        if (!found_high_priority) {
            input = start_input;
            do {
                if (!m_input_occupied[input] && m_voq[input][output] > 0) {
                    m_grants[output][input] = 1;
                    break;
                }
                input = (input + 1) % m_input_num;
            } while (input != start_input);
        }
    }
}

void islip_threshold::do_accept() {
    for (int input = 0; input < m_input_num; input++) {
        if (m_input_occupied[input]) continue;

        int start_output = m_a_ptr[input];
        int output = start_output;
        do {
            if (!m_output_occupied[output] && m_grants[output][input] == 1) {
                m_accepts[input][output] = 1;
                break;
            }
            output = (output + 1) % m_output_num;
        } while (output != start_output);
    }
}

void islip_threshold::update_pointers() {
    for (int input = 0; input < m_input_num; input++) {
        if (m_input_occupied[input]) continue;
        for (int output = 0; output < m_output_num; output++) {
            if (m_output_occupied[output]) continue;

            if (m_accepts[input][output] == 1) {
                m_input_occupied[input] = true;
                m_output_occupied[output] = true;
                m_voq[input][output]--;

                // 仅在第一轮迭代更新指针以保证公平性
                if (m_iter_cnt == 0) {
                    m_g_ptr[output] = (input + 1) % m_input_num;
                    m_a_ptr[input] = (output + 1) % m_output_num;
                }
                sch_result.emplace_back(make_pair(input, output));
                cout << "[Sch] In:" << input << " -> Out:" << output << " (iSLIP_threshold Success)" << endl;
            }
        }
    }
}

void islip_threshold::reset_arbiter() {
    m_iter_cnt = 0;
    sch_result.clear();
    fill(m_input_occupied.begin(), m_input_occupied.end(), false);
    fill(m_output_occupied.begin(), m_output_occupied.end(), false);
    for(int j=0; j<m_output_num; j++) fill(m_grants[j].begin(), m_grants[j].end(), 0);
    for(int i=0; i<m_input_num; i++) fill(m_accepts[i].begin(), m_accepts[i].end(), 0);
}

void islip_threshold::reset_iteration() {
    for(int j=0; j<m_output_num; j++) fill(m_grants[j].begin(), m_grants[j].end(), 0);
    for(int i=0; i<m_input_num; i++) fill(m_accepts[i].begin(), m_accepts[i].end(), 0);
}

vector<pair<int, int>> islip_threshold::get_sch_result() { return sch_result; }