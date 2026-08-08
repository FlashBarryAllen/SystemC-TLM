#include <systemc>

using namespace std;
using namespace sc_core;

class A : public sc_module
{
public:
    SC_HAS_PROCESS(A);
    A(sc_module_name name) : sc_module(name) {
        SC_METHOD(proc);
        sensitive << m_clk.pos();
        dont_initialize();
    }

    void proc() {
        tx.nb_write(data);
        cout << "A::proc() sent data: " << data << " at " << sc_time_stamp() << endl;
        data++;
    }

    int data = 0;
    sc_in_clk m_clk;
    sc_fifo_out<int> tx;
};

class B : public sc_module
{
public:
    SC_HAS_PROCESS(B);
    B(sc_module_name name) : sc_module(name) {
        SC_METHOD(proc);
        sensitive << m_clk.pos();
        dont_initialize();
    }

    void proc() {
        int data;
        if (tx.nb_read(data)) {
            cout << "B::proc() received data: " << data << " at " << sc_time_stamp() << endl;
        }
    }

    sc_in_clk m_clk;
    sc_fifo_in<int> tx;
};

class sc_fifo_top : public sc_module
{
public:
    sc_fifo_top(sc_module_name name) : sc_module(name), a("a"), b("b"), m_clk("m_clk", 10, SC_NS) {
        a.m_clk(m_clk);
        b.m_clk(m_clk);

        a.tx(fifo);
        b.tx(fifo);
    }

    sc_clock m_clk;
    sc_fifo<int> fifo;
    A a;
    B b;
};

int sc_main(int argc, char* argv[])
{
    sc_fifo_top t("t");
    sc_start(100, SC_NS);
    return 0;
}