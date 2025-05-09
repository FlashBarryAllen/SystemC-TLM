#include <top.h>

TrafficDesc transfers(merge({
    Write(0x0, DATA(0xEE, 0x10, 0x80, 0x0)),
    Write(0x4, DATA(0x0, 0x00, 0x10, 0x0)),
    Write(0x8, DATA(0x0, 0x0, 0x0, 0x2)),

	Read(0x0, 4),
		Expect(DATA(0xEE, 0x10, 0x80, 0x0), 4),
	Read(0x4, 4),
		Expect(DATA(0x0, 0x00, 0x10, 0x0), 4),
	Read(0x8, 4),
		Expect(DATA(0x0, 0x0, 0x0, 0x2), 4),
}));

top::top(sc_core::sc_module_name name) : sc_module(name), tg("tg"), clk("clk", sc_time(1, SC_NS)), b0_mem("b0_mem", sc_time(0, SC_NS), RAM_SIZE)
{
    clk_in(clk);
    SC_METHOD(run);
    sensitive << clk_in.pos();
    //
    // Setup traffic generator
    //
    tg.addTransfers(transfers);
    tg.enableDebug();

    tg.socket.bind(b0_mem.socket);
}

top::~top()
{

}

void top::run()
{
    // TBD
}