#ifndef PIM_H
#define PIM_H

#include <vector>
#include <utility>
#include <random>

class pim {
public:
    pim(int input_num, int output_num, int iterations = 1, int voq_size = 4);
    ~pim();

    void request(int input, int output);
    void arbitration();
    std::vector<std::pair<int, int>> get_sch_result();

private:
    void do_grant(std::mt19937& g);
    void do_accept(std::mt19937& g);
    void update_pointers(); // 仅保留名称，内部逻辑更新占用状态
    void reset();

    int m_input_num;
    int m_output_num;
    int m_iterations;
    int m_iter_cnt;
    int m_voq_size;

    std::vector<std::vector<int>> m_voq;
    std::vector<std::vector<int>> m_grants;
    std::vector<std::vector<int>> m_accepts;
    std::vector<bool> m_input_occupied;
    std::vector<bool> m_output_occupied;
    std::vector<std::pair<int, int>> sch_result;
};

#endif