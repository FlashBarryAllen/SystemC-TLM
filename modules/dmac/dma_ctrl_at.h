#ifndef DMA_CTRL_AT_H
#define DMA_CTRL_AT_H

#include "systemc.h"
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include <vector>
#include <iostream>

SC_MODULE(AT_DmaController) {
    tlm_utils::simple_initiator_socket<AT_DmaController> mem_socket;

    SC_CTOR(AT_DmaController) : mem_socket("mem_socket") {
        SC_THREAD(dma_transfer_thread);
    }

    void dma_transfer_thread() {
        uint64_t src_addr = 0x100;
        uint64_t dst_addr = 0x200;
        unsigned int length = 16; // 传输16字节
        unsigned int burst_size = 4; // 假设DMA以4字节（1Dword）为突发大小

        std::cout << sc_time_stamp() << ": AT_DMA: Starting transfer from 0x"
                  << std::hex << src_addr << " to 0x" << dst_addr
                  << " for " << std::dec << length << " bytes." << std::endl;

        tlm::tlm_generic_payload gp;
        sc_time local_delay = SC_ZERO_TIME; // 每次事务的累积延迟
        unsigned char data_buffer[length];

        // 模拟突发传输
        for (unsigned int i = 0; i < length; i += burst_size) {
            unsigned int current_len = std::min(burst_size, length - i);

            // Read burst
            gp.set_command(tlm::TLM_READ_COMMAND);
            gp.set_address(src_addr + i);
            gp.set_data_ptr(&data_buffer[i]);
            gp.set_data_length(current_len);
            gp.set_streaming_width(current_len); // 设置流宽度
            gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

            local_delay = SC_ZERO_TIME; // 每次b_transport调用前清零，让target累积
            mem_socket->b_transport(gp, local_delay);
            wait(local_delay); // 等待从Target返回的延迟

            if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
                std::cerr << "AT_DMA: Read burst failed at 0x" << std::hex << (src_addr + i) << std::endl;
                sc_stop(); return;
            }

            std::cout << sc_time_stamp() << ": AT_DMA: Read burst of " << std::dec << current_len
                      << " bytes from 0x" << std::hex << (src_addr + i) << std::endl;

            // Write burst
            gp.set_command(tlm::TLM_WRITE_COMMAND);
            gp.set_address(dst_addr + i);
            // data_buffer[i] 已经包含数据
            gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

            local_delay = SC_ZERO_TIME;
            mem_socket->b_transport(gp, local_delay);
            wait(local_delay); // 等待从Target返回的延迟

            if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
                std::cerr << "AT_DMA: Write burst failed at 0x" << std::hex << (dst_addr + i) << std::endl;
                sc_stop(); return;
            }

            std::cout << sc_time_stamp() << ": AT_DMA: Wrote burst of " << std::dec << current_len
                      << " bytes to 0x" << std::hex << (dst_addr + i) << std::endl;
        }

        std::cout << sc_time_stamp() << ": AT_DMA: Transfer completed." << std::endl;
        sc_stop();
    }
};

// 内存模型 (AT 版本 - 会累积实际延迟)
SC_MODULE(AT_Memory) {
    tlm_utils::simple_target_socket<AT_Memory> socket;
    std::vector<unsigned char> mem;
    enum { MEM_SIZE = 1024 };

    SC_CTOR(AT_Memory) : socket("socket") {
        socket.register_b_transport(this, &AT_Memory::b_transport);
        mem.resize(MEM_SIZE, 0xAA);
        std::cout << "AT_Memory: Initialized " << MEM_SIZE << " bytes to 0xAA" << std::endl;
    }

    virtual void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
        tlm::tlm_command cmd = trans.get_command();
        sc_dt::uint64   addr = trans.get_address();
        unsigned char* ptr  = trans.get_data_ptr();
        unsigned int    len  = trans.get_data_length();

        // 模拟传输延迟：例如，固定每次事务10ns的开销 + 每字节1ns
        delay += sc_time(10, SC_NS); // 事务开销
        delay += sc_time(len, SC_NS); // 数据传输延迟

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
SC_MODULE(Top_AT) {
    AT_DmaController dma_ctrl;
    AT_Memory mem;

    SC_CTOR(Top_AT) : dma_ctrl("dma_ctrl"), mem("mem") {
        dma_ctrl.mem_socket.bind(mem.socket);
    }
};

// Top 模块
// int sc_main(int argc, char* argv[]) {
//     Top_AT top("top_at");
//     sc_start();
//     return 0;
// }

#endif // LT_DMA_CTRL_H