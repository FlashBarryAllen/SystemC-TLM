#include <iostream>
#include <vector>
#include <queue>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <memory>
#include <chrono>
#include <bitset>
#include <cassert>
#include "pcie_arb.h"

// PCIe Transaction Types
enum class TransactionType {
    MEMORY_READ,
    MEMORY_WRITE,
    IO_READ,
    IO_WRITE,
    CONFIG_READ,
    CONFIG_WRITE,
    MESSAGE,
    COMPLETION
};

// Traffic Class and Virtual Channel
enum class TrafficClass : uint8_t {
    TC0 = 0, TC1 = 1, TC2 = 2, TC3 = 3,
    TC4 = 4, TC5 = 5, TC6 = 6, TC7 = 7
};

enum class VirtualChannel : uint8_t {
    VC0 = 0, VC1 = 1, VC2 = 2, VC3 = 3,
    VC4 = 4, VC5 = 5, VC6 = 6, VC7 = 7
};

// PCIe Packet Structure
struct PCIePacket {
    uint32_t requester_id;
    uint32_t destination_id;
    uint32_t address;
    uint32_t data_length;
    TransactionType type;
    TrafficClass tc;
    VirtualChannel vc;
    uint32_t tag;
    uint8_t ingress_port;
    uint8_t egress_port;
    uint8_t priority;
    std::chrono::steady_clock::time_point timestamp;
    //std::vector<uint8_t> data;
    
    PCIePacket(uint32_t req_id, uint32_t dest_id, uint32_t addr, 
               uint32_t len, TransactionType t, TrafficClass traffic_class,
               VirtualChannel virtual_channel, uint32_t tg, uint8_t ing_port,
               uint8_t eg_port, uint8_t prio = 0)
        : requester_id(req_id), destination_id(dest_id), address(addr),
          data_length(len), type(t), tc(traffic_class), vc(virtual_channel),
          tag(tg), ingress_port(ing_port), egress_port(eg_port),
          priority(prio), timestamp(std::chrono::steady_clock::now()) {
        //data.resize(data_length, 0);
    }
};

// Port Configuration
struct PortConfig {
    uint8_t port_number;
    bool enabled;
    uint32_t max_bandwidth;     // Mbps
    uint32_t current_bandwidth;
    uint32_t packet_count;
    std::map<VirtualChannel, uint32_t> vc_credits;
    std::map<TrafficClass, uint8_t> tc_priority_map;
    
    PortConfig(uint8_t port_num) 
        : port_number(port_num), enabled(true), max_bandwidth(8000),
          current_bandwidth(0), packet_count(0) {
        // Initialize VC credits
        for (int i = 0; i < 8; i++) {
            vc_credits[static_cast<VirtualChannel>(i)] = 256;
        }
        // Initialize TC priority mapping
        for (int i = 0; i < 8; i++) {
            tc_priority_map[static_cast<TrafficClass>(i)] = 7 - i;
        }
    }
};

// Arbitration Request
struct ArbitrationRequest {
    uint8_t ingress_port;
    uint8_t egress_port;
    uint8_t priority;
    VirtualChannel vc;
    std::shared_ptr<PCIePacket> packet;
    std::chrono::steady_clock::time_point request_time;
    
    ArbitrationRequest(uint8_t ing_port, uint8_t eg_port, uint8_t prio,
                      VirtualChannel virtual_channel, 
                      std::shared_ptr<PCIePacket> pkt)
        : ingress_port(ing_port), egress_port(eg_port), priority(prio),
          vc(virtual_channel), packet(pkt),
          request_time(std::chrono::steady_clock::now()) {}
};

// Priority Comparator for Arbitration
struct ArbitrationComparator {
    bool operator()(const std::shared_ptr<ArbitrationRequest>& a,
                   const std::shared_ptr<ArbitrationRequest>& b) const {
        // Higher priority first (reverse order for priority queue)
        if (a->priority != b->priority) {
            return a->priority < b->priority;
        }
        
        // If same priority, use ingress port number (lower port number first)
        if (a->ingress_port != b->ingress_port) {
            return a->ingress_port > b->ingress_port;
        }
        
        // If same ingress port, use timestamp (earlier request first)
        return a->request_time > b->request_time;
    }
};

// PCIe Switch Arbitration Engine
class PCIeSwitchArbitrator {
private:
    // Port configurations
    std::map<uint8_t, std::unique_ptr<PortConfig>> ingress_ports;
    std::map<uint8_t, std::unique_ptr<PortConfig>> egress_ports;
    
    // Arbitration queues per egress port
    std::map<uint8_t, std::priority_queue<
        std::shared_ptr<ArbitrationRequest>,
        std::vector<std::shared_ptr<ArbitrationRequest>>,
        ArbitrationComparator>> arbitration_queues;
    
