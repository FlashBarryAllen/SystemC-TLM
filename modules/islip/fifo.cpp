#include "fifo.h"
#include <iostream>

using namespace std;

fifo::fifo(int input_num, int output_num) {
    m_input_num = input_num;
    m_output_num = output_num;
    m_fifos.resize(m_input_num);
    m_g_ptr.resize(m_output_num, 0);
    m_grants.resize(m_output_num, vector<int>(m_input_num, 0));
    m_input_occupied.resize(m_input_num, false);
    m_output_occupied.resize(m_output_num, false);
}

// 模拟数据包到达
void fifo::request(int input, int output) {
    if (input >= 0 && input < m_input_num) {
        m_fifos[input].push(output);
        cout << "[Queue] In " << input << " enqueue packet for Out " << output << endl;
    }
}

void fifo::arbitration() {
    reset();
    // FIFO 调度通常只需要一轮 Grant/Accept 即可，因为输入端只有一个请求
    do_grant();
    update_pointers();
}

void fifo::init_priority_ptr() {
    for (int i = 0; i < m_output_num; i++) {
        m_g_ptr[i] = (i + 1) % m_output_num;
    }
}

void fifo::do_grant() {
    // 遍历每一个输出端口，查看是否有输入端口的“队头”想要发给它
    for (int out = 0; out < m_output_num; out++) {
        int start_in = m_g_ptr[out];
        int in = start_in;

        do {
            // 检查输入端口 FIFO 是否有包，且队头包的目标是否是当前输出端口 out
            if (!m_fifos[in].empty() && m_fifos[in].front() == out) {
                m_grants[out][in] = 1;
                break; // 找到一个匹配的队头，Grant 给它（轮询保证公平）
            }
            in = (in + 1) % m_input_num;
        } while (in != start_in);
    }
}

void fifo::update_pointers() {
    for (int out = 0; out < m_output_num; out++) {
        for (int in = 0; in < m_input_num; in++) {
            if (m_grants[out][in] == 1) {
                // 成功匹配：输入 in 的队头包发往输出 out
                cout << "[Sch] Match: In " << in << " -> Out " << out << " (FIFO Success)" << endl;
                
                // 从 FIFO 队列移除已发送的包
                m_fifos[in].pop();

                // 更新该输出端口的轮询指针，指向下一个输入
                m_g_ptr[out] = (in + 1) % m_input_num;

                // 一个周期内，一个输出只能接收一个，一个输入只能发一个
                // 因为是按 out 遍历且 break，out 已经唯一，只需跳出 in 循环
                break; 
            }
        }
    }
}

void fifo::reset() {
    for (auto& row : m_grants) fill(row.begin(), row.end(), 0);
    fill(m_input_occupied.begin(), m_input_occupied.end(), false);
    fill(m_output_occupied.begin(), m_output_occupied.end(), false);
}