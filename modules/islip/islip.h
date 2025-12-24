#ifndef ISLIP_H
#define ISLIP_H

#include <stdlib.h>
#include <vector>
#include <deque>

class islip
{
public:
    islip(int input_num, int output_num);
    ~islip();

public:
    void init_priority_ptr();
    void set_ql(int i, int j);
    void islip_sch(int max_iterations = 1, int speedup = 1);
    void init();
    void send_request(int speedup);
    void do_grant(int speedup);
    void do_accept(int speedup);
    void update_priority_ptr();

public:
    int m_num_port;
    std::vector<int>  m_gi;
    std::vector<int>  m_ai;
    std::vector<int>  m_ql;
    std::vector<bool> m_accept;
    std::vector<bool> m_request;
    std::vector<bool> m_grant;
    std::vector<std::pair<int, int>> sch_result;

    std::vector<int> m_shadow_ql;
    std::vector<int> m_input_match_count, m_output_match_count;
    int m_current_iter;
};

#endif