    // Packet buffers per ingress port
    std::map<uint8_t, std::queue<std::shared_ptr<PCIePacket>>> ingress_buffers;
    
    // Routing table: destination_id -> egress_port
    std::unordered_map<uint32_t, uint8_t> routing_table;
    
    // Statistics
    struct Statistics {
        uint64_t total_packets_processed = 0;
        uint64_t total_bytes_processed = 0;
        std::map<uint8_t, uint64_t> port_packet_count;
        std::map<uint8_t, uint64_t> port_byte_count;
        std::map<uint8_t, uint64_t> arbitration_wins;
        uint64_t arbitration_conflicts = 0;
        uint64_t dropped_packets = 0;
    } stats;
    
    // Round-robin state for equal priority arbitration
    std::map<uint8_t, uint8_t> last_served_ingress_port;
    
public:
    PCIeSwitchArbitrator() {
        initializePorts();
        initializeRoutingTable();
    }
    
    // Initialize ports based on the diagram
    void initializePorts() {
        // Initialize ingress ports (0, 1, 2, 3)
        for (uint8_t i = 0; i < 4; i++) {
            ingress_ports[i] = std::make_unique<PortConfig>(i);
            ingress_buffers[i] = std::queue<std::shared_ptr<PCIePacket>>();
            stats.port_packet_count[i] = 0;
            stats.port_byte_count[i] = 0;
        }
        
        // Initialize egress ports (2, 3) - based on diagram
        for (uint8_t i = 2; i < 4; i++) {
            egress_ports[i] = std::make_unique<PortConfig>(i);
            arbitration_queues[i] = std::priority_queue<
                std::shared_ptr<ArbitrationRequest>,
                std::vector<std::shared_ptr<ArbitrationRequest>>,
                ArbitrationComparator>();
            stats.arbitration_wins[i] = 0;
            last_served_ingress_port[i] = 0;
        }
    }
    
    // Initialize routing table
    void initializeRoutingTable() {
        // Example routing: even destination IDs -> port 2, odd -> port 3
        for (uint32_t dest = 0x1000; dest < 0x2000; dest++) {
            routing_table[dest] = (dest % 2 == 0) ? 2 : 3;
        }
    }
    
    // Add custom routing entry
    void addRoute(uint32_t destination_id, uint8_t egress_port) {
        if (egress_ports.find(egress_port) != egress_ports.end()) {
            routing_table[destination_id] = egress_port;
        }
    }
    
    // Configure port priority mapping
    void configurePortPriority(uint8_t port, TrafficClass tc, uint8_t priority) {
        if (ingress_ports.find(port) != ingress_ports.end()) {
            ingress_ports[port]->tc_priority_map[tc] = priority;
        }
    }
    
    // Receive packet on ingress port
    bool receivePacket(std::shared_ptr<PCIePacket> packet) {
        uint8_t ingress_port = packet->ingress_port;
        
        // Validate ingress port
        if (ingress_ports.find(ingress_port) == ingress_ports.end() ||
            !ingress_ports[ingress_port]->enabled) {
            stats.dropped_packets++;
            return false;
        }
        
        // Check VC credits
        auto& port_config = ingress_ports[ingress_port];
        if (port_config->vc_credits[packet->vc] == 0) {
            stats.dropped_packets++;
            return false;
        }
        
        // Consume VC credit
        port_config->vc_credits[packet->vc]--;
        
        // Determine egress port using routing table
        uint8_t egress_port = routePacket(packet);
        if (egress_port == 0xFF) {
            // No route found, drop packet
            stats.dropped_packets++;
            return false;
        }
        
        packet->egress_port = egress_port;
        
        // Set priority based on TC
        packet->priority = port_config->tc_priority_map[packet->tc];
        
        // Add to ingress buffer
        ingress_buffers[ingress_port].push(packet);
        
        // Update statistics
        port_config->packet_count++;
        stats.port_packet_count[ingress_port]++;
        stats.port_byte_count[ingress_port] += packet->data_length;
        
        return true;
    }
    
    // Route packet to determine egress port
    uint8_t routePacket(std::shared_ptr<PCIePacket> packet) {
        auto route_it = routing_table.find(packet->destination_id);
        if (route_it != routing_table.end()) {
            return route_it->second;
        }
        
        // Default routing based on address ranges
        if (packet->address < 0x80000000) {
            return 2;  // Lower addresses to port 2
        } else {
            return 3;  // Higher addresses to port 3
        }
    }
    
