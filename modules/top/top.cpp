#include <top.h>

/*
TrafficDesc transfers(merge({
    Write(0x0, DATA(0xEE, 0x10, 0x80, 0x0)),
    Write(0x4, DATA(0x0, 0x00, 0x10, 0x0)),
    Write(0x8, DATA(0x0, 0x0, 0x0, 0x2)),

	Read(0x0, 4),
		//Expect(DATA(0xEE, 0x10, 0x80, 0x0), 4),
	Read(0x4, 4),
		//Expect(DATA(0x0, 0x00, 0x10, 0x0), 4),
	Read(0x8, 4),
		//Expect(DATA(0x0, 0x0, 0x0, 0x2), 4),
}));
*/

top::top(sc_core::sc_module_name name) : sc_module(name), tg("tg"), 
    b0_mem("b0_mem", sc_time(0, SC_NS), RAM_SIZE), 
    //tgt_socket("tgt_socket"),
    clk("clk", sc_time(1, SC_NS))
{
    //tgt_socket.register_b_transport(this, &top::b_transport);
    clk_in(clk);
    SC_METHOD(run);
    sensitive << clk_in.pos();
    dont_initialize();

    //
    // Setup traffic generator
    //
    DataTransferVec data_transfers = tg.get_config("gen.xml");
    static TrafficDesc m_transfers(merge(data_transfers));
    tg.addTransfers(m_transfers);
    tg.enableDebug();

    tg.socket.bind(b0_mem.socket);
}

top::~top()
{

}

void top::b_transport(tlm::tlm_generic_payload& trans, sc_time& delay)
{
    tlm::tlm_command cmd = trans.get_command();
	sc_dt::uint64 addr = trans.get_address();
	unsigned char *ptr = trans.get_data_ptr();
	unsigned int len = trans.get_data_length();

    shared_ptr<gen_data> gen = make_shared<gen_data>();
    gen->cmd      = cmd;
    gen->addr     = addr;
    gen->data_ptr = ptr;
    gen->len      = len;
    m_input_que.push_back(gen);

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

void top::run()
{
    cout << sc_time_stamp() << endl;
    if (!m_input_que.empty())
    {
        shared_ptr<gen_data> gen = m_input_que.front();
        m_input_que.pop_front();
    }
}