#include "islip_ycl.h"

islip_ycl::islip_ycl(int input_num, int output_num, int iterations, int voq_size, bool islip_mode, int priority_levels)
    : m_islip_mode(islip_mode), m_priority_levels(priority_levels)
{
    m_input_num = input_num;
    m_output_num = output_num;
    m_iterations = iterations;
    m_voq_size = voq_size;
    m_iter_cnt = 0;

    m_g_ptr.resize(m_output_num, 0);
    m_a_ptr.resize(m_input_num, 0);

    m_grants.resize(m_output_num, vector<int>(m_input_num, 0));
    m_accepts.resize(m_input_num, vector<int>(m_output_num, 0));

    m_input_occupied.resize(m_input_num, false);
    m_output_occupied.resize(m_output_num, false);

    // 初始化三维 VOQ: [Input][Output][Priority]
    m_voq.resize(m_input_num, vector<vector<int>>(m_output_num, vector<int>(m_priority_levels, 0)));
}

islip_ycl::~islip_ycl() {}

void islip_ycl::request(int input, int output, int priority)
{
    if (input < 0 || input >= m_input_num || output < 0 || output >= m_output_num || priority >= m_priority_levels) {
        cerr << "Error: Index out of range" << endl;
        return;
    }
    
    if (m_voq[input][output][priority] >= m_voq_size) {
        cout << "VOQ Full: Input " << input << " Output " << output << " Prio " << priority << " dropped." << endl;
        return;
    }

    m_voq[input][output][priority]++;
}

void islip_ycl::arbitration()
{
    reset();
    
    for (m_iter_cnt = 0; m_iter_cnt < m_iterations; m_iter_cnt++)
    {
        do_grant();
        do_accept();
        update_pointers();
    }
}

void islip_ycl::do_grant()
{
    for (int output = 0; output < m_output_num; output++)
    {
        if (m_output_occupied[output]) continue;

        bool granted = false;
        // 严格优先级搜索：先检查所有输入的 Priority 0，再检查 Priority 1...
        for (int p = 0; p < m_priority_levels && !granted; p++)
        {
            int start_input = m_g_ptr[output];
            int input = start_input;

            do {
                if (!m_input_occupied[input] && m_voq[input][output][p] > 0)
                {
                    m_grants[output][input] = 1;
                    if (!m_islip_mode) {
                        m_g_ptr[output] = (input + 1) % m_input_num;
                    }
                    granted = true;
                    break;
                }
                input = (input + 1) % m_input_num;
            } while (input != start_input);
        }
    }
}

void islip_ycl::do_accept()
{
    for (int input = 0; input < m_input_num; input++)
    {
        if (m_input_occupied[input]) continue;

        int start_output = m_a_ptr[input];
        int output = start_output;

        do {
            if (!m_output_occupied[output] && m_grants[output][input] == 1)
            {
                m_accepts[input][output] = 1;
                if (!m_islip_mode) {
                    m_a_ptr[input] = (output + 1) % m_output_num;
                }
                break;
            }
            output = (output + 1) % m_output_num;
        } while (output != start_output);
    }
}

void islip_ycl::update_pointers()
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

                // 确定被选中的优先级（扣除最高优先级的包）
                int selected_prio = -1;
                for (int p = 0; p < m_priority_levels; p++) {
                    if (m_voq[input][output][p] > 0) {
                        m_voq[input][output][p]--;
                        selected_prio = p;
                        break;
                    }
                }

                if (m_iter_cnt == 0 && m_islip_mode) {
                    m_g_ptr[output] = (input + 1) % m_input_num;
                    m_a_ptr[input] = (output + 1) % m_output_num;
                }

                sch_result.emplace_back(make_tuple(input, output, selected_prio));
                cout << "[Sch] In:" << input << " -> Out:" << output << " (Prio:" << selected_prio << ")" << endl;
            }
        }
    }
}

vector<tuple<int, int, int>> islip_ycl::get_sch_result() { return sch_result; }

void islip_ycl::reset()
{
    m_iter_cnt = 0;
    sch_result.clear();
    for (int i = 0; i < m_input_num; i++) {
        m_input_occupied[i] = false;
        fill(m_accepts[i].begin(), m_accepts[i].end(), 0);
    }
    for (int j = 0; j < m_output_num; j++) {
        m_output_occupied[j] = false;
        fill(m_grants[j].begin(), m_grants[j].end(), 0);
    }
}