    // Process ingress buffers and create arbitration requests
    void processIngressBuffers() {
        for (auto& [port_num, buffer] : ingress_buffers) {
            if (!buffer.empty()) {
                auto packet = buffer.front();
                buffer.pop();
                
                // Create arbitration request
                auto request = std::make_shared<ArbitrationRequest>(
                    packet->ingress_port, packet->egress_port,
                    packet->priority, packet->vc, packet);
                
                // Add to appropriate arbitration queue
                if (arbitration_queues.find(packet->egress_port) != 
                    arbitration_queues.end()) {
                    arbitration_queues[packet->egress_port].push(request);
                }
            }
        }
    }
    
    // Perform arbitration for a specific egress port
    std::shared_ptr<PCIePacket> arbitrateEgressPort(uint8_t egress_port) {
        auto& queue = arbitration_queues[egress_port];
        
        if (queue.empty()) {
            return nullptr;
        }
        
        // Check for conflicts (multiple requests with same priority)
        std::vector<std::shared_ptr<ArbitrationRequest>> same_priority_requests;
        uint8_t highest_priority = queue.top()->priority;
        
        // Collect all requests with highest priority
        std::vector<std::shared_ptr<ArbitrationRequest>> temp_storage;
        while (!queue.empty() && queue.top()->priority == highest_priority) {
            same_priority_requests.push_back(queue.top());
            temp_storage.push_back(queue.top());
            queue.pop();
        }
        
        // Put back lower priority requests
        for (auto& req : temp_storage) {
            if (req->priority < highest_priority) {
                queue.push(req);
            }
        }
        
        std::shared_ptr<ArbitrationRequest> selected_request;
        
        if (same_priority_requests.size() > 1) {
            // Conflict detected - use round-robin among same priority
            stats.arbitration_conflicts++;
            selected_request = roundRobinArbitration(egress_port, same_priority_requests);
            
            // Put back non-selected requests
            for (auto& req : same_priority_requests) {
                if (req != selected_request) {
                    queue.push(req);
                }
            }
        } else {
            // No conflict, select the highest priority request
            selected_request = same_priority_requests[0];
        }
        
        // Update statistics
        stats.arbitration_wins[egress_port]++;
        stats.total_packets_processed++;
        stats.total_bytes_processed += selected_request->packet->data_length;
        
        return selected_request->packet;
    }
    
    // Round-robin arbitration for same priority requests
    std::shared_ptr<ArbitrationRequest> roundRobinArbitration(
        uint8_t egress_port, 
        std::vector<std::shared_ptr<ArbitrationRequest>>& requests) {
        
        // Sort by ingress port number
        std::sort(requests.begin(), requests.end(),
                  [](const auto& a, const auto& b) {
                      return a->ingress_port < b->ingress_port;
                  });
        
        // Find next port after last served
        uint8_t last_served = last_served_ingress_port[egress_port];
        auto it = std::find_if(requests.begin(), requests.end(),
                              [last_served](const auto& req) {
                                  return req->ingress_port > last_served;
                              });
        
        if (it == requests.end()) {
            // Wrap around to first port
            it = requests.begin();
        }
        
        last_served_ingress_port[egress_port] = (*it)->ingress_port;
        return *it;
    }
    
    // Main arbitration cycle
    void arbitrationCycle() {
        // Process all ingress buffers
        processIngressBuffers();
        
        // Arbitrate for each egress port
        for (auto& [egress_port, queue] : arbitration_queues) {
            auto winning_packet = arbitrateEgressPort(egress_port);
            if (winning_packet) {
                // Transmit packet (simulate)
                transmitPacket(winning_packet);
            }
        }
    }
    
    // Simulate packet transmission
    void transmitPacket(std::shared_ptr<PCIePacket> packet) {
        // Update egress port statistics
        auto& egress_config = egress_ports[packet->egress_port];
        egress_config->packet_count++;
        egress_config->current_bandwidth += packet->data_length;
        
        // Restore VC credits (simulate completion)
        restoreVCCredits(packet->ingress_port, packet->vc, 1);
        
        std::cout << "Transmitted packet: Ingress Port " 
                  << static_cast<int>(packet->ingress_port)
                  << " -> Egress Port " << static_cast<int>(packet->egress_port)
                  << ", Priority: " << static_cast<int>(packet->priority)
                  << ", VC: " << static_cast<int>(packet->vc)
                  << ", Length: " << packet->data_length << " bytes"
                  << std::endl;
    }
    
    // Restore VC credits
    void restoreVCCredits(uint8_t port, VirtualChannel vc, uint32_t credits) {
        if (ingress_ports.find(port) != ingress_ports.end()) {
            auto& port_config = ingress_ports[port];
            port_config->vc_credits[vc] = std::min(
                port_config->vc_credits[vc] + credits, 256u);
        }
    }
    
