#ifdef DMA_ENGINE_H
#define DMA_ENGINE_H

#include "systemc.h"
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "tlm_utils/peq_with_get.h" // 用于异步时间调度

// 方便打印 TLM Phase
const char* phase_to_string(tlm::tlm_phase p) {
    switch (p) {
        case tlm::BEGIN_REQ: return "BEGIN_REQ";
        case tlm::END_REQ:   return "END_REQ";
        case tlm::BEGIN_RESP:return "BEGIN_RESP";
        case tlm::END_RESP:  return "END_RESP";
        case tlm::UNINITIALIZED_PHASE: return "UNINITIALIZED_PHASE";
        default: return "UNKNOWN_PHASE";
    }
}

// ----------------------------------------------------------------------------
// Memory 模块
// ----------------------------------------------------------------------------
SC_MODULE(Memory) {
    tlm_utils::simple_target_socket<Memory> socket;
    std::vector<unsigned char> mem;
    enum { MEM_SIZE = 4096 }; // 4KB 内存

    SC_CTOR(Memory) : socket("socket") {
        socket.register_nb_transport_fw(this, &Memory::nb_transport_fw);
        mem.resize(MEM_SIZE, 0); // 初始化为0
        std::cout << "Memory: Initialized " << MEM_SIZE << " bytes." << std::endl;
    }

    // 这里实现的是阻塞传输，但在 TLM 2.0 AT 模型中，通常也通过 PEQ 来模拟非阻塞传输的延迟
    // 为了简化，这里直接执行，不使用PEQ，因为主要测试DMA的PEQ
    virtual tlm::tlm_sync_enum nb_transport_fw(
        tlm::tlm_generic_payload& trans, tlm::tlm_phase& phase, sc_core::sc_time& delay)
    {
        sc_dt::uint64   addr = trans.get_address();
        unsigned int    len  = trans.get_data_length();
        unsigned char* data = trans.get_data_ptr();
        tlm::tlm_command cmd = trans.get_command();

        // 模拟内存访问延迟
        // delay += sc_time(len, SC_NS); // 简单的每字节 1ns 延迟

        if (addr >= MEM_SIZE || (addr + len) > MEM_SIZE) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        } else {
            if (cmd == tlm::TLM_READ_COMMAND) {
                std::memcpy(data, &mem[addr], len);
                std::cout << sc_time_stamp() << ": Memory: Read " << std::dec << len << " bytes from 0x"
                          << std::hex << addr << std::endl;
            } else if (cmd == tlm::TLM_WRITE_COMMAND) {
                std::memcpy(&mem[addr], data, len);
                std::cout << sc_time_stamp() << ": Memory: Wrote " << std::dec << len << " bytes to 0x"
                          << std::hex << addr << std::endl;
            }
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
        }

        // AT 模型的典型处理：立即返回 BEGIN_RESP，或在延迟后返回
        // 简化：直接将 phase 推进到 BEGIN_RESP，并返回 TLM_COMPLETED
        // 实际：这里会使用PEQ，将事务推入，在延迟后从PEQ弹出，再通知发起端
        if (phase == tlm::BEGIN_REQ) {
            phase = tlm::BEGIN_RESP;
            return tlm::TLM_COMPLETED; // 表示事务已完成，或者将立即进入下一个相位
        }
        return tlm::TLM_ACCEPTED; // 表示事务已接受，但尚未完成
    }

    virtual tlm::tlm_sync_enum nb_transport_bw(
        tlm::tlm_generic_payload& trans, tlm::tlm_phase& phase, sc_core::sc_time& delay) {
        SC_REPORT_FATAL(name(), "nb_transport_bw not implemented for Memory!");
        return tlm::TLM_COMPLETED;
    }

    virtual void invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range) {}
    virtual bool get_direct_mem_ptr(tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data) { return false; }
};

