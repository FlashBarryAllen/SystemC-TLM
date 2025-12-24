#ifndef TEST_H
#define TEST_H

#include <iostream>
#include <deque>
#include <vector>
#include "islip.h"

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

void TEST_islip();
void TEST_islip_lonely_pair();
void TEST_islip_real_lonely();
void TEST_islip_lonely();
void TEST_islip_final_showdown();
void TEST_islip_priority();
void TEST_Shadow_Preemption();
void TEST_Quota_Exhaustion();
void TEST_Full_Load_Suppression();

#endif // TEST_H