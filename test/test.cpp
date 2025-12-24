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