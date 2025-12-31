#include "test.h"
#include <iomanip>
#include <map>

Qos::Qos()
{
    vc.resize(VIRTURL_CHANNEL_NUM);
}

Qos::~Qos()
{
}

int Qos::in(int priority, int data)
{
    if (priority < 0 || priority >= VIRTURL_CHANNEL_NUM)
    {
        std::cerr << "Error: Priority out of range" << std::endl;
        return -1;
    }

    vc[priority].push_back(data);
    cnt++;
    return 0;
}

int Qos::sp_sch()
{
    while (cnt > 0)
    {
        for (int i = VIRTURL_CHANNEL_NUM - 1; i >= 0; i--)
        {
            if (!vc[i].empty())
            {
                std::cout << "Processing data from channel " << i << ": " << vc[i].front() << std::endl;
                vc[i].pop_front();
                cnt--;
                break;
            }
        }
    }

    return 0;
}

int Qos::rr_sch()
{
    while (cnt > 0)
    {
        time++;
        while (last_sch < VIRTURL_CHANNEL_NUM)
        {
            if (!vc[last_sch].empty())
            {
                std::cout << "time: " << time << ", Processing data from channel " << last_sch << ": " << vc[last_sch].front() << std::endl;
                vc[last_sch].pop_front();
                cnt--;
                last_sch++;
                if (last_sch >= VIRTURL_CHANNEL_NUM)
                {
                    last_sch = 0;
                }
                break;
            }

            last_sch++;
            if (last_sch >= VIRTURL_CHANNEL_NUM)
            {
                last_sch = 0;
            }
        }
    }

    return 0;
}

void TEST_islip() {
    islip* myislip = new islip(4, 4);
    myislip->init_priority_ptr();

    while (1) {
        myislip->init();

        for (auto i = 0; i < 4; i++) {
            myislip->set_ql(i, 0);
            myislip->set_ql(i, 1);
            myislip->set_ql(i, 2);
            myislip->set_ql(i, 3);
        }

        myislip->islip_sch(2,1,false);

        auto ret = myislip->sch_result;

        for (auto i = 0; i < ret.size(); i++) {
            auto in = ret[i].first;
            auto out = ret[i].second;

            std::cout << "(" << in << ", " << out << ")" << std::endl;
        }

        std::cout << std::endl;
    }

    return;
}

void TEST_islip_lonely_pair() {
    // 实例化一个 4x4 的调度器
    islip* myislip = new islip(4, 4);
    myislip->init_priority_ptr(); // 初始指针 g0=0, g1=1...

    myislip->init();

    // --- 构造冲突场景 ---
    
    // 输入 0 请求 输出 0
    myislip->set_ql(0, 0); 
    
    // 输入 1 同时请求 输出 0 和 输出 2
    // 注意：输出 0 会有竞争，而输出 2 是完全空闲的
    myislip->set_ql(1, 0); 
    myislip->set_ql(1, 2); 

    // 输入 2 请求 输出 1
    myislip->set_ql(2, 1);

    // 执行调度（当前代码只执行 1 次迭代）
    myislip->islip_sch();

    auto ret = myislip->sch_result;

    std::cout << "--- 匹配结果 ---" << std::endl;
    for (auto i = 0; i < ret.size(); i++) {
        std::cout << "输入 " << ret[i].first << " -> 输出 " << ret[i].second << std::endl;
    }

    /* 预期输出：
     输入 0 -> 输出 0
     输入 2 -> 输出 1
     
     分析：
     1. 输出 0 收到输入 0 和 1 的请求，根据指针优先选了 0。
     2. 输入 1 请求失败，虽然输出 2 是空闲的且输入 1 想去，但因为只有 1 次迭代，
        输入 1 没机会在这一周期再找输出 2 握手了。
     3. 输入 1 和 输出 2 成了“孤男寡女”。
    */
}

