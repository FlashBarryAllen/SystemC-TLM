#ifndef RRM_H
#define RRM_H

#include <vector>
using namespace std;

class rrm
{
public:
    rrm(int input_num, int output_num, int iterations = 1, int voq_size = 4);
    ~rrm();

public:
    void init_priority_ptr();
    void request(int input, int output);
    void arbitration();
    vector<pair<int, int>> get_sch_result();

private:
    void do_grant();
    void do_accept();
    void update_pointers();
    void reset_arbiter(); 
    void reset_iteration();

private:
    int m_input_num;
    int m_output_num;
    int m_voq_size;
    int m_viq_size;
    int m_iterations;
    int m_iter_cnt;
    vector<int> m_g_ptr;
    vector<int> m_a_ptr;
    vector<vector<int>> m_voq;
    vector<vector<int>> m_viq;
    vector<vector<int>> m_grants;
    vector<vector<int>> m_accepts;
    vector<bool> m_input_occupied;
    vector<bool> m_output_occupied;
    vector<pair<int, int>> sch_result;
};

#endif // RRM_H