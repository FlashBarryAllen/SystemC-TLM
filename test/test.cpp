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

        myislip->islip_sch();

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
    myislip->set_ql(1, 2); 

    // 2. 关键：确保输出 0 和 输出 2 都没有其他追求者，只能授权给输入 1
    // 不要调用 set_ql(0, 0) 或 set_ql(3, 2)

    // 3. 让 输入 2 去请求一个无关的 输出 1，保证系统不全空
    myislip->set_ql(2, 2);

    myislip->islip_sch();

    auto ret = myislip->sch_result;
    std::cout << "--- 1次迭代的最终对决结果 ---" << std::endl;
    for (auto& p : ret) {
        std::cout << "输入 " << p.first << " -> 输出 " << p.second << std::endl;
    }
}