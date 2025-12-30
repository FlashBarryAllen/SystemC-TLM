#ifndef ISLIP_WRR_H
#define ISLIP_WRR_H

#include <vector>
#include <iostream>

using namespace std;

class islip_wrr {
public:
    // weights 传入每个 (input, output) 的权重
    islip_wrr(int input_num, int output_num, int iterations, vector<vector<int>> weights);
    
    void request(int input, int output);
    void arbitration();
    void reset_iteration(); // 每时隙开始前重置临时状态

private:
    void do_grant();
    void do_accept();
    void update_pointers();

    int m_input_num, m_output_num, m_iterations, m_iter_cnt;
    
    vector<int> m_g_ptr; // Grant 指针
    vector<int> m_a_ptr; // Accept 指针

    vector<vector<int>> m_voq;           // 队列计数
    vector<vector<int>> m_weights;       // 固定权重配置
    vector<vector<int>> m_current_quota; // 当前剩余配额

    vector<vector<int>> m_grants;
    vector<vector<int>> m_accepts;
    vector<bool> m_input_occupied;
    vector<bool> m_output_occupied;
};

#endif