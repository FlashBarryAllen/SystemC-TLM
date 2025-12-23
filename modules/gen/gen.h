#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <iomanip>

using namespace sc_core;
using namespace tlm;
using namespace tlm_utils;

// 数据包类型定义
enum PacketType {
    PACKET_READ = 0,
    PACKET_WRITE = 1,
    PACKET_CONTROL = 2,
    PACKET_DATA = 3
};

// 数据包结构
struct Packet {
    PacketType type;
    unsigned int id;
    sc_dt::uint64 address;
    unsigned char data[16];
    unsigned int length;
    sc_time timestamp;
    
    void print() const {
        cout << "  [Packet-" << id << "] 类型=";
        switch(type) {
            case PACKET_READ: cout << "READ   "; break;
            case PACKET_WRITE: cout << "WRITE  "; break;
            case PACKET_CONTROL: cout << "CONTROL"; break;
            case PACKET_DATA: cout << "DATA   "; break;
        }
        cout << ", 地址=0x" << hex << address 
             << ", 长度=" << dec << length 
             << ", 数据=[";
        for (unsigned int i = 0; i < min(length, 4u); i++) {
            cout << hex << setw(2) << setfill('0') << (int)data[i];
            if (i < min(length, 4u) - 1) cout << " ";
        }
        if (length > 4) cout << "...";
        cout << "]" << dec << endl;
    }
};

// 数据包发生器 (Generator)
class PacketGenerator : public sc_module {
public:
    simple_initiator_socket<PacketGenerator> initiator_socket;
    
    SC_HAS_PROCESS(PacketGenerator);
    
    PacketGenerator(sc_module_name name, 
                   int num_packets = 10,
                   int interval_ns = 50) : 
        sc_module(name),
        initiator_socket("initiator_socket"),
        total_packets(num_packets),
        packet_interval(interval_ns),
        packet_counter(0) {
        
        SC_THREAD(generate_process);
        
        // 初始化随机数种子
        srand(time(NULL));
    }
    
    void generate_process() {
        cout << "\n=== 发包器启动 ===" << endl;
        cout << "配置: 总数=" << total_packets 
             << ", 间隔=" << packet_interval << "ns\n" << endl;
        
        for (int i = 0; i < total_packets; i++) {
            // 生成随机数据包
            Packet pkt = generate_random_packet();
            
            cout << "Generator @ " << sc_time_stamp() 
                 << " - 生成数据包 #" << packet_counter << ":" << endl;
            pkt.print();
            
            // 发送数据包
            send_packet(pkt);
            
            // 等待发送间隔
            wait(packet_interval, SC_NS);
        }
        
        cout << "\n=== 发包器完成 ===" << endl;
        cout << "总共发送: " << packet_counter << " 个数据包\n" << endl;
    }
    
    Packet generate_random_packet() {
        Packet pkt;
        
        pkt.id = packet_counter++;
        pkt.type = static_cast<PacketType>(rand() % 4);
        pkt.address = (rand() % 64) * 4;  // 0x00 - 0xFC, 4字节对齐
        pkt.length = 1 + (rand() % 8);    // 1-8字节
        pkt.timestamp = sc_time_stamp();
        
        // 生成随机数据
        for (unsigned int i = 0; i < pkt.length; i++) {
            pkt.data[i] = rand() % 256;
        }
        
        return pkt;
    }
    
    void send_packet(const Packet& pkt) {
        tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;
        
        // 根据数据包类型设置TLM事务
        if (pkt.type == PACKET_READ) {
            trans.set_command(TLM_READ_COMMAND);
        } else {
            trans.set_command(TLM_WRITE_COMMAND);
        }
        
        trans.set_address(pkt.address);
        trans.set_data_ptr(const_cast<unsigned char*>(pkt.data));
        trans.set_data_length(pkt.length);
        trans.set_streaming_width(pkt.length);
        trans.set_byte_enable_ptr(0);
        trans.set_dmi_allowed(false);
        trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
        
        // 发送事务
        initiator_socket->b_transport(trans, delay);
        
        // 检查响应
        if (trans.is_response_error()) {
            cout << "  [错误] " << trans.get_response_string() << endl;
            error_count++;
        } else {
            cout << "  [成功] 数据包已发送, 延迟=" << delay << endl;
            success_count++;
        }
        
        wait(delay);
    }
    