void TEST_islip_real_lonely() {
    islip* myislip = new islip(4, 4);
    myislip->init_priority_ptr(); 
    myislip->init();

    // 1. 让输出 0 别无选择，只能投给输入 1
    myislip->set_ql(1, 0); 
    
    // 2. 让输出 2 也别无选择，只能投给输入 1
    myislip->set_ql(1, 2); 

    // 3. 让输出 1 有人要，不参与干扰
    myislip->set_ql(2, 1);

    // 执行调度
    myislip->islip_sch();

    auto ret = myislip->sch_result;

    std::cout << "--- 匹配结果 ---" << std::endl;
    for (auto i = 0; i < ret.size(); i++) {
        std::cout << "输入 " << ret[i].first << " -> 输出 " << ret[i].second << std::endl;
    }

    /* 逻辑推演：
       - do_grant: 
         输出 0 授权给输入 1 (Grant[1,0]=1)
         输出 2 授权给输入 1 (Grant[1,2]=1)
       - do_accept:
         输入 1 拿到了两个授权。如果指针 m_ai[1] 指向 0，它会 Accept 输出 0。
       - 结果：
         输出 2 被输入 1 拒绝了。
         此时：输入 1 去往输出 2 的需求还在，输出 2 也是空闲的。
         这对“孤男寡女”就此产生！
    */
}

void TEST_islip_lonely() {
    islip* myislip = new islip(4, 4);
    myislip->init_priority_ptr(); 
    myislip->init();

    // --- 构造“二选一”冲突 ---
    
    // 情况：输出 0 和 输出 2 都只有输入 1 这一个追求者
    myislip->set_ql(1, 0); 
    myislip->set_ql(1, 2); 

    // 输入 2 随便去一个不相关的输出 1
    myislip->set_ql(2, 1);

    myislip->islip_sch();

    auto ret = myislip->sch_result;

    std::cout << "--- 只有 1 次迭代的结果 ---" << std::endl;
    for (auto& p : ret) {
        std::cout << "输入 " << p.first << " -> 输出 " << p.second << std::endl;
    }
}

void TEST_islip_final_showdown() {
    islip* myislip = new islip(4, 4);
    myislip->init_priority_ptr(); // 假设 ai 指针初始指向 0
    myislip->init();

    // 1. 让 输入 1 成为“焦点”：同时请求输出 0 和 输出 2
    myislip->set_ql(1, 0); 
    myislip->set_ql(1, 1); 
    myislip->set_ql(1, 2);
    myislip->set_ql(1, 3); 

    // 2. 关键：确保输出 0 和 输出 2 都没有其他追求者，只能授权给输入 1
    // 不要调用 set_ql(0, 0) 或 set_ql(3, 2)

    // 3. 让 输入 2 去请求一个无关的 输出 1，保证系统不全空
    myislip->set_ql(2, 2);
    myislip->set_ql(3, 3);

    //myislip->islip_sch(1);
    myislip->islip_sch(2);
    //myislip->islip_sch(3);
    //myislip->islip_sch(4);

    //myislip->islip_sch(1, 2);
    //myislip->islip_sch(2, 2);
    //myislip->islip_sch(3, 2);
    //myislip->islip_sch(4, 2);

    auto ret = myislip->sch_result;
    std::cout << "--- 1次迭代的最终对决结果 ---" << std::endl;
    for (auto& p : ret) {
        std::cout << "输入 " << p.first << " -> 输出 " << p.second << std::endl;
    }
}

void TEST_Shadow_Preemption() {
    islip* myislip = new islip(4, 4, 2); 
    myislip->init();
    myislip->m_gi[0] = 0; // 确保输出 0 的指针指向输入 0，让输入 0 优先

    // 高优先级冲突
    myislip->set_ql(0, 0, 0); // In 0 -> Out 0 (High)
    myislip->set_ql(1, 0, 0); // In 1 -> Out 0 (High) - 会输给 In 0

    // 低优先级备选
    myislip->set_ql(1, 2, 1); // In 1 -> Out 2 (Low)

    myislip->islip_sch(4, 1); // Speedup = 1

    // 预期结果：
    // 输入 0 -> 输出 0 (High 胜出)
    // 输入 1 -> 输出 2 (High 输了，但 Low 捡漏成功)
    auto ret = myislip->sch_result;
    std::cout << "--- 抢占与捡漏测试 ---" << std::endl;
    for (auto& p : ret) std::cout << "输入 " << p.first << " -> 输出 " << p.second << std::endl;
}

