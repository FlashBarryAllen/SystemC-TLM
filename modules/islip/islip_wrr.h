#ifndef ISLIP_WRR_H
#define ISLIP_WRR_H

#include <vector>
#include <utility>
#include <algorithm>

using namespace std;

class islip_wrr {
private:
    int m_input_num;               // 输入端口数
    int m_output_num;              // 输出端口数
    int m_iterations;              // ISLIP迭代次数
    vector<vector<int>> m_weights; // 权重矩阵 [in][out]
    
    // WRR核心：每个(in,out)的剩余配额（按权重初始化）
    vector<vector<int>> m_current_quota;
    // 每个输出端口的总权重（缓存，避免重复计算）
    vector<int> m_total_weight_out;
    
    // ISLIP轮询指针
    vector<int> m_g_ptr;           // 输出授予指针 [out]
    vector<int> m_a_ptr;           // 输入接受指针 [in]
    
    // VOQ队列：[in][out] 队列中的包数
    vector<vector<int>> m_voq;
    // 仲裁过程变量
    vector<vector<int>> m_grants;  // 授予矩阵 [out][in]
    vector<vector<int>> m_accepts; // 接受矩阵 [in][out]
    vector<bool> m_input_occupied; // 输入是否已匹配
    vector<bool> m_output_occupied;// 输出是否已匹配
    int m_iter_cnt;                // 当前迭代次数
    
    // 调度结果
    vector<pair<int, int>> sch_result;

    // 重置单次迭代的授予/接受状态
    void reset_iteration();
    // 重置整个仲裁器状态
    void reset_arbiter();
    // 为输出端口选择权重优先级最高的输入（WRR核心）
    int select_wrr_input(int out);
    // 为输入端口选择权重优先级最高的输出（多输出核心）
    int select_wrr_output(int in);

public:
    // 构造函数
    islip_wrr(int input_num, int output_num, int iterations, vector<vector<int>> weights);
    
    // 发送请求：input向output发送包（VOQ+1）
    void request(int input, int output);
    
    // 执行ISLIP+WRR仲裁
    void arbitration();
    
    // 获取调度结果
    vector<pair<int, int>> get_sch_result();
};

#endif // ISLIP_WRR_H