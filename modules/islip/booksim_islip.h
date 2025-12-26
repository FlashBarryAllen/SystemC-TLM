#ifndef BOOKSIM_ISLIP_H
#define BOOKSIM_ISLIP_H

#include <stdlib.h>
#include <vector>
#include <deque>

class islip_booksim
{
public:
    islip_booksim(int input_num, int output_num, int iteratoins = 1);
    ~islip_booksim();

public:
    void init_priority_ptr();
    void set_ql(int i, int j);
    void islip_sch();
    void init();
    void send_request();
    void do_grant();
    void do_accept();
    void update_priority_ptr();

public:
    int m_input_num;
    int m_output_num;
    int m_num_port;
    int m_iterations;
    int m_iter_cnt;
    std::vector<int>  m_gi;
    std::vector<int>  m_ai;
    std::vector<int>  m_ql;
    std::vector<bool> m_accept;
    std::vector<bool> m_request;
    std::vector<bool> m_grant;
    std::vector<bool> m_input_occupied;
    std::vector<bool> m_output_occupied;
    std::vector<std::pair<int, int>> sch_result;
};

#endif