void TEST_Quota_Exhaustion() {
    islip* myislip = new islip(4, 4, 3); // 3层优先级
    myislip->init();

    myislip->set_ql(0, 0, 0); // Prio 0 (High)
    myislip->set_ql(0, 1, 1); // Prio 1 (Mid)
    myislip->set_ql(0, 2, 2); // Prio 2 (Low)

    myislip->islip_sch(4, 2); // Speedup = 2

    // 预期结果：
    // 输入 0 -> 输出 0
    // 输入 0 -> 输出 1
    // (输出 2 不应该出现，因为 Speedup 额度已用完)
    auto ret = myislip->sch_result;
    std::cout << "--- 额度耗尽测试 (S=2) ---" << std::endl;
    for (auto& p : ret) std::cout << "输入 " << p.first << " -> 输出 " << p.second << std::endl;
}

void TEST_Full_Load_Suppression() {
    islip* myislip = new islip(4, 4, 2);
    myislip->init();

    // 所有人都在抢 Out 0 (High Prio)
    for(int i=0; i<4; i++) myislip->set_ql(i, 0, 0);

    // In 0 还有一个低优先级的备选 Out 1
    myislip->set_ql(0, 1, 1);

    myislip->islip_sch(2, 2); // 1次迭代，S=2

    // 预期结果分析：
    // 如果 In 0 赢了 Out 0，结果只有 (0,0)。
    // 如果 In 0 输了 Out 0 (比如 In 1 赢了)，结果应为 (1,0) 和 (0,1)。
    // 因为 In 0 输了之后 match_count 仍为 0，它可以在 P=1 时去拿 Out 1。
    auto ret = myislip->sch_result;
    std::cout << "--- 全负载压制测试 ---" << std::endl;
    for (auto& p : ret) std::cout << "输入 " << p.first << " -> 输出 " << p.second << std::endl;
}

void TEST_max_match() {
    islip* myislip = new islip(4, 4);
    myislip->init();

    myislip->set_ql(0, 0);
    myislip->set_ql(0, 1);
    myislip->set_ql(2, 1);
    myislip->set_ql(2, 3);
    myislip->set_ql(3, 3);

    myislip->islip_sch(3, 1, false);

    auto ret = myislip->sch_result;
    std::cout << "--- max_match测试 ---" << std::endl;
    for (auto& p : ret) std::cout << "输入 " << p.first << " -> 输出 " << p.second << std::endl;
}

void TEST_booksim_max_match() {
    islip_booksim* myislip = new islip_booksim(4, 4, 4, true);
    myislip->init();

    myislip->set_ql(0, 0);
    myislip->set_ql(0, 1);
    myislip->set_ql(2, 1);
    myislip->set_ql(2, 3);
    myislip->set_ql(3, 3);

    myislip->islip_sch();

    auto ret = myislip->sch_result;
    std::cout << "--- booksim_max_match测试 ---" << std::endl;
    for (auto& p : ret) std::cout << "输入 " << p.first << " -> 输出 " << p.second << std::endl;
}

void TEST_starvation() {
    islip_booksim* myislip = new islip_booksim(3, 3, 2);
    
    while (1) {
        myislip->init();
        myislip->set_ql(0, 0);
        myislip->set_ql(0, 1);
        myislip->set_ql(0, 2);
        myislip->set_ql(1, 1);
        myislip->set_ql(2, 1);

        myislip->islip_sch();

        auto ret = myislip->sch_result;
        std::cout << "--- starvation测试 ---" << std::endl;
        for (auto& p : ret) std::cout << "输入 " << p.first << " -> 输出 " << p.second << std::endl;
    }
}

void TEST_fifo() {
    int input_num  = 2;
    int output_num = 2;
    fifo* myislip = new fifo(input_num, output_num);
    myislip->init_priority_ptr();

    cout << "---------------------------------------------------------" << endl;
    cout << "--- test fifo arbitration ---" << endl;
    int cell_times = 4;
    for (int cell = 0; cell < cell_times; ++cell) {
        cout << "----------------------------" << endl;
        cout << "--- cell " << cell << " ---" << endl;

        myislip->request(0, 0);
        myislip->request(0, 1);
        myislip->request(1, 0);
        myislip->request(1, 1);
        myislip->arbitration();

        cout << "----------------------------" << endl;
    }

    delete myislip;
}

