#ifndef TOP_H
#define TOP_H

#include <node.h>
#include <tg-tlm.h>
#include <traffic-desc.h>
#include <gen_utils.h>
#include <memory.h>

using namespace sc_core;
using namespace sc_dt;
using namespace std;
using namespace gen_utils;

#define RAM_SIZE (8 * 1024)

class top : public sc_core::sc_module {
    public:
        SC_HAS_PROCESS(top);
        top(sc_core::sc_module_name name);
        ~top();

        void run();
 
    public:
        sc_clock clk;
        sc_in_clk clk_in;
        TLMTrafficGenerator tg;
        memory b0_mem;
 };

 #endif