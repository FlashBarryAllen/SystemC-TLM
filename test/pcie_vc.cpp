#include <iostream>
#include <vector>
#include <queue>
#include <map>
#include <algorithm>
#include <memory>
#include <chrono>

// PCIe Transaction Types
enum class TransactionType {
    MEMORY_READ,
    MEMORY_WRITE,
    IO_READ,
    IO_WRITE,
    CONFIG_READ,
    CONFIG_WRITE,
    MESSAGE
};

// Traffic Class definitions (0-7)
enum class TrafficClass : uint8_t {
    TC0 = 0, TC1 = 1, TC2 = 2, TC3 = 3,
    TC4 = 4, TC5 = 5, TC6 = 6, TC7 = 7
};

// Virtual Channel definitions (0-7)
enum class VirtualChannel : uint8_t {
    VC0 = 0, VC1 = 1, VC2 = 2, VC3 = 3,
    VC4 = 4, VC5 = 5, VC6 = 6, VC7 = 7
};

// Arbitration schemes as per PCIe spec
enum class ArbitrationScheme {
    STRICT_PRIORITY,
    ROUND_ROBIN,
    WEIGHTED_ROUND_ROBIN,
    TIME_BASED
};

// PCIe Transaction Packet
struct PCIePacket {
    uint32_t requester_id;
    uint32_t address;
    uint32_t data_length;
    TransactionType type;
    TrafficClass tc;
    VirtualChannel vc;
    uint32_t tag;
    std::chrono::steady_clock::time_point timestamp;
    
    PCIePacket(uint32_t req_id, uint32_t addr, uint32_t len, 
               TransactionType t, TrafficClass traffic_class, 
               VirtualChannel virtual_channel, uint32_t tg)
        : requester_id(req_id), address(addr), data_length(len),
          type(t), tc(traffic_class), vc(virtual_channel), tag(tg),
          timestamp(std::chrono::steady_clock::now()) {}
};

// VC Resource Configuration
struct VCResource {
    VirtualChannel vc_id;
    bool enabled;
    uint8_t priority;           // 0-7, higher number = higher priority
    uint16_t weight;           // For weighted round-robin
    uint32_t max_credits;      // Flow control credits
    uint32_t current_credits;
    ArbitrationScheme scheme;
    
    VCResource(VirtualChannel id, uint8_t prio = 0, uint16_t w = 1)
        : vc_id(id), enabled(true), priority(prio), weight(w),
          max_credits(256), current_credits(256), 
          scheme(ArbitrationScheme::STRICT_PRIORITY) {}
};

// VC Arbitration Engine
class VCArbitrationEngine {
private:
    std::map<VirtualChannel, std::unique_ptr<VCResource>> vc_resources;
    std::map<VirtualChannel, std::queue<std::shared_ptr<PCIePacket>>> vc_queues;
    
    // Round-robin state
    VirtualChannel last_served_vc = VirtualChannel::VC0;
    std::map<VirtualChannel, uint32_t> rr_counters;
    
    // Statistics
    std::map<VirtualChannel, uint64_t> packets_served;
    std::map<VirtualChannel, uint64_t> bytes_served;
    
public:
    VCArbitrationEngine() {
        initializeVCs();
    }
    
    // Initialize default VC configuration
    void initializeVCs() {
        // VC0 is mandatory and always enabled
        vc_resources[VirtualChannel::VC0] = 
            std::make_unique<VCResource>(VirtualChannel::VC0, 7, 4);
        
        // Initialize other VCs with lower priorities
        for (int i = 1; i < 8; i++) {
            VirtualChannel vc = static_cast<VirtualChannel>(i);
            vc_resources[vc] = 
                std::make_unique<VCResource>(vc, 7-i, 1);
            vc_resources[vc]->enabled = false; // Disabled by default
        }
        
        // Initialize queues and counters
        for (auto& [vc, resource] : vc_resources) {
            vc_queues[vc] = std::queue<std::shared_ptr<PCIePacket>>();
            rr_counters[vc] = 0;
            packets_served[vc] = 0;
            bytes_served[vc] = 0;
        }
    }
    