// ----------------------------------------------------------------------------
// DMA Device 模块
// ----------------------------------------------------------------------------
SC_MODULE(DmaDevice) {
    tlm_utils::simple_target_socket<DmaDevice> mmio_socket;      // 用于 CPU 访问 MMIO 寄存器
    tlm_utils::simple_initiator_socket<DmaDevice> mem_socket;    // 用于 DMA 访问系统内存

    sc_out<bool> interrupt_out; // 模拟中断线

    // 模拟 DMA 寄存器
    uint64_t dma_src_addr;
    uint64_t dma_dst_addr;
    unsigned int dma_length;
    unsigned int dma_control; // 0x1 = start DMA, 0x0 = idle

    // PEQ 用于调度 DMA 传输的完成时间
    tlm_utils::peq_with_get<tlm::tlm_generic_payload> dma_peq;

    // 存储当前正在进行的 DMA 事务
    tlm::tlm_generic_payload* current_dma_gp;
    sc_event dma_transfer_done_event; // DMA 传输完成事件，用于在 Device 内部同步

    SC_CTOR(DmaDevice) :
        mmio_socket("mmio_socket"),
        mem_socket("mem_socket"),
        interrupt_out("interrupt_out"),
        dma_peq("dma_peq")
    {
        mmio_socket.register_nb_transport_fw(this, &DmaDevice::nb_transport_fw_mmio);
        mem_socket.register_nb_transport_bw(this, &DmaDevice::nb_transport_bw_mem);

        dma_src_addr = 0;
        dma_dst_addr = 0;
        dma_length = 0;
        dma_control = 0;
        current_dma_gp = nullptr;

        SC_THREAD(dma_engine_thread);
        SC_THREAD(dma_peq_monitor_thread); // 监听 PEQ 弹出事件
        interrupt_out.initialize(false); // 初始化中断线为低
    }

    // 接收来自 CPU 的 MMIO 访问 (通过 mmio_socket)
    virtual tlm::tlm_sync_enum nb_transport_fw_mmio(
        tlm::tlm_generic_payload& trans, tlm::tlm_phase& phase, sc_core::sc_time& delay)
    {
        sc_dt::uint64   addr = trans.get_address();
        unsigned int    len  = trans.get_data_length();
        unsigned char* data = trans.get_data_ptr();
        tlm::tlm_command cmd = trans.get_command();

        std::cout << sc_time_stamp() << ": DmaDevice: MMIO access (0x" << std::hex << addr
                  << ") Phase: " << phase_to_string(phase) << ", Cmd: "
                  << (cmd == tlm::TLM_READ_COMMAND ? "READ" : "WRITE")
                  << ", Delay: " << delay << std::endl;

        if (phase == tlm::BEGIN_REQ) {
            if (cmd == tlm::TLM_WRITE_COMMAND) {
                if (addr == 0x0) { // DMA 源地址寄存器
                    dma_src_addr = *(reinterpret_cast<uint64_t*>(data));
                    std::cout << "DmaDevice: Configured DMA src_addr = 0x" << std::hex << dma_src_addr << std::endl;
                } else if (addr == 0x8) { // DMA 目的地址寄存器
                    dma_dst_addr = *(reinterpret_cast<uint64_t*>(data));
                    std::cout << "DmaDevice: Configured DMA dst_addr = 0x" << std::hex << dma_dst_addr << std::endl;
                } else if (addr == 0x10) { // DMA 长度寄存器
                    dma_length = *(reinterpret_cast<unsigned int*>(data));
                    std::cout << "DmaDevice: Configured DMA length = " << std::dec << dma_length << " bytes" << std::endl;
                } else if (addr == 0x18) { // DMA 控制寄存器 (启动 DMA)
                    dma_control = *(reinterpret_cast<unsigned int*>(data));
                    if (dma_control == 1) {
                        std::cout << sc_time_stamp() << ": DmaDevice: Received DMA start command! " << std::endl;
                        dma_transfer_done_event.notify(); // 触发 DMA 引擎线程开始工作
                    }
                }
            } else if (cmd == tlm::TLM_READ_COMMAND) {
                 if (addr == 0x18) { // 读取 DMA 状态 (例如，0表示完成，1表示进行中)
                    *(reinterpret_cast<unsigned int*>(data)) = dma_control;
                    std::cout << "DmaDevice: Read DMA control/status: " << dma_control << std::endl;
                }
            }

            trans.set_response_status(tlm::TLM_OK_RESPONSE);

            // 这是一个 MMIO 访问，通常很快完成
            // 为了模拟真实的 PCIe 延迟，这里可以加上 delay
            // delay += sc_time(10, SC_NS); // 模拟 MMIO 访问延迟

            // 将事务推入 PEQ，在延迟后处理响应
            dma_peq.notify(trans, phase, delay);
            return tlm::TLM_ACCEPTED; // 接受请求，等待PEQ处理并返回响应
        } else if (phase == tlm::END_RESP) {
            // CPU 发起的 END_RESP 消息，表明它已处理完响应
            // 对于 Target，通常不做特殊处理
            return tlm::TLM_COMPLETED;
        }

        return tlm::TLM_ACCEPTED;
    }

    // 接收来自 Memory 的反向路径传输 (通过 mem_socket)
    virtual tlm::tlm_sync_enum nb_transport_bw_mem(
        tlm::tlm_generic_payload& trans, tlm::tlm_phase& phase, sc_core::sc_time& delay)
    {
        std::cout << sc_time_stamp() << ": DmaDevice: Received BW from Memory for 0x"
                  << std::hex << trans.get_address() << ", Phase: " << phase_to_string(phase)
                  << ", Delay: " << delay << std::endl;

        // 如果是 DMA 引擎发起的事务，并且接收到 END_RESP
        if (trans.get_response_status() == tlm::TLM_OK_RESPONSE && phase == tlm::BEGIN_RESP) {
            // DMA 传输完成，通知 DMA 引擎线程
            current_dma_gp = &trans; // 存储已完成的事务，以便 dma_peq_monitor_thread 处理
            dma_peq.notify(trans, phase, delay); // 将响应推入 PEQ，延迟后处理
        }
        return tlm::TLM_ACCEPTED;
    }

    virtual void invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range) {}
    virtual bool get_direct_mem_ptr(tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data) { return false; }


    // DMA 引擎线程 (核心逻辑)
    void dma_engine_thread() {
        while (true) {
            wait(dma_transfer_done_event); // 等待 CPU 通过 MMIO 启动 DMA

            std::cout << sc_time_stamp() << ": DmaDevice: DMA Engine starting transfer..." << std::endl;

            // 模拟 DMA 读写
            tlm::tlm_generic_payload* gp = new tlm::tlm_generic_payload();
            unsigned char* data_buffer = new unsigned char[dma_length];

            // 示例：从源地址读取数据到本地缓冲区
            gp->set_command(tlm::TLM_READ_COMMAND);
            gp->set_address(dma_src_addr);
            gp->set_data_ptr(data_buffer);
            gp->set_data_length(dma_length);
            gp->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE); // 初始状态

            tlm::tlm_phase phase = tlm::BEGIN_REQ;
            sc_time delay = sc_time(dma_length, SC_NS); // 模拟 DMA 读延迟（每字节1ns）

            std::cout << sc_time_stamp() << ": DmaDevice: DMA Engine initiating READ from 0x"
                      << std::hex << dma_src_addr << " for " << std::dec << dma_length << " bytes." << std::endl;

            mem_socket->nb_transport_fw(*gp, phase, delay);

            // 等待来自 Memory 的响应，通常通过 dma_peq_monitor_thread 间接通知
            // 简单起见，这里假设nb_transport_fw会立即返回，并且dma_peq_monitor_thread会处理完成
            // 真实情况会更复杂，需要等待 bw_transport 回调
            // 简化：模拟等待 Read 传输完成
            wait(sc_time(dma_length, SC_NS)); // 实际的等待应该由 PEQ 完成后通知

            // 模拟数据准备好，写入目标地址
            gp->set_command(tlm::TLM_WRITE_COMMAND);
            gp->set_address(dma_dst_addr);
            gp->set_data_ptr(data_buffer); // 使用读取到的数据
            gp->set_data_length(dma_length);
            gp->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

            phase = tlm::BEGIN_REQ;
            delay = sc_time(dma_length, SC_NS); // 模拟 DMA 写延迟

            std::cout << sc_time_stamp() << ": DmaDevice: DMA Engine initiating WRITE to 0x"
                      << std::hex << dma_dst_addr << " for " << std::dec << dma_length << " bytes." << std::endl;

            mem_socket->nb_transport_fw(*gp, phase, delay);

            // 模拟等待 Write 传输完成
            wait(sc_time(dma_length, SC_NS)); // 实际的等待应该由 PEQ 完成后通知


            // DMA 传输完成，通知 CPU (模拟中断)
            dma_control = 0; // 设置状态寄存器为完成
            interrupt_out.write(true); // 拉高中断线
            std::cout << sc_time_stamp() << ": DmaDevice: DMA transfer completed. Asserting interrupt." << std::endl;
            wait(1, SC_NS); // 模拟中断持续时间
            interrupt_out.write(false); // 拉低中断线
            std::cout << sc_time_stamp() << ": DmaDevice: De-asserting interrupt." << std::endl;

            // 释放资源
            delete[] data_buffer;
            delete gp;
        }
    }

    // PEQ 监视线程
    void dma_peq_monitor_thread() {
        while (true) {
            tlm::tlm_generic_payload* trans = dma_peq.get(); // 阻塞直到 PEQ 有事务到期
            tlm::tlm_phase phase = tlm::UNINITIALIZED_PHASE; // PEQ get() 返回时 phase 已更新
            // 注意：trans->get_extension<tlm::tlm_phase_extension>()->get_phase() 在 PEQ get() 后是正确的
            // 假设 PEQ 已经在内部处理了 phase 的更新

            std::cout << sc_time_stamp() << ": DmaDevice: PEQ processed trans for 0x"
                      << std::hex << trans->get_address() << ", new phase "
                      << phase_to_string(trans->get_extension<tlm::tlm_phase_extension>()->get_phase())
                      << std::endl;

            // 这里可以根据 phase 和事务类型做更多复杂的处理
            // 例如，如果是 MMIO 响应，则不再处理
            // 如果是 DMA 传输完成的响应 (来自 Memory)，则可以进一步通知 dma_engine_thread
            // 但在当前模型中，dma_engine_thread 使用 wait(sc_time) 进行简化模拟
            // 真实情况应该由 bw_transport_bw 收到 MEM 的 BEGIN_RESP，然后 PEQ 弹出后通知 dma_engine_thread
            // 来驱动 DMA 引擎的下一个状态
            if (trans == current_dma_gp && trans->get_extension<tlm::tlm_phase_extension>()->get_phase() == tlm::BEGIN_RESP) {
                // 这是来自内存的 DMA 响应，表明 DMA 传输完成了一部分
                // 在这个简化模型中，我们直接假设 DMA 引擎会自己处理整个传输过程
                // 更复杂时，这里需要通知 dma_engine_thread 进行下一步操作
                // 例如 dma_transfer_done_event.notify();
            }

            // 如果是 CPU 发起的 MMIO 请求，我们现在可以通知 CPU (如果需要)
            // mem_socket->nb_transport_bw(*trans, trans->get_extension<tlm::tlm_phase_extension>()->get_phase(), SC_ZERO_TIME);
            // 但对于简单的 MMIO，通常由 target 直接返回 COMPLETED
        }
    }
};

