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
    void islip_sch(int max_iterations = 1);
    void init();
    void send_request();
    void do_grant();
    void do_accept();
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

    // 在 islip 类定义中增加：
    std::vector<bool> m_input_occupied;
    std::vector<bool> m_output_occupied;
    int m_current_iter; // 记录当前是第几次迭代
};

#endif