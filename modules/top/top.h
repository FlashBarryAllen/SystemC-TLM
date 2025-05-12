#ifndef TOP_H
#define TOP_H

#include <node.h>
#include <tg-tlm.h>
#include <traffic-desc.h>
#include <gen_utils.h>
#include <memory.h>
#include <memory>

using namespace sc_core;
using namespace sc_dt;
using namespace std;
using namespace gen_utils;

#define RAM_SIZE (8 * 1024)

struct gen_data
{
    int cmd;
    uint64_t addr;
    unsigned char * data_ptr;
    int len;
};

class top : public sc_core::sc_module {
    public:
        SC_HAS_PROCESS(top);
        top(sc_core::sc_module_name name);
        ~top();
 
        void run();
        virtual void b_transport(tlm::tlm_generic_payload& trans, sc_time& delay);
 
    public:
        sc_clock clk;
        sc_in_clk clk_in;
        TLMTrafficGenerator tg;
        memory b0_mem;
        //tlm_utils::simple_target_socket<top> tgt_socket;
        deque<shared_ptr<gen_data>> m_input_que;
 };

 #endif