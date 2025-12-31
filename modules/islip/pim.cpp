#include <iostream>
#include <algorithm>
#include "pim.h"

using namespace std;

pim::pim(int input_num, int output_num, int iterations, int voq_size)
{
    m_input_num = input_num;
    m_output_num = output_num;
    m_iterations = iterations;
    m_iter_cnt = 0;
    m_voq_size = voq_size;

    // 清理了 m_g_ptr 和 m_a_ptr 的初始化
    m_grants.resize(m_output_num, vector<int>(m_input_num, 0));
    m_accepts.resize(m_input_num, vector<int>(m_output_num, 0));

    m_input_occupied.resize(m_input_num, false);
    m_output_occupied.resize(m_output_num, false);
    m_voq.resize(m_input_num, vector<int>(m_output_num, 0));
}

pim::~pim() {}

void pim::request(int input, int output)
{
    if (input < 0 || input >= m_input_num || output < 0 || output >= m_output_num) return;
    
    if (m_voq[input][output] < m_voq_size) {
        m_voq[input][output]++;
    } else {
        cout << "VOQ Full: Drop packet at In " << input << " -> Out " << output << endl;
    }
}

void pim::arbitration()
{
   reset_arbiter();

    // 使用静态随机数生成器提高效率
    static std::random_device rd;
    static std::mt19937 g(rd());

    for (m_iter_cnt = 0; m_iter_cnt < m_iterations; m_iter_cnt++)
    {
        do_grant(g);
        do_accept(g);
        update_pointers();
        reset_iteration();
    }
}

void pim::do_grant(std::mt19937& g)
{
    for (int output = 0; output < m_output_num; output++)
    {
        if (m_output_occupied[output]) continue;

        vector<int> candidates;
        for (int input = 0; input < m_input_num; input++)
        {
            // 如果输入端未匹配且有请求
            if (!m_input_occupied[input] && m_voq[input][output] > 0) {
                candidates.push_back(input);
            }
        }

        if (!candidates.empty()) {
            // PIM 核心：随机选择一个 Grant
            uniform_int_distribution<> dis(0, candidates.size() - 1);
            int selected_input = candidates[dis(g)];
            m_grants[output][selected_input] = 1;
        }
    }
}

void pim::do_accept(std::mt19937& g)
{
    for (int input = 0; input < m_input_num; input++)
    {
        if (m_input_occupied[input]) continue;

        vector<int> candidates;
        for (int output = 0; output < m_output_num; output++)
        {
            // 如果输出端未匹配且发出了 Grant
            if (!m_output_occupied[output] && m_grants[output][input] == 1) {
                candidates.push_back(output);
            }
        }

        if (!candidates.empty()) {
            // PIM 核心：随机选择一个 Accept
            uniform_int_distribution<> dis(0, candidates.size() - 1);
            int selected_output = candidates[dis(g)];
            m_accepts[input][selected_output] = 1;
        }
    }
}

void pim::update_pointers()
{
    for (int input = 0; input < m_input_num; input++)
    {
        if (m_input_occupied[input]) continue;

        for (int output = 0; output < m_output_num; output++)
        {
            if (m_output_occupied[output]) continue;

            if (m_accepts[input][output] == 1)
            {
                m_input_occupied[input] = true;
                m_output_occupied[output] = true;
                m_voq[input][output]--;

                sch_result.emplace_back(make_pair(input, output));

                cout << "[Sch] In:" << input << " -> Out:" << output << " (PIM Success)" << endl;
            }
        }
    }
}

void pim::reset_arbiter() {
    m_iter_cnt = 0;
    sch_result.clear();
    fill(m_input_occupied.begin(), m_input_occupied.end(), false);
    fill(m_output_occupied.begin(), m_output_occupied.end(), false);
    for(int j=0; j<m_output_num; j++) fill(m_grants[j].begin(), m_grants[j].end(), 0);
    for(int i=0; i<m_input_num; i++) fill(m_accepts[i].begin(), m_accepts[i].end(), 0);
}

void pim::reset_iteration() {
    for(int j=0; j<m_output_num; j++) fill(m_grants[j].begin(), m_grants[j].end(), 0);
    for(int i=0; i<m_input_num; i++) fill(m_accepts[i].begin(), m_accepts[i].end(), 0);
}