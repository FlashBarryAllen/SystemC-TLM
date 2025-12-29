#include "test.h"

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

void TEST_islip_priority() {
    // 4端口，4输出，2个优先级 (0=High, 1=Low)
    islip* myislip = new islip(4, 4, 2); 
    myislip->init();
    
    // 场景：Speedup = 1
    // Input 0 有一个高优先级包去 Output 0
    myislip->set_ql(0, 0, 0); 
    
    // Input 0 也有一个低优先级包去 Output 1
    myislip->set_ql(0, 1, 1);

    // 运行调度
    myislip->islip_sch(4, 1); // 4次迭代，Speedup=1

    // 预期结果：
    // 因为 Speedup=1，Input 0 只能发一个包。
    // 由于 Strict Priority，Prio 0 应该胜出。
    // 结果应为 (0, 0)，而不是 (0, 1)

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

void TEST_islip_ycl() {
    islip_ycl* myislip = new islip_ycl(4, 4, 2);
    
    while (1) {
        std::cout << "--- islip_ycl测试 ---" << std::endl;
        myislip->request(0, 0);
        myislip->request(0, 1);
        myislip->request(2, 1);
        myislip->request(2, 3);
        myislip->request(3, 3);

        myislip->arbitration();

        auto ret = myislip->get_sch_result();
        myislip->reset();
        for (auto& p : ret) std::cout << "输入 " << p.first << " -> 输出 " << p.second << std::endl;
    }
}

void TEST_dpa() {
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