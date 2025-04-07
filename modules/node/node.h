#ifndef NODE_H
#define NODE_H

#include <memory>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/peq_with_get.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <tinyxml2.h>

using namespace tinyxml2;
using namespace std;

enum msg_type {
    CTRL_MSG = 0,
    DATA_MSG = 1,
    MAX_MSG
};

typedef struct ctrl_msg
{
    int      credit;
} MY_CTRL_T;

typedef struct data
{
    int a;
    int b;
    int c;
} MY_DAT_T;

typedef struct api
{
    msg_type type;
    std::shared_ptr<void> dat;
} MY_API_T;

class node : public sc_core::sc_module {
    public:
     SC_HAS_PROCESS(node);
     node(sc_core::sc_module_name name);
    
     virtual void get_config(const char* xml_file);
     virtual void mth_entry();
     void cal_speed();
     void set_credit(int credit);
     virtual tlm::tlm_sync_enum nb_transport_fw(tlm::tlm_generic_payload& trans,
                                                tlm::tlm_phase& phase,
                                                sc_core::sc_time& time);
 
    public:
     int m_cycle_cnt;
     int m_val;
     int m_credit;
     int m_credit_ret;
     struct timespec m_now_ts;
     sc_core::sc_in_clk m_clk;
     tlm_utils::simple_initiator_socket<node> tx;
     tlm_utils::simple_target_socket<node>    rx;
     std::shared_ptr<spdlog::logger> m_logger;
 };

#endif // NODE_H