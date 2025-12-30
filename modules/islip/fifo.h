#ifndef FIFO_H
#define FIFO_H

#include <vector>
#include <queue>

class fifo {
public:
    fifo(int input_num, int output_num);
    void request(int input, int output); // 输入包进入 FIFO 队列
    void arbitration();                  // 仲裁过程
    void init_priority_ptr();            // 初始化指针

private:
    int m_input_num;
    int m_output_num;
    
    // 核心：每个输入端口只有一个队列，存储该包的目标 output_id
    std::vector<std::queue<int>> m_fifos; 
    
    std::vector<int> m_g_ptr; // Grant 指针（轮询）
    std::vector<std::vector<int>> m_grants;
    std::vector<bool> m_input_occupied;
    std::vector<bool> m_output_occupied;

    void do_grant();
    void update_pointers();
    void reset();
};

#endif // FIFO_H