void TEST_pim() {
    int input_num  = 2;
    int output_num = 2;
    pim* mypim = new pim(input_num, output_num);

    cout << "---------------------------------------------------------" << endl;
    cout << "--- test pim arbitration ---" << endl;
    int cell_times = 4;
    for (int cell = 0; cell < cell_times; ++cell) {
        cout << "----------------------------" << endl;
        cout << "--- cell " << cell << " ---" << endl;
        
        mypim->request(0, 0);
        mypim->request(0, 1);
        mypim->request(1, 0);
        mypim->request(1, 1);
        mypim->arbitration();

        cout << "----------------------------" << endl;
    }

    delete mypim;
}

void TEST_rrm() {
    int input_num  = 2;
    int output_num = 2;
    int iterations = 1;
    int voq_size   = 4;
    rrm* myislip = new rrm(input_num, output_num, iterations, voq_size);
    myislip->init_priority_ptr();

    cout << "---------------------------------------------------------" << endl;
    cout << "--- test rrm arbitration ---" << endl;
    int cell_times = 4;
    for (int cell = 0; cell < cell_times; ++cell) {
        cout << "----------------------------" << endl;
        cout << "--- cell " << cell << " ---" << endl;
        
        myislip->request(0, 0);
        myislip->request(0, 1);
        myislip->request(1, 0);
        myislip->request(1, 1);
        myislip->arbitration();

        cout << "----------------------------" << endl;
    }

    delete myislip;
}

void TEST_fifo_and_rrm() {
    int input_num  = 2;
    int output_num = 2;
    fifo* myfifo = new fifo(input_num, output_num);
    //myfifo->init_priority_ptr();

    rrm* myrrm = new rrm(input_num, output_num, 2, 4);
    //myrrm->init_priority_ptr();

    cout << "---------------------------------------------------------" << endl;
    cout << "--- test fifo and rrm arbitration ---" << endl;
    int cell_times = 1;
    for (int cell = 0; cell < cell_times; ++cell) {
        cout << "----------------------------" << endl;
        cout << "--- cell " << cell << " ---" << endl;

        myfifo->request(0, 0);
        myfifo->request(0, 1);
        myfifo->request(1, 0);
        myfifo->request(1, 1);
        myfifo->arbitration();

        myrrm->request(0, 0);
        myrrm->request(0, 1);
        myrrm->request(1, 0);
        myrrm->request(1, 1);
        myrrm->arbitration();

        cout << "----------------------------" << endl;
    }

    delete myfifo;
    delete myrrm;
}

void TEST_rrm_and_islip_basic() {
    int input_num  = 2;
    int output_num = 2;
    int iterations = 1;
    int voq_size   = 4;

    rrm* myrrm = new rrm(input_num, output_num, iterations, voq_size);

    islip_basic* myislip = new islip_basic(input_num, output_num, iterations, voq_size);

    cout << "---------------------------------------------------------" << endl;
    cout << "--- test rrm and islip_basic 1 iteration arbitration ---" << endl;
    int cell_times = 4;
    for (int cell = 0; cell < cell_times; ++cell) {
        cout << "----------------------------" << endl;
        cout << "--- cell " << cell << " ---" << endl;

        myrrm->request(0, 0);
        myrrm->request(0, 1);
        myrrm->request(1, 0);
        myrrm->request(1, 1);
        myrrm->arbitration();

        myislip->request(0, 0);
        myislip->request(0, 1);
        myislip->request(1, 0);
        myislip->request(1, 1);
        myislip->arbitration();

        cout << "----------------------------" << endl;
    }

    delete myislip;
}

