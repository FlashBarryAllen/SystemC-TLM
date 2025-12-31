#ifndef ISLIP_SP_H
#define ISLIP_SP_H

#include <vector>
#include <iostream>
#include <tuple>

using namespace std;

class islip_sp {
public:
    // 构造函数增加了 priority_levels 参数
    islip_sp(int input_num, int output_num, int iterations, int voq_size, int priority_levels = 2);
    ~islip_sp();

    void request(int input, int output, int priority = 0);
    void arbitration();

    // 结果包含：<input, output, priority>
    vector<tuple<int, int, int>> get_sch_result();

private:
    void do_grant();
    void do_accept();
    void update_pointers();
    void reset_arbiter(); 
    void reset_iteration();

    int m_input_num;
    int m_output_num;
    int m_iterations;
    int m_iter_cnt;
    int m_voq_size;
    int m_priority_levels;

    vector<int> m_g_ptr;
    vector<int> m_a_ptr;

    vector<vector<int>> m_grants;
    vector<vector<int>> m_accepts;

    vector<bool> m_input_occupied;
    vector<bool> m_output_occupied;

    // m_voq[input][output][priority]
    vector<vector<vector<int>>> m_voq;
    
    // 存储调度成功的三元组：输入, 输出, 优先级
    vector<tuple<int, int, int>> sch_result;
};

#endif