#ifndef ISLIP_THRESHOLD_H
#define ISLIP_THRESHOLD_H

#include <vector>
#include <iostream>
#include <string>

using namespace std;

class islip_threshold {
public:
    islip_threshold(int input_num, int output_num, int iterations, int voq_size, int threshold);
    ~islip_threshold();

    void request(int input, int output);
    void arbitration();
    vector<pair<int, int>> get_sch_result();

protected:
    void do_grant();
    void do_accept();
    void update_pointers();
    void reset();

    int m_input_num;
    int m_output_num;
    int m_iterations;
    int m_iter_cnt;
    int m_voq_size;
    int m_threshold; // T-SLIP 特有的阈值参数

    vector<int> m_g_ptr; // Grant pointers
    vector<int> m_a_ptr; // Accept pointers

    vector<vector<int>> m_grants;
    vector<vector<int>> m_accepts;
    vector<vector<int>> m_voq;

    vector<bool> m_input_occupied;
    vector<bool> m_output_occupied;

    vector<pair<int, int>> sch_result;
};

#endif