void TEST_rrm_and_islip_basic_with_mutilple_iteration() {
    int input_num  = 3;
    int output_num = 3;
    int iterations = 2;
    int voq_size   = 4;
    rrm* myrrm = new rrm(input_num, output_num, iterations, voq_size);
    islip_basic* myislip = new islip_basic(input_num, output_num, iterations, voq_size);

    cout << "---------------------------------------------------------" << endl;
    cout << "--- test rrm and islip basic 2 iterations arbitration ---" << endl;
    int cell_times = 5;
    for (int cell = 0; cell < cell_times; ++cell) {
        cout << "----------------------------" << endl;
        cout << "--- cell " << cell << " ---" << endl;

        myrrm->request(0, 0);
        myrrm->request(0, 1);
        myrrm->request(0, 2);
        myrrm->request(1, 1);
        myrrm->request(2, 1);
        myrrm->arbitration();
        
        myislip->request(0, 0);
        myislip->request(0, 1);
        myislip->request(0, 2);
        myislip->request(1, 1);
        myislip->request(2, 1);
        myislip->arbitration();

        cout << "----------------------------" << endl;
    }

    delete myislip;
}

void TEST_islip_priority() {
    int input_num   = 4;
    int output_num  = 4;
    int iterations  = 2;
    int voq_size    = 4;
    int priority_levels = 3; // 增加优先级层数, 0~2 共3级, 0为最高优先级
    
    // 统计结果：<input, output, priority> -> 成功发送的包数
    vector<int> total_snd(output_num, 0);
    vector<vector<vector<int>>> voq_stats(input_num, vector<vector<int>>(output_num, vector<int>(priority_levels, 0)));
    vector<vector<double>> ideal_percentages(input_num, vector<double>(output_num, 0.0));

    vector<tuple<int, int, int>> test_cases = {{0, 0, 0}, {0, 0, 1}, {0, 0, 2},
                                               {0, 1, 0}, {0, 1, 1}, {0, 1, 2},
                                               {1, 1, 0}, {1, 1, 1}, {1, 1, 2},
                                               {2, 2, 0}, {2, 2, 1}, {2, 2, 2},
                                               {3, 3, 0}, {3, 3, 1}, {3, 3, 2}};
    
    cout << "---------------------------------------------------------" << endl;
    cout << "--- test islip strict priority arbitration ---" << endl;
    
    int cell_times = 1000;
    islip_sp* myislip = new islip_sp(input_num, output_num, iterations, voq_size, priority_levels);
    for (int cell = 0; cell < cell_times; ++cell) {
        // 构造请求
        for (auto& tc : test_cases) {
            int in  = std::get<0>(tc);
            int out = std::get<1>(tc);
            int p   = std::get<2>(tc);

            myislip->request(in, out, p);
        }

        myislip->arbitration();

        auto slot_results = myislip->get_sch_result();
        for (auto& slot : slot_results) {
            int in  = std::get<0>(slot);
            int out = std::get<1>(slot);
            int p   = std::get<2>(slot);
            cout << "Cell " << cell << ": In " << in << " -> Out " << out << " (Prio:" << p << ")" << endl;
            voq_stats[in][out][p]++;
            total_snd[out]++;
        }
    }

    // --- 打印统计结果 ---
    cout << "\n========================================================" << endl;
    cout << "                SP BANDWIDTH REPORT (Output)            " << endl;
    cout << "========================================================" << endl;
    cout << left << setw(15) << "Channel" << setw(10) << "priority" << setw(15) << "Grant Count" << "Actual %" << "Ideal %" << endl;
    cout << "--------------------------------------------------------" << endl;

    for (auto& tc : test_cases) {
        int in  = std::get<0>(tc);
        int out = std::get<1>(tc);
        int p   = std::get<2>(tc);

        if (total_snd[out] == 0) continue; // 避免除以零

        double actual_p = (voq_stats[in][out][p] * 100.0) / total_snd[out];

        cout << "In " << in << " -> Out " << out << setw(6) << "" 
            << setw(10) << p
            << setw(15) << voq_stats[in][out][p] 
            << fixed << setprecision(1) << actual_p << "%" 
            << setw(4) << "" << ideal_percentages[in][out] << "%" << endl;
    }
    cout << "========================================================" << endl;

    delete myislip;
}

