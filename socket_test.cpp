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
		A(sc_module_name name) : sc_module(name) {
			SC_METHOD(process);
			sensitive << m_clk.pos();
			dont_initialize();
		}
		~A() {}

		void process() {
			val++;
			std::cout << "A::process()" << std::endl;
			tlm_generic_payload pl;
			pl.set_command(TLM_WRITE_COMMAND);
			pl.set_address(0x10000);
			//unsigned char data[4] = {0x12, 0x34, 0x56, 0x78};
			unsigned char data[4] = {0x78, 0x56, 0x34, 0x12};
			pl.set_data_ptr(data);
			pl.set_data_length(4);
			tlm_phase phase = BEGIN_REQ;
			sc_time time = SC_ZERO_TIME;
			tx->nb_transport_fw(pl, phase, time);
		}
	public:
		int val;
		sc_in_clk m_clk;
		simple_initiator_socket<A> tx;
};

class B : public sc_module {
	public:
		SC_HAS_PROCESS(B);
		B(sc_module_name name) : sc_module(name) {
			rx.register_nb_transport_fw(this, &B::rcv_from);
			SC_METHOD(process);
			sensitive << m_clk.pos();
			dont_initialize();
		}
		~B() {}
		void process() {
			val++;
			std::cout << "B::process()" << std::endl;
		}
		tlm_sync_enum rcv_from(tlm_generic_payload& pl, tlm_phase& phase, sc_time& time) {
			tlm_command cmd = pl.get_command();
			uint64_t addr = pl.get_address();
			uint32_t len = pl.get_data_length();
			unsigned int* data = reinterpret_cast<unsigned int*>(pl.get_data_ptr());
			std::cout << std::hex << "cmd: " << cmd << ", addr=0x" << addr << ", len=0x" << len << ", data=0x" << data[0] << std::endl;
			//fifo.push_back(pl);
			return TLM_COMPLETED;
		}

	public:
		int val;
		sc_in_clk m_clk;
		simple_target_socket<B> rx;
		std::deque<tlm_generic_payload> fifo;
};

class topo : sc_module {
	public:
		topo(sc_module_name name) : sc_module(name), snd("snd"), rcv("rcv"), clk("clk", 1, SC_NS) {
			snd.m_clk(clk);
			rcv.m_clk(clk);
			snd.tx.bind(rcv.rx);
		}

	public:
		A snd;
		B rcv;
		sc_clock clk;
};


int sc_main(int argc, char* argv[])
{
	topo top("top");
	sc_start(2, SC_NS);
	return 0;
}
