#ifndef TEST_H
#define TEST_H

#include <iostream>
#include <deque>
#include <vector>
#include <cassert>
#include "islip.h"
#include "booksim_islip.h"
#include "fifo.h"
#include "pim.h"
#include "rrm.h"
#include "islip_basic.h"
#include "islip_sp.h"
#include "islip_threshold.h"
#include "islip_wrr.h"
#include "dpa.h"
#include "lsu.h"

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
void TEST_max_match();
void TEST_starvation();
void TEST_booksim_max_match();

void TEST_fifo();
void TEST_pim();
void TEST_rrm();
void TEST_fifo_and_rrm();
void TEST_rrm_and_islip_basic();
void TEST_rrm_and_islip_basic_with_mutilple_iteration();
void TEST_islip_priority();
void TEST_islip_non_saturated();
void TEST_islip_threshold();
void TEST_islip_wrr();

void TEST_dpa();

void TEST_load_forwarding();

#endif // TEST_H