void TEST_islip_non_saturated() {
    int input_num = 4;
    int output_num = 4;
    int iterations = 2;
    int voq_size = 64; // 适当调大缓存以容纳突发
    int priority_levels = 3;
    
    // 统计结果
    vector<int> total_snd(output_num, 0);
    vector<vector<vector<int>>> voq_stats(input_num, vector<vector<int>>(output_num, vector<int>(priority_levels, 0)));

    // 定义测试配置：{Input, Output, Priority, 发送概率(0.0~1.0)}
    struct TrafficConfig {
        int in, out, prio;
        double prob;
    };

    vector<TrafficConfig> configs = {
        // --- 观察点：Out 1 ---
        {0, 1, 0, 0.3},  // In 0 发送 P0 的概率只有 30%
        {1, 1, 1, 1.0},  // In 1 始终想发送 P1 (100%)
        
        // --- 观察点：Out 2 ---
        {2, 2, 0, 0.8},  // P0 占用 80%
        {2, 2, 2, 1.0}   // P2 始终存在，观察是否能捡到剩下的 20%
    };
    
    int cell_times = 10000; // 增加样本量使概率统计更准确
    islip_sp* myislip = new islip_sp(input_num, output_num, iterations, voq_size, priority_levels);

    // 随机数生成器
    srand(time(0));

    for (int cell = 0; cell < cell_times; ++cell) {
        // --- 非饱和请求构造 ---
        for (auto& cfg : configs) {
            double r = (double)rand() / RAND_MAX;
            if (r < cfg.prob) {
                myislip->request(cfg.in, cfg.out, cfg.prio);
            }
        }

        myislip->arbitration();

        auto slot_results = myislip->get_sch_result();
        for (auto& slot : slot_results) {
            int in = std::get<0>(slot);
            int out = std::get<1>(slot);
            int p = std::get<2>(slot);
            voq_stats[in][out][p]++;
            total_snd[out]++;
        }
    }

    // --- 打印报告 ---
    cout << "\n========================================================" << endl;
    cout << "           NON-SATURATED SP REPORT (Output)            " << endl;
    cout << "========================================================" << endl;
    cout << left << setw(15) << "Channel" << setw(10) << "Prio" << setw(15) << "Prob" << "Actual %" << endl;
    cout << "--------------------------------------------------------" << endl;

    for (auto& cfg : configs) {
        if (total_snd[cfg.out] == 0) continue;
        double actual_p = (voq_stats[cfg.in][cfg.out][cfg.prio] * 100.0) / total_snd[cfg.out];

        cout << "In " << cfg.in << " -> Out " << cfg.out << setw(6) << "" 
             << setw(10) << cfg.prio
             << fixed << setprecision(1) << cfg.prob * 100 << "%" << setw(10) << ""
             << actual_p << "%" << endl;
    }
    cout << "========================================================" << endl;
    delete myislip;
}

void TEST_islip_threshold() {
    int input_num  = 3;
    int output_num = 3;
    int iterations = 1; // 1次迭代即可看出门限优先级的影响
    int voq_size   = 10;
    int threshold  = 3; // 设定门限为3
    
    // 使用我们之前整合的 islip_threshold 类
    islip_threshold* mytslip = new islip_threshold(input_num, output_num, iterations, voq_size, threshold);

    cout << "---------------------------------------------------------" << endl;
    cout << "--- Starting Threshold SLIP (T-SLIP) Test ---" << endl;
    cout << "Configuration: Threshold = " << threshold << endl;

    // --- Cell 0: 构造竞争场景 ---
    cout << "\n--- Cell 0: Building Congestion ---" << endl;
    
    // 1. 输入0 向 输出1 请求 (只有1个包，低于门限)
    mytslip->request(0, 1);
    
    // 2. 输入2 向 输出1 请求 (瞬间发送3个包，达到门限)
    mytslip->request(2, 1);
    mytslip->request(2, 1);
    mytslip->request(2, 1);

    // 执行仲裁
    // 预期：即使 Input 0 在轮询顺序中可能排在前面，
    // 但因为 Input 2 满足了 threshold 优先级，Output 1 应该授权给 Input 2。
    mytslip->arbitration();
    
    auto results = mytslip->get_sch_result();
    bool priority_met = false;
    for (auto& p : results) {
        if (p.first == 2 && p.second == 1) priority_met = true;
    }

    if (priority_met) {
        cout << "[Result] Success: Input 2 (High Priority) was scheduled over Input 0." << endl;
    } else {
        cout << "[Result] Logic check needed: Input 2 should have priority." << endl;
    }

    // --- Cell 1: 消耗掉高优先级后的场景 ---
    cout << "\n--- Cell 1: Normal Round-Robin ---" << endl;
    // 此时 Input 2 剩余 2 个包（低于门限），Input 0 剩余 1 个包。
    // 两者都回归普通优先级，按 iSLIP 轮询指针处理。
    mytslip->arbitration();

    delete mytslip;
}