// ----------------------------------------------------------------------------
// Host (CPU) 模块
// ----------------------------------------------------------------------------
SC_MODULE(Host) :
    public tlm::tlm_bw_transport_if<tlm::tlm_base_protocol_types> // 接收来自 DmaDevice 的 MMIO 响应
{
    tlm_utils::simple_initiator_socket<Host> mmio_socket; // 用于 MMIO 访问 DmaDevice
    tlm_utils::simple_initiator_socket<Host> mem_socket;  // 用于访问 Memory

    sc_in<bool> interrupt_in; // 模拟中断线

    SC_CTOR(Host) :
        mmio_socket("mmio_socket"),
        mem_socket("mem_socket"),
        interrupt_in("interrupt_in")
    {
        mmio_socket.register_bw_transport(this, &Host::nb_transport_bw_mmio);
        mem_socket.register_bw_transport(this, &Host::nb_transport_bw_mem);

        SC_THREAD(main_thread);
        SC_METHOD(interrupt_handler);
        sensitive << interrupt_in.pos(); // 敏感于中断线的上升沿
        dont_initialize(); // 不在仿真开始时立即执行
    }

    // 接收来自 DmaDevice 的 MMIO 响应
    virtual tlm::tlm_sync_enum nb_transport_bw(
        tlm::tlm_generic_payload& trans, tlm::tlm_phase& phase, sc_core::sc_time& delay)
    {
        std::cout << sc_time_stamp() << ": Host: Received BW from DmaDevice (MMIO) for 0x"
                  << std::hex << trans.get_address() << ", Phase: " << phase_to_string(phase)
                  << ", Delay: " << delay << std::endl;

        if (phase == tlm::BEGIN_RESP) {
            // CPU 收到响应，可以发送 END_RESP
            phase = tlm::END_RESP;
            delay = SC_ZERO_TIME; // 立即响应
            return tlm::TLM_COMPLETED; // 完成事务
        }
        return tlm::TLM_ACCEPTED;
    }

    // 接收来自 Memory 的响应 (如果 Host 直接访问内存)
    virtual tlm::tlm_sync_enum nb_transport_bw_mem(
        tlm::tlm_generic_payload& trans, tlm::tlm_phase& phase, sc_core::sc_time& delay)
    {
        std::cout << sc_time_stamp() << ": Host: Received BW from Memory for 0x"
                  << std::hex << trans.get_address() << ", Phase: " << phase_to_string(phase)
                  << ", Delay: " << delay << std::endl;
        if (phase == tlm::BEGIN_RESP) {
            phase = tlm::END_RESP;
            delay = SC_ZERO_TIME;
            return tlm::TLM_COMPLETED;
        }
        return tlm::TLM_ACCEPTED;
    }


    virtual void invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range) {}

    // 主线程 (模拟 CPU 驱动程序逻辑)
    void main_thread() {
        // 等待一段时间，确保所有模块初始化完成
        wait(10, SC_NS);

        std::cout << sc_time_stamp() << ": Host: Starting DMA setup..." << std::endl;

        // --------------------------------------------------------------------
        // 1. 初始化内存中的数据
        // --------------------------------------------------------------------
        unsigned char data_to_write[16];
        for (int i = 0; i < 16; ++i) {
            data_to_write[i] = i + 0x10; // 0x10, 0x11, ... 0x1F
        }
        unsigned int initial_mem_addr = 0x100; // 内存起始地址

        tlm::tlm_generic_payload* write_gp = new tlm::tlm_generic_payload();
        write_gp->set_command(tlm::TLM_WRITE_COMMAND);
        write_gp->set_address(initial_mem_addr);
        write_gp->set_data_ptr(data_to_write);
        write_gp->set_data_length(16);
        write_gp->set_streaming_width(16);
        write_gp->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        tlm::tlm_phase phase = tlm::BEGIN_REQ;
        sc_time delay = SC_ZERO_TIME;

        std::cout << sc_time_stamp() << ": Host: Writing initial data to Memory (0x" << std::hex << initial_mem_addr << ")..." << std::endl;
        mem_socket->nb_transport_fw(*write_gp, phase, delay);
        // 通常需要等待 BEGIN_RESP，这里简化为立即继续
        wait(sc_time(10, SC_NS)); // 模拟写内存的延迟

        delete write_gp;


        // --------------------------------------------------------------------
        // 2. 配置 DMA 寄存器 (通过 MMIO)
        //    期望：从 Memory 0x100 读取 16 字节，写入 Memory 0x200
        // --------------------------------------------------------------------
        uint64_t src_addr = initial_mem_addr;
        uint64_t dst_addr = 0x200;
        unsigned int length = 16;
        unsigned int dma_start_cmd = 1;

        // 写入源地址
        mmio_write(0x0, src_addr);
        // 写入目的地址
        mmio_write(0x8, dst_addr);
        // 写入长度
        mmio_write(0x10, length);

        // --------------------------------------------------------------------
        // 3. 启动 DMA (通过 MMIO)
        // --------------------------------------------------------------------
        std::cout << sc_time_stamp() << ": Host: Issuing DMA start command (MMIO 0x18 = 1)..." << std::endl;
        mmio_write(0x18, dma_start_cmd);

        // --------------------------------------------------------------------
        // 4. CPU 等待中断 (或轮询状态寄存器)
        // --------------------------------------------------------------------
        std::cout << sc_time_stamp() << ": Host: Waiting for DMA completion interrupt..." << std::endl;
        wait(interrupt_received_event); // 等待中断处理器通知

        std::cout << sc_time_stamp() << ": Host: DMA completion interrupt received!" << std::endl;

        // --------------------------------------------------------------------
        // 5. 检查 DMA 结果 (从目的地址读取数据)
        // --------------------------------------------------------------------
        unsigned char read_back_data[16];
        tlm::tlm_generic_payload* read_gp = new tlm::tlm_generic_payload();
        read_gp->set_command(tlm::TLM_READ_COMMAND);
        read_gp->set_address(dst_addr);
        read_gp->set_data_ptr(read_back_data);
        read_gp->set_data_length(16);
        read_gp->set_streaming_width(16);
        read_gp->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        phase = tlm::BEGIN_REQ;
        delay = SC_ZERO_TIME;

        std::cout << sc_time_stamp() << ": Host: Reading data from Memory (0x" << std::hex << dst_addr << ") to verify DMA..." << std::endl;
        mem_socket->nb_transport_fw(*read_gp, phase, delay);
        wait(sc_time(10, SC_NS)); // 模拟读内存的延迟

        std::cout << sc_time_stamp() << ": Host: Data read from 0x" << std::hex << dst_addr << ": ";
        for (int i = 0; i < 16; ++i) {
            std::cout << std::hex << (int)read_back_data[i] << " ";
        }
        std::cout << std::endl;

        // 验证数据是否正确传输
        bool success = true;
        for (int i = 0; i < 16; ++i) {
            if (read_back_data[i] != (unsigned char)(i + 0x10)) {
                success = false;
                break;
            }
        }
        if (success) {
            std::cout << sc_time_stamp() << ": Host: DMA verification SUCCESS!" << std::endl;
        } else {
            std::cout << sc_time_stamp() << ": Host: DMA verification FAILED!" << std::endl;
        }

        delete read_gp;
        sc_stop();
    }

    // 辅助函数：MMIO 写入
    template<typename T>
    void mmio_write(sc_dt::uint64 addr, T value) {
        tlm::tlm_generic_payload* gp = new tlm::tlm_generic_payload();
        gp->set_command(tlm::TLM_WRITE_COMMAND);
        gp->set_address(addr);
        gp->set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        gp->set_data_length(sizeof(T));
        gp->set_streaming_width(sizeof(T));
        gp->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        tlm::tlm_phase phase = tlm::BEGIN_REQ;
        sc_time delay = SC_ZERO_TIME; // MMIO 写入通常从 0 delay 开始

        mmio_socket->nb_transport_fw(*gp, phase, delay);
        // 这里模拟等待 MMIO 写入完成（如果需要等待 BEGIN_RESP）
        // 对于同步模型，我们直接阻塞，对于AT/CA模型，我们会等待PEQ或回调
        // 这里简化为发送后等待一个delta周期
        wait(1, SC_NS); // 模拟 MMIO 传输延迟
        delete gp;
    }

    // 辅助函数：MMIO 读取 (未在主线程中使用，但可用于完整性)
    template<typename T>
    T mmio_read(sc_dt::uint64 addr) {
        T value;
        tlm::tlm_generic_payload* gp = new tlm::tlm_generic_payload();
        gp->set_command(tlm::TLM_READ_COMMAND);
        gp->set_address(addr);
        gp->set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        gp->set_data_length(sizeof(T));
        gp->set_streaming_width(sizeof(T));
        gp->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        tlm::tlm_phase phase = tlm::BEGIN_REQ;
        sc_time delay = SC_ZERO_TIME;

        mmio_socket->nb_transport_fw(*gp, phase, delay);
        wait(1, SC_NS); // 模拟 MMIO 传输延迟
        delete gp;
        return value;
    }

    // 中断处理方法
    sc_event interrupt_received_event;
    void interrupt_handler() {
        if (interrupt_in.read() == true) {
            std::cout << sc_time_stamp() << ": Host: Hardware Interrupt asserted!" << std::endl;
            interrupt_received_event.notify(); // 通知主线程 DMA 完成
        }
    }
};

// ----------------------------------------------------------------------------
// Top 模块
// ----------------------------------------------------------------------------
SC_MODULE(DMA_Top) {
    Host host;
    DmaDevice dma_device;
    Memory memory;

    SC_CTOR(DMA_Top) :
        host("host"),
        dma_device("dma_device"),
        memory("memory")
    {
        // Host 的 MMIO socket 连接到 DmaDevice 的 MMIO socket
        host.mmio_socket.bind(dma_device.mmio_socket);
        // Host 的 Memory socket 连接到 Memory 的 socket
        host.mem_socket.bind(memory.socket);
        // DmaDevice 的 Memory socket 连接到 Memory 的 socket (用于 DMA 传输)
        dma_device.mem_socket.bind(memory.socket);

        // 连接中断线
        host.interrupt_in(dma_device.interrupt_out);
    }
};

/*
int sc_main(int argc, char* argv[]) {
    Top top("top");
    sc_start();
    return 0;
}
*/

#endif // DMA_ENGINE_H