    // 获取统计信息
    void print_statistics() {
        cout << "\n=== 发包器统计 ===" << endl;
        cout << "总发送: " << packet_counter << endl;
        cout << "成功: " << success_count << endl;
        cout << "失败: " << error_count << endl;
        cout << "成功率: " << (success_count * 100.0 / packet_counter) << "%" << endl;
    }
    
private:
    int total_packets;
    int packet_interval;
    unsigned int packet_counter;
    unsigned int success_count = 0;
    unsigned int error_count = 0;
};

// 数据包接收器 (Receiver) - 用于测试
class PacketReceiver : public sc_module {
public:
    simple_target_socket<PacketReceiver> target_socket;
    
    SC_CTOR(PacketReceiver) : target_socket("target_socket") {
        target_socket.register_b_transport(this, &PacketReceiver::b_transport);
        
        // 初始化存储器
        for (int i = 0; i < 256; i++) {
            memory[i] = 0;
        }
    }
    
    virtual void b_transport(tlm_generic_payload& trans, sc_time& delay) {
        tlm_command cmd = trans.get_command();
        sc_dt::uint64 addr = trans.get_address();
        unsigned char* ptr = trans.get_data_ptr();
        unsigned int len = trans.get_data_length();
        
        // 地址检查
        if (addr >= 256 || addr + len > 256) {
            trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
            cout << "Receiver: 地址错误 0x" << hex << addr << dec << endl;
            return;
        }
        
        if (cmd == TLM_READ_COMMAND) {
            // 读操作
            for (unsigned int i = 0; i < len; i++) {
                ptr[i] = memory[addr + i];
            }
            cout << "Receiver: READ  @ 0x" << hex << addr 
                 << ", 长度=" << dec << len << endl;
            received_reads++;
        }
        else if (cmd == TLM_WRITE_COMMAND) {
            // 写操作
            for (unsigned int i = 0; i < len; i++) {
                memory[addr + i] = ptr[i];
            }
            cout << "Receiver: WRITE @ 0x" << hex << addr 
                 << ", 长度=" << dec << len 
                 << ", 数据=[" << hex << setw(2) << setfill('0') << (int)ptr[0];
            for (unsigned int i = 1; i < min(len, 4u); i++) {
                cout << " " << setw(2) << (int)ptr[i];
            }
            if (len > 4) cout << "...";
            cout << "]" << dec << endl;
            received_writes++;
        }
        
        // 模拟处理延迟
        delay += sc_time(5, SC_NS);
        
        trans.set_response_status(TLM_OK_RESPONSE);
        total_received++;
    }
    
    void print_statistics() {
        cout << "\n=== 接收器统计 ===" << endl;
        cout << "总接收: " << total_received << endl;
        cout << "读操作: " << received_reads << endl;
        cout << "写操作: " << received_writes << endl;
    }
    
    void dump_memory(int start, int end) {
        cout << "\n=== 内存内容 [0x" << hex << start << "-0x" << end << "] ===" << endl;
        for (int i = start; i <= end; i++) {
            if ((i - start) % 16 == 0) {
                cout << "0x" << hex << setw(2) << setfill('0') << i << ": ";
            }
            cout << setw(2) << (int)memory[i] << " ";
            if ((i - start) % 16 == 15 || i == end) {
                cout << endl;
            }
        }
        cout << dec << endl;
    }
    
private:
    unsigned char memory[256];
    unsigned int total_received = 0;
    unsigned int received_reads = 0;
    unsigned int received_writes = 0;
};

// 顶层测试模块
class TestBench : public sc_module {
public:
    PacketGenerator* generator;
    PacketReceiver* receiver;
    
    SC_CTOR(TestBench) {
        // 创建发包器: 15个数据包, 每50ns发送一个
        generator = new PacketGenerator("generator", 15, 50);
        receiver = new PacketReceiver("receiver");
        
        // 连接
        generator->initiator_socket.bind(receiver->target_socket);
    }
    
    ~TestBench() {
        delete generator;
        delete receiver;
    }
};