void TEST_islip_wrr() {
    int input_num   = 4;
    int output_num  = 4;
    int iterations  = 2;
    int total_slots = 10000; // 模拟 1000 个时隙以获得稳定的比例

    // 定义权重：In 0, 1, 2 对 Out 0 的竞争权重分别为 5:2:1
    // 意味着 In 0 应该获得约 62.5% (5/8) 的带宽
    vector<tuple<int, int, int>> test_cases = {{0, 0, 5}, {1, 0, 2}, {2, 0, 1}, 
                                               {0, 1, 2}, {1, 1, 1}, {2, 1, 1}};
    vector<vector<int>> weights(input_num, vector<int>(output_num, 1));
    vector<int> total_num(output_num, 0);
    for (auto& tc : test_cases) {
            int in = std::get<0>(tc);
            int out = std::get<1>(tc);
            int w = std::get<2>(tc);

            weights[in][out] = w;
            total_num[out] += w;
    }

    vector<vector<double>> ideal_percentages(input_num, vector<double>(output_num, 0.0));
    for (auto& tc : test_cases) {
            int in = std::get<0>(tc);
            int out = std::get<1>(tc);
            int w = std::get<2>(tc);

            ideal_percentages[in][out] = (w * 100.0) / total_num[out];
    }

    // 初始化 WRR 调度器
    islip_wrr* wrr_scheduler = new islip_wrr(input_num, output_num, iterations, weights);

    // 统计结果：<input, output> -> 成功发送的包数
    vector<int> total_snd(output_num, 0);
    vector<vector<int>> voq_stats(input_num, vector<int>(output_num, 0));

    cout << "---------------------------------------------------------" << endl;
    cout << "--- test islip wrr arbitration ---" << endl;
    cout << "Configured Weights for Output 0 -> In0:5, In1:2, In2:1" << endl;

    for (int slot = 0; slot < total_slots; ++slot) {
        // 1. 模拟全负载压力：每个时隙所有输入都有发往 Out 0 和 Out 1的请求
        wrr_scheduler->request(0, 0);
        wrr_scheduler->request(1, 0);
        wrr_scheduler->request(2, 0);

        wrr_scheduler->request(0, 1);
        wrr_scheduler->request(1, 1);
        wrr_scheduler->request(2, 1);

        // 2. 执行仲裁
        wrr_scheduler->arbitration();

        auto slot_results = wrr_scheduler->get_sch_result();
        for (auto& p : slot_results) {
            //cout << "Slot " << slot << ": In " << p.first << " -> Out " << p.second << endl;
            voq_stats[p.first][p.second]++;
            total_snd[p.second]++;
        }
    }

    // --- 打印统计结果 ---
    cout << "\n========================================================" << endl;
    cout << "                WRR BANDWIDTH REPORT (Output)            " << endl;
    cout << "========================================================" << endl;
    cout << left << setw(15) << "Channel" << setw(10) << "Weight" << setw(15) << "Grant Count" << "Actual %" << "Ideal %" << endl;
    cout << "--------------------------------------------------------" << endl;

    for (auto& tc : test_cases) {
        int in = std::get<0>(tc);
        int out = std::get<1>(tc);
        int w = std::get<2>(tc);

        if (total_snd[out] == 0) continue; // 避免除以零

        double actual_p = (voq_stats[in][out] * 100.0) / total_snd[out];

        cout << "In " << in << " -> Out " << out << setw(6) << "" 
            << setw(10) << w 
            << setw(15) << voq_stats[in][out] 
            << fixed << setprecision(1) << actual_p << "%" 
            << setw(4) << "" << ideal_percentages[in][out] << "%" << endl;
    }
    cout << "========================================================" << endl;

    delete wrr_scheduler;
}

