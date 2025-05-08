#ifndef TEST_H
#define TEST_H

#include <iostream>
#include <deque>
#include <vector>

#define VIRTURL_CHANNEL_NUM 8

class Qos
{
    public:
        Qos();
        ~Qos();

        int in(int priority, int data);
        int sp_sch();
        int rr_sch();
    public:
        std::vector<std::deque<int>> vc;
        int cnt = 0;
        int last_sch = 0;
        int time = 0;
};

#endif // TEST_H