    // Get port statistics
    void printPortStatistics() const {
        std::cout << "\n=== PCIe Switch Arbitration Statistics ===\n";
        
        std::cout << "\nIngress Ports:\n";
        for (const auto& [port_num, config] : ingress_ports) {
            std::cout << "Port " << static_cast<int>(port_num) 
                      << ": Packets=" << config->packet_count
                      << ", Enabled=" << (config->enabled ? "Yes" : "No")
                      << std::endl;
            
            std::cout << "  VC Credits: ";
            for (int i = 0; i < 8; i++) {
                std::cout << "VC" << i << "=" 
                          << config->vc_credits.at(static_cast<VirtualChannel>(i)) << " ";
            }
            std::cout << std::endl;
        }
        
        std::cout << "\nEgress Ports:\n";
        for (const auto& [port_num, config] : egress_ports) {
            std::cout << "Port " << static_cast<int>(port_num) 
                      << ": Packets=" << config->packet_count
                      << ", Wins=" << stats.arbitration_wins.at(port_num)
                      << ", Bandwidth=" << config->current_bandwidth << " bytes"
                      << std::endl;
        }
        
        std::cout << "\nOverall Statistics:\n";
        std::cout << "Total Packets Processed: " << stats.total_packets_processed << std::endl;
        std::cout << "Total Bytes Processed: " << stats.total_bytes_processed << std::endl;
        std::cout << "Arbitration Conflicts: " << stats.arbitration_conflicts << std::endl;
        std::cout << "Dropped Packets: " << stats.dropped_packets << std::endl;
    }
    
    // Get queue depths for monitoring
    void printQueueStatus() const {
        std::cout << "\n=== Queue Status ===\n";
        for (const auto& [port_num, buffer] : ingress_buffers) {
            std::cout << "Ingress Port " << static_cast<int>(port_num) 
                      << " Buffer: " << buffer.size() << " packets" << std::endl;
        }
        
        for (const auto& [port_num, queue] : arbitration_queues) {
            std::cout << "Egress Port " << static_cast<int>(port_num) 
                      << " Arbitration Queue: " << queue.size() << " requests" << std::endl;
        }
    }
};

// Test and demonstration
int pcie_arb_main() {
    PCIeSwitchArbitrator arbitrator;
    
    // Configure port priorities
    arbitrator.configurePortPriority(0, TrafficClass::TC0, 3);  // Low priority
    arbitrator.configurePortPriority(1, TrafficClass::TC1, 3);  // Low priority
    arbitrator.configurePortPriority(2, TrafficClass::TC2, 2);  // Medium priority
    arbitrator.configurePortPriority(3, TrafficClass::TC3, 1);  // High priority
    
    // Add some routing entries
    arbitrator.addRoute(0x1000, 2);
    arbitrator.addRoute(0x1001, 3);
    arbitrator.addRoute(0x1002, 2);
    arbitrator.addRoute(0x1003, 3);
    
    // Create test packets
    std::vector<std::shared_ptr<PCIePacket>> test_packets;
    
    // High priority packet from port 3
    test_packets.push_back(std::make_shared<PCIePacket>(
        0x3000, 0x1000, 0x10000000, 64, TransactionType::MEMORY_READ,
        TrafficClass::TC3, VirtualChannel::VC0, 1, 3, 2, 1));
    
    // Low priority packet from port 0
    test_packets.push_back(std::make_shared<PCIePacket>(
        0x0000, 0x1001, 0x20000000, 128, TransactionType::MEMORY_WRITE,
        TrafficClass::TC0, VirtualChannel::VC0, 2, 0, 3, 3));
    
    // Medium priority packet from port 2
    test_packets.push_back(std::make_shared<PCIePacket>(
        0x2000, 0x1002, 0x30000000, 256, TransactionType::MEMORY_READ,
        TrafficClass::TC2, VirtualChannel::VC1, 3, 2, 2, 2));
    
    // Another high priority packet from port 1 (conflict scenario)
    test_packets.push_back(std::make_shared<PCIePacket>(
        0x1000, 0x1003, 0x40000000, 512, TransactionType::MEMORY_WRITE,
        TrafficClass::TC1, VirtualChannel::VC0, 4, 1, 3, 3));
    
    // Receive packets
    std::cout << "Receiving test packets...\n";
    for (auto& packet : test_packets) {
        if (arbitrator.receivePacket(packet)) {
            std::cout << "Packet received on ingress port " 
                      << static_cast<int>(packet->ingress_port) << std::endl;
        } else {
            std::cout << "Packet dropped!" << std::endl;
        }
    }
    
    arbitrator.printQueueStatus();
    
    // Run arbitration cycles
    std::cout << "\nRunning arbitration cycles...\n";
    for (int cycle = 0; cycle < 5; cycle++) {
        std::cout << "\n--- Arbitration Cycle " << (cycle + 1) << " ---\n";
        arbitrator.arbitrationCycle();
        arbitrator.printQueueStatus();
    }
    
    // Print final statistics
    arbitrator.printPortStatistics();
    
    return 0;
}