    // Configure VC resource
    bool configureVC(VirtualChannel vc, bool enable, uint8_t priority, 
                     uint16_t weight, ArbitrationScheme scheme) {
        if (vc_resources.find(vc) == vc_resources.end()) {
            return false;
        }
        
        auto& resource = vc_resources[vc];
        resource->enabled = enable;
        resource->priority = priority;
        resource->weight = weight;
        resource->scheme = scheme;
        
        return true;
    }
    
    // Map Traffic Class to Virtual Channel
    VirtualChannel mapTCtoVC(TrafficClass tc) {
        // Default 1:1 mapping, can be customized
        switch (tc) {
            case TrafficClass::TC0: return VirtualChannel::VC0;
            case TrafficClass::TC1: return VirtualChannel::VC1;
            case TrafficClass::TC2: return VirtualChannel::VC2;
            case TrafficClass::TC3: return VirtualChannel::VC3;
            case TrafficClass::TC4: return VirtualChannel::VC4;
            case TrafficClass::TC5: return VirtualChannel::VC5;
            case TrafficClass::TC6: return VirtualChannel::VC6;
            case TrafficClass::TC7: return VirtualChannel::VC7;
            default: return VirtualChannel::VC0;
        }
    }
    
    // Enqueue packet for transmission
    bool enqueuePacket(std::shared_ptr<PCIePacket> packet) {
        VirtualChannel target_vc = mapTCtoVC(packet->tc);
        
        // Check if VC is enabled
        if (!vc_resources[target_vc]->enabled) {
            // Route to VC0 if target VC is disabled
            target_vc = VirtualChannel::VC0;
        }
        
        // Check flow control credits
        auto& resource = vc_resources[target_vc];
        if (resource->current_credits == 0) {
            return false; // No credits available
        }
        
        // Update packet's VC assignment
        packet->vc = target_vc;
        // Enqueue packet
        
        vc_queues[target_vc].push(packet);
        
        // Consume credit
        resource->current_credits--;
        
        return true;
    }
    
    // Strict Priority Arbitration
    std::shared_ptr<PCIePacket> arbitrateStrictPriority() {
        VirtualChannel selected_vc = VirtualChannel::VC0;
        uint8_t highest_priority = 0;
        bool found = false;
        
        for (auto& [vc, resource] : vc_resources) {
            if (!resource->enabled || vc_queues[vc].empty()) {
                continue;
            }
            
            if (resource->priority > highest_priority || !found) {
                highest_priority = resource->priority;
                selected_vc = vc;
                found = true;
            }
        }
        
        if (!found) {
            return nullptr;
        }
        
        return dequeueFromVC(selected_vc);
    }
    
    // Round Robin Arbitration
    std::shared_ptr<PCIePacket> arbitrateRoundRobin() {
        VirtualChannel start_vc = last_served_vc;
        VirtualChannel current_vc = start_vc;
        
        do {
            // Move to next VC
            current_vc = static_cast<VirtualChannel>(
                (static_cast<int>(current_vc) + 1) % 8);
            
            if (vc_resources[current_vc]->enabled && 
                !vc_queues[current_vc].empty()) {
                last_served_vc = current_vc;
                return dequeueFromVC(current_vc);
            }
        } while (current_vc != start_vc);
        
        return nullptr;
    }
    
    // Weighted Round Robin Arbitration
    std::shared_ptr<PCIePacket> arbitrateWeightedRoundRobin() {
        for (auto& [vc, resource] : vc_resources) {
            if (!resource->enabled || vc_queues[vc].empty()) {
                continue;
            }
            
            if (rr_counters[vc] < resource->weight) {
                rr_counters[vc]++;
                return dequeueFromVC(vc);
            }
        }
        
        // Reset all counters and try again
        for (auto& [vc, counter] : rr_counters) {
            counter = 0;
        }
        
        return arbitrateWeightedRoundRobin();
    }
    
    // Main arbitration function
    std::shared_ptr<PCIePacket> arbitrate() {
        // Use the arbitration scheme of the highest priority enabled VC
        ArbitrationScheme scheme = ArbitrationScheme::STRICT_PRIORITY;
        
        for (auto& [vc, resource] : vc_resources) {
            if (resource->enabled && !vc_queues[vc].empty()) {
                scheme = resource->scheme;
                break;
            }
        }
        
        switch (scheme) {
            case ArbitrationScheme::STRICT_PRIORITY:
                return arbitrateStrictPriority();
            case ArbitrationScheme::ROUND_ROBIN:
                return arbitrateRoundRobin();
            case ArbitrationScheme::WEIGHTED_ROUND_ROBIN:
                return arbitrateWeightedRoundRobin();
            case ArbitrationScheme::TIME_BASED:
                // Implement time-based arbitration if needed
                return arbitrateStrictPriority();
            default:
                return arbitrateStrictPriority();
        }
    }
    
