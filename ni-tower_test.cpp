#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <deque>

using namespace sc_core;
using namespace tlm;
using namespace tlm_utils;

class A : public sc_module {
    public:
        SC_HAS_PROCESS(A);
        A(sc_module_name name) : sc_module(name), m_credits(4) {
            rx_ctrl.register_nb_transport_fw(this, &A::rcv_ctrl_from);
            SC_METHOD(process);
            sensitive << m_clk.pos();
            dont_initialize();
        }
        ~A() {

        }

        void process();
        tlm_sync_enum rcv_ctrl_from(tlm_generic_payload& pl, tlm_phase& phase, sc_time& delay);

    public:
        sc_in_clk m_clk;
        simple_target_socket<A> rx_ctrl;
        simple_initiator_socket<A> tx_data;
        int m_credits;
};

void A::process() {
    if (m_credits > 0) {
        m_credits--;
        tlm_generic_payload pl;
        pl.set_command(TLM_READ_COMMAND);
        pl.set_address(0x10000);
        pl.set_data_length(4);
        unsigned char data[] = {0x12, 0x34, 0x56, 0x78};
        pl.set_data_ptr(data);
        tlm_phase phase = BEGIN_REQ;
        sc_time delay = SC_ZERO_TIME;
        tx_data->nb_transport_fw(pl, phase, delay);
    } else {
        std::cout << "time: " << sc_time_stamp() << " A has no credits to send data" << std::endl;
    }
}

tlm_sync_enum A::rcv_ctrl_from(tlm_generic_payload& pl, tlm_phase& phase, sc_time& delay) {
    m_credits++;
    return TLM_COMPLETED;
}

class B : public sc_module {
    public:
        SC_HAS_PROCESS(B);
        B(sc_module_name name) : sc_module(name), m_delay_cnt(0) {
            rx_data.register_nb_transport_fw(this, &B::rcv_data_from);
            SC_METHOD(process);
            sensitive << m_clk.pos();
            dont_initialize();
        }
        ~B() {

        }

        void process();
        tlm_sync_enum rcv_data_from(tlm_generic_payload& pl, tlm_phase& phase, sc_time& delay);
    
    public:
        sc_in_clk m_clk;
        simple_target_socket<B> rx_data;
        simple_initiator_socket<B> tx_ctrl;
        std::deque<int> m_data_queue;
        int m_delay_cnt;
};

void B::process() {
    if (m_data_queue.empty()) {
        return;
    }

    m_delay_cnt++;
    if (m_delay_cnt == 2) {  // Simulate a delay
        m_delay_cnt = 0;
        int data = m_data_queue.front();
        std::cout << "time: " << sc_time_stamp() << " B pop data: 0x" << std::hex << data << std::dec << std::endl;
        m_data_queue.pop_front();
        tlm_generic_payload pl;
        pl.set_command(TLM_WRITE_COMMAND);
        tlm_phase phase = BEGIN_REQ;
        sc_time delay = SC_ZERO_TIME;
        tx_ctrl->nb_transport_fw(pl, phase, delay);
    }
}

tlm_sync_enum B::rcv_data_from(tlm_generic_payload& pl, tlm_phase& phase, sc_time& delay) {
    unsigned char* data_ptr = pl.get_data_ptr();
    int data = (data_ptr[0] << 24) | (data_ptr[1] << 16) | (data_ptr[2] << 8) | data_ptr[3];
    std::cout << "time: " << sc_time_stamp() << " B rcv and push data: 0x" << std::hex << data << std::dec << std::endl;
    m_data_queue.push_back(data);
    return TLM_COMPLETED;
}

class Top : public sc_module {
    public:
        SC_HAS_PROCESS(Top);
        Top(sc_module_name name) : sc_module(name), a("a"), b("b"), m_clk("clk", 1, SC_NS) {
            a.m_clk(m_clk);
            b.m_clk(m_clk);
            a.tx_data(b.rx_data);
            b.tx_ctrl(a.rx_ctrl);           
        }

        ~Top() {

        }
    
    public:
        A a;
        B b;
        sc_clock m_clk;
};

int sc_main(int argc, char* argv[]) {
    Top top("top");
    sc_start(11, SC_NS);
    return 0;
}