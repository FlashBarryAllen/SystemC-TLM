/* implement diagonal propagation arbiter */
#ifndef DPA_H
#define DPA_H

#include <vector>

class arbiter_cell; // Forward declaration

class dpa_scheduler {
public:
    dpa_scheduler(int input_port, int output_port);
    void init();
    void request(int input_port, int output_port);
    void arbitration();
    int has_granted(int input_port, int output_port);
    int is_valid_port(int port) const;

public:
    int m_ptr;
    std::vector<int> m_row_mask;
    std::vector<int> m_col_mask;
    int m_num_ports;
    std::vector<std::vector<arbiter_cell*>> m_arbiter_cells;
    std::vector<std::vector<int>> m_requests;
    std::vector<std::vector<int>> m_grants;
    std::vector<std::vector<int>> m_dpa_map;
    std::vector<std::pair<int, int>> sch_result;
};

#endif // DPA_H