    // Dequeue packet from specific VC
    std::shared_ptr<PCIePacket> dequeueFromVC(VirtualChannel vc) {
        if (vc_queues[vc].empty()) {
            return nullptr;
        }
        
        auto packet = vc_queues[vc].front();
        vc_queues[vc].pop();
        
        // Update statistics
        packets_served[vc]++;
        bytes_served[vc] += packet->data_length;
        
        return packet;
    }
    
    // Restore flow control credits
    void restoreCredits(VirtualChannel vc, uint32_t credits) {
        auto& resource = vc_resources[vc];
        resource->current_credits = std::min(
            resource->current_credits + credits, 
            resource->max_credits);
    }
    
    // Get VC queue status
    size_t getQueueDepth(VirtualChannel vc) const {
        auto it = vc_queues.find(vc);
        return (it != vc_queues.end()) ? it->second.size() : 0;
    }
    
    // Print statistics
    void printStatistics() const {
        std::cout << "\n=== VC Arbitration Statistics ===\n";
        for (auto& [vc, count] : packets_served) {
            std::cout << "VC" << static_cast<int>(vc) 
                      << ": Packets=" << count 
                      << ", Bytes=" << bytes_served.at(vc)
                      << ", Queue Depth=" << getQueueDepth(vc)
                      << ", Credits=" << vc_resources.at(vc)->current_credits
                      << std::endl;
        }
    }
};

// Example usage and test
int pcie_vc_main() {
    VCArbitrationEngine arbiter;
    
    // Configure VCs
    arbiter.configureVC(VirtualChannel::VC0, true, 7, 4, ArbitrationScheme::STRICT_PRIORITY);
    arbiter.configureVC(VirtualChannel::VC1, true, 5, 2, ArbitrationScheme::WEIGHTED_ROUND_ROBIN);
    arbiter.configureVC(VirtualChannel::VC2, true, 3, 1, ArbitrationScheme::ROUND_ROBIN);
    
    // Create test packets
    std::vector<std::shared_ptr<PCIePacket>> test_packets;
    
    // High priority traffic (TC7 -> VC7, but routed to VC0 if VC7 disabled)
    test_packets.push_back(std::make_shared<PCIePacket>(
        0x1000, 0x10000000, 64, TransactionType::MEMORY_READ, 
        TrafficClass::TC7, VirtualChannel::VC0, 1));
    
    // Medium priority traffic
    test_packets.push_back(std::make_shared<PCIePacket>(
        0x2000, 0x20000000, 128, TransactionType::MEMORY_WRITE, 
        TrafficClass::TC1, VirtualChannel::VC1, 2));
    
    // Low priority traffic
    test_packets.push_back(std::make_shared<PCIePacket>(
        0x3000, 0x30000000, 256, TransactionType::IO_READ, 
        TrafficClass::TC2, VirtualChannel::VC2, 3));
    
    // Enqueue packets
    std::cout << "Enqueueing test packets...\n";
    for (auto& packet : test_packets) {
        if (arbiter.enqueuePacket(packet)) {
            std::cout << "Packet enqueued to VC" 
                      << static_cast<int>(packet->vc) << std::endl;
        } else {
            std::cout << "Failed to enqueue packet" << std::endl;
        }
    }
    
    // Arbitrate and process packets
    std::cout << "\nArbitrating packets...\n";
    while (true) {
        auto packet = arbiter.arbitrate();
        if (!packet) {
            break;
        }
        
        std::cout << "Arbitrated packet from VC" 
                  << static_cast<int>(packet->vc)
                  << ", TC" << static_cast<int>(packet->tc)
                  << ", Length=" << packet->data_length << std::endl;
        
        // Simulate credit restoration
        arbiter.restoreCredits(packet->vc, 1);
    }
    
    arbiter.printStatistics();
    
    return 0;
}