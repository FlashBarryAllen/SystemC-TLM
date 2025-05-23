#ifndef DMA_CTRL_LT_H
#define DMA_CTRL_LT_H

#include "systemc.h"
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include <vector>
#include <iostream>

SC_MODULE(LT_DmaController) {
    tlm_utils::simple_initiator_socket<LT_DmaController> mem_socket; // 连接内存
    // ... 可能还有连接到 CPU 的配置接口等，这里简化

    SC_CTOR(LT_DmaController) : mem_socket("mem_socket") {
        SC_THREAD(dma_transfer_thread);
    }

    void dma_transfer_thread() {
        // 假设通过某种机制（如寄存器配置）获取到DMA传输参数
        uint64_t src_addr = 0x100;
        uint64_t dst_addr = 0x200;
        unsigned int length = 16; // 传输16字节

        std::cout << sc_time_stamp() << ": LT_DMA: Starting transfer from 0x"
                  << std::hex << src_addr << " to 0x" << dst_addr
                  << " for " << std::dec << length << " bytes." << std::endl;

        tlm::tlm_generic_payload gp;
        sc_time delay = SC_ZERO_TIME;
        unsigned char data_buffer[length]; // 用于读写的数据缓冲区

        // 1. 从源地址读取数据 (Read Phase)
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(src_addr);
        gp.set_data_ptr(data_buffer);
        gp.set_data_length(length);
        gp.set_streaming_width(length); // 设置流宽度，这里假设全宽
        gp.set_byte_enable_ptr(0); // 读操作通常不需要字节使能
        gp.set_dmi_allowed(false);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        // 调用阻塞传输
        mem_socket->b_transport(gp, delay); // delay在这里通常是SC_ZERO_TIME或者被忽略

        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "LT_DMA: Read failed!" << std::endl;
            sc_stop();
            return;
        }

        std::cout << sc_time_stamp() << ": LT_DMA: Read " << std::dec << length
                  << " bytes from 0x" << std::hex << src_addr << std::endl;

        // 2. 将数据写入目标地址 (Write Phase)
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(dst_addr);
        // data_buffer 已经包含了读取到的数据
        gp.set_byte_enable_ptr(0); // 写操作通常全使能，除非有特殊逻辑
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        mem_socket->b_transport(gp, delay); // delay在这里通常是SC_ZERO_TIME或者被忽略

        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "LT_DMA: Write failed!" << std::endl;
            sc_stop();
            return;
        }

        std::cout << sc_time_stamp() << ": LT_DMA: Wrote " << std::dec << length
                  << " bytes to 0x" << std::hex << dst_addr << std::endl;

        std::cout << sc_time_stamp() << ": LT_DMA: Transfer completed." << std::endl;
        //sc_stop(); // 停止仿真
    }
};

// 内存模型 (与之前示例类似，但为了LT模式，忽略delay的累积)
SC_MODULE(LT_Memory) {
    tlm_utils::simple_target_socket<LT_Memory> socket;
    std::vector<unsigned char> mem;
    enum { MEM_SIZE = 1024 };

    SC_CTOR(LT_Memory) : socket("socket") {
        socket.register_b_transport(this, &LT_Memory::b_transport);
        mem.resize(MEM_SIZE, 0xAA);
        std::cout << "LT_Memory: Initialized " << MEM_SIZE << " bytes to 0xAA" << std::endl;
    }

    virtual void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
        tlm::tlm_command cmd = trans.get_command();
        sc_dt::uint64   addr = trans.get_address();
        unsigned char* ptr  = trans.get_data_ptr();
        unsigned int    len  = trans.get_data_length();

        // LT模式下，delay在这里通常是SC_ZERO_TIME，或者不进行实质性累积
        // 这里为了简单，我们还是给一个非常小的延迟，但它不代表真实时序
        delay = SC_ZERO_TIME; // 在LT模式下，通常不累积实际延迟

        if (addr >= MEM_SIZE || (addr + len) > MEM_SIZE) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        if (cmd == tlm::TLM_READ_COMMAND) {
            for (unsigned int i = 0; i < len; ++i) {
                ptr[i] = mem[addr + i];
            }
        } else if (cmd == tlm::TLM_WRITE_COMMAND) {
            for (unsigned int i = 0; i < len; ++i) {
                mem[addr + i] = ptr[i];
            }
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

// Top 模块
SC_MODULE(Top_LT) {
    LT_DmaController dma_ctrl;
    LT_Memory mem;

    SC_CTOR(Top_LT) : dma_ctrl("dma_ctrl"), mem("mem") {
        dma_ctrl.mem_socket.bind(mem.socket);
    }
};

// int sc_main(int argc, char* argv[]) {
//     Top_LT top("top_lt");
//     sc_start();
//     // 验证内存内容
//     // std::cout << "LT_Memory at 0x200: " << std::hex << (int)top.mem.mem[0x200] << std::endl;
//     return 0;
// }

#endif // LT_DMA_CTRL_H