void TEST_dpa() {
    cout << "---------------------------------------------------------" << endl;
    // Test case 1: Basic functionality
    std::cout << "Test case 1: Basic functionality" << std::endl;
    dpa_scheduler arbiter1(4, 4);
    arbiter1.request(0, 2);
    arbiter1.request(0, 3);
    arbiter1.request(1, 0);
    arbiter1.request(1, 2);
    arbiter1.request(2, 0);
    arbiter1.request(2, 1);
    arbiter1.request(2, 3);
    arbiter1.request(3, 1);
    arbiter1.request(3, 3);

    for (int i = 0; i < arbiter1.m_num_ports * 2; i++) {
        std::cout << arbiter1.m_ptr << std::endl;
        arbiter1.arbitration();
        for (int row = 0; row < arbiter1.m_num_ports; row++) {
            for (int col = 0; col < arbiter1.m_num_ports; col++) {
                std::cout << arbiter1.m_grants[row][col] << ", ";
            }

            std::cout << " " << std::endl;
        }

        auto result = arbiter1.sch_result;

        bool grant_0_2 = arbiter1.has_granted(0, 2);
        bool grant_0_3 = arbiter1.has_granted(0, 3);
        bool grant_1_0 = arbiter1.has_granted(1, 0);
        bool grant_1_2 = arbiter1.has_granted(1, 2);
        bool grant_2_0 = arbiter1.has_granted(2, 0);
        bool grant_2_1 = arbiter1.has_granted(2, 1);
        bool grant_2_3 = arbiter1.has_granted(2, 3);
        bool grant_3_1 = arbiter1.has_granted(3, 1);
        bool grant_3_3 = arbiter1.has_granted(3, 3);
    }

    if (arbiter1.has_granted(1, 2)) {
        std::cout << "Input 1 is granted access to output 2." << std::endl;
    } else {
        std::cout << "Input 1 is not granted access." << std::endl;
    }

    if (arbiter1.has_granted(2, 0)) {
        std::cout << "Input 2 is granted access to output 0." << std::endl;
    } else {
        std::cout << "Input 2 is not granted access." << std::endl;
    }

    // Test case 2: Invalid port numbers
    std::cout << "\nTest case 2: Invalid port numbers" << std::endl;
    try {
        arbiter1.request(-1, 2);  // Invalid input port
    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << std::endl;
    }
    try {
        arbiter1.has_granted(4, 0);  // Invalid output port
    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << std::endl;
    }

    // Test case 3: No requests
    std::cout << "\nTest case 3: No requests" << std::endl;
    dpa_scheduler arbiter2(2, 2);
    arbiter2.arbitration();
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            if (arbiter2.has_granted(i, j)) {
                std::cout << "Unexpected grant: Input " << i << " to output "
                          << j << std::endl;
            }
        }
    }

    // Test case 4: Multiple requests for the same output port
    std::cout << "\nTest case 4: Multiple requests for the same output port"
              << std::endl;
    dpa_scheduler arbiter3(3, 3);
    arbiter3.request(0, 2);
    arbiter3.request(1, 2);
    arbiter3.request(2, 2);
    arbiter3.arbitration();
    if (arbiter3.has_granted(0, 2)) {
        std::cout << "Input 0 is granted access to output 2." << std::endl;
    } else {
        std::cout << "Input 0 is not granted access." << std::endl;
    }
    if (arbiter3.has_granted(1, 2)) {
        std::cout << "Input 1 is granted access to output 2." << std::endl;
    } else {
        std::cout << "Input 1 is not granted access." << std::endl;
    }
    if (arbiter3.has_granted(2, 2)) {
        std::cout << "Input 2 is granted access to output 2." << std::endl;
    } else {
        std::cout << "Input 2 is not granted access." << std::endl;
    }
}