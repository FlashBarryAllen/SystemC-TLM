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

int Qos::pq_sch()
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
        while (last_sch < VIRTURL_CHANNEL_NUM)
        {
            if (!vc[last_sch].empty())
            {
                std::cout << "Processing data from channel " << last_sch << ": " << vc[last_sch].front() << std::endl;
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