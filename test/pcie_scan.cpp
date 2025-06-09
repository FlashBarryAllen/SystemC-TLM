#include <iostream>
#include <vector>
#include <memory>
#include <map>
#include <iomanip>
#include <string>
#include <sstream>
#include "pcie_scan.h"

// PCI Configuration Space Register Offsets
#define PCI_VENDOR_ID           0x00
#define PCI_DEVICE_ID           0x02
#define PCI_COMMAND             0x04
#define PCI_STATUS              0x06
#define PCI_REVISION_ID         0x08
#define PCI_CLASS_CODE          0x09
#define PCI_CACHE_LINE_SIZE     0x0C
#define PCI_LATENCY_TIMER       0x0D
#define PCI_HEADER_TYPE         0x0E
#define PCI_BIST                0x0F

// PCI-to-PCI Bridge Specific Registers
#define PCI_PRIMARY_BUS         0x18
#define PCI_SECONDARY_BUS       0x19
#define PCI_SUBORDINATE_BUS     0x1A

// Header Type Values
#define PCI_HEADER_TYPE_NORMAL      0x00
#define PCI_HEADER_TYPE_BRIDGE      0x01
#define PCI_HEADER_TYPE_CARDBUS     0x02
#define PCI_HEADER_TYPE_MULTIFUNCTION 0x80

// Invalid Vendor ID
#define PCI_INVALID_VENDOR_ID   0xFFFF

// Maximum values
#define PCI_MAX_BUS             255
#define PCI_MAX_DEVICE          31
#define PCI_MAX_FUNCTION        7

class PCIDevice {
public:
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t header_type;
    uint8_t class_code;
    bool is_bridge;
    bool is_multifunction;
    
    // Bridge-specific fields
    uint8_t primary_bus;
    uint8_t secondary_bus;
    uint8_t subordinate_bus;
    
    PCIDevice(uint8_t b, uint8_t d, uint8_t f) 
        : bus(b), device(d), function(f), vendor_id(0), device_id(0),
          header_type(0), class_code(0), is_bridge(false), is_multifunction(false),
          primary_bus(0), secondary_bus(0), subordinate_bus(0) {}
    
    std::string toString() const {
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::uppercase;
        ss << "Bus " << std::setw(2) << (int)bus 
           << ", Device " << std::setw(2) << (int)device
           << ", Function " << (int)function
           << " - Vendor: " << std::setw(4) << vendor_id
           << ", Device: " << std::setw(4) << device_id
           << " (" << (is_bridge ? "Bridge" : "Endpoint") << ")";
        if (is_bridge) {
            ss << " [P:" << (int)primary_bus 
               << " S:" << (int)secondary_bus 
               << " Sub:" << (int)subordinate_bus << "]";
        }
        return ss.str();
    }
};

class PCIEnumerator {
private:
    std::vector<std::shared_ptr<PCIDevice>> discovered_devices;
    uint8_t last_bus_number;
    
    // Simulated configuration space - in real implementation, this would be hardware access
    std::map<uint32_t, uint32_t> config_space;
    
    // Helper function to create configuration space address
    uint32_t makeConfigAddress(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
        return ((uint32_t)bus << 16) | ((uint32_t)device << 11) | 
               ((uint32_t)function << 8) | (offset & 0xFC);
    }
    
    // Simulated PCI configuration read
    uint32_t readConfigDword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
        uint32_t addr = makeConfigAddress(bus, device, function, offset);
        auto it = config_space.find(addr);
        if (it != config_space.end()) {
            return it->second;
        }
        return 0xFFFFFFFF; // Device not present
    }
    
    // Simulated PCI configuration write
    void writeConfigDword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
        uint32_t addr = makeConfigAddress(bus, device, function, offset);
        config_space[addr] = value;
        
        std::cout << "    CONFIG WRITE: Bus " << (int)bus 
                  << ", Dev " << (int)device 
                  << ", Func " << (int)function
                  << ", Offset 0x" << std::hex << (int)offset 
                  << " = 0x" << std::hex << value << std::dec << std::endl;
    }
    
    // Write to specific byte in configuration space
    void writeConfigByte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value) {
        uint8_t aligned_offset = offset & 0xFC;
        uint32_t current = readConfigDword(bus, device, function, aligned_offset);
        uint8_t byte_offset = offset & 0x03;
        
        uint32_t mask = 0xFF << (byte_offset * 8);
        current &= ~mask;
        current |= ((uint32_t)value << (byte_offset * 8));
        
        writeConfigDword(bus, device, function, aligned_offset, current);
    }
    
    uint16_t readConfigWord(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
        uint32_t dword = readConfigDword(bus, device, function, offset & 0xFC);
        uint8_t word_offset = (offset & 0x02) ? 2 : 0;
        return (dword >> (word_offset * 8)) & 0xFFFF;
    }
    
    uint8_t readConfigByte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
        uint32_t dword = readConfigDword(bus, device, function, offset & 0xFC);
        uint8_t byte_offset = offset & 0x03;
        uint8_t result = (dword >> (byte_offset * 8)) & 0xFF;
        
        // Debug output for header type reads
        if (offset == PCI_HEADER_TYPE) {
            std::cout << "    Reading Header Type: Bus " << (int)bus 
                      << ", Dev " << (int)device 
                      << ", Func " << (int)function
                      << " -> 0x" << std::hex << (int)result << std::dec << std::endl;
        }
        
        return result;
    }
    
    bool deviceExists(uint8_t bus, uint8_t device, uint8_t function) {
        uint16_t vendor_id = readConfigWord(bus, device, function, PCI_VENDOR_ID);
        return vendor_id != PCI_INVALID_VENDOR_ID && vendor_id != 0x0000;
    }
    
    std::shared_ptr<PCIDevice> probeDevice(uint8_t bus, uint8_t device, uint8_t function) {
        if (!deviceExists(bus, device, function)) {
            return nullptr;
        }
        
        auto pci_device = std::make_shared<PCIDevice>(bus, device, function);
        
        // Read basic device information
        pci_device->vendor_id = readConfigWord(bus, device, function, PCI_VENDOR_ID);
        pci_device->device_id = readConfigWord(bus, device, function, PCI_DEVICE_ID);
        pci_device->header_type = readConfigByte(bus, device, function, PCI_HEADER_TYPE);
        pci_device->class_code = readConfigByte(bus, device, function, PCI_CLASS_CODE);
        
        // Determine if this is a multifunction device
        pci_device->is_multifunction = (pci_device->header_type & PCI_HEADER_TYPE_MULTIFUNCTION) != 0;
        
        // Clean header type (remove multifunction bit)
        uint8_t clean_header_type = pci_device->header_type & 0x7F;
        pci_device->is_bridge = (clean_header_type == PCI_HEADER_TYPE_BRIDGE);
        
        if (pci_device->is_bridge) {
            // Read current bridge configuration
            pci_device->primary_bus = readConfigByte(bus, device, function, PCI_PRIMARY_BUS);
            pci_device->secondary_bus = readConfigByte(bus, device, function, PCI_SECONDARY_BUS);
            pci_device->subordinate_bus = readConfigByte(bus, device, function, PCI_SUBORDINATE_BUS);
        }
        
        std::cout << "  DISCOVERED: " << pci_device->toString() << std::endl;
        return pci_device;
    }
    
    void configureBridge(std::shared_ptr<PCIDevice> bridge, uint8_t primary, uint8_t secondary, uint8_t subordinate) {
        std::cout << "  CONFIGURING BRIDGE: Bus " << (int)bridge->bus 
                  << ", Dev " << (int)bridge->device 
                  << " -> Primary=" << (int)primary 
                  << ", Secondary=" << (int)secondary 
                  << ", Subordinate=" << (int)subordinate << std::endl;
        
        bridge->primary_bus = primary;
        bridge->secondary_bus = secondary;
        bridge->subordinate_bus = subordinate;
        
        writeConfigByte(bridge->bus, bridge->device, bridge->function, PCI_PRIMARY_BUS, primary);
        writeConfigByte(bridge->bus, bridge->device, bridge->function, PCI_SECONDARY_BUS, secondary);
        writeConfigByte(bridge->bus, bridge->device, bridge->function, PCI_SUBORDINATE_BUS, subordinate);
    }
    
    void updateBridgeSubordinate(std::shared_ptr<PCIDevice> bridge, uint8_t subordinate) {
        std::cout << "  UPDATING BRIDGE SUBORDINATE: Bus " << (int)bridge->bus 
                  << ", Dev " << (int)bridge->device 
                  << " -> Subordinate=" << (int)subordinate << std::endl;
        
        bridge->subordinate_bus = subordinate;
        writeConfigByte(bridge->bus, bridge->device, bridge->function, PCI_SUBORDINATE_BUS, subordinate);
    }
    
    uint8_t enumerateBus(uint8_t bus_num) {
        std::cout << "\nSCANNING BUS " << (int)bus_num << std::endl;
        uint8_t max_subordinate_bus = bus_num;
        
        // Scan all possible devices on this bus
        for (uint8_t device = 0; device <= PCI_MAX_DEVICE; device++) {
            // First check function 0
            uint8_t function = 0;
            auto pci_device = probeDevice(bus_num, device, function);
            if (!pci_device) {
                continue; // Device doesn't exist
            }
            
            discovered_devices.push_back(pci_device);
            
            // If this is a bridge, perform depth-first enumeration
            if (pci_device->is_bridge) {
                uint8_t secondary_bus = ++last_bus_number;
                configureBridge(pci_device, bus_num, secondary_bus, PCI_MAX_BUS);
                
                // Recursively enumerate the secondary bus
                uint8_t subordinate_bus = enumerateBus(secondary_bus);
                
                // Update the bridge's subordinate bus number
                updateBridgeSubordinate(pci_device, subordinate_bus);
                max_subordinate_bus = std::max(max_subordinate_bus, subordinate_bus);
            } else if (pci_device->is_multifunction) {
                function++;
                while (function <= PCI_MAX_FUNCTION) {
                    auto func_device = probeDevice(bus_num, device, function);
                    if (func_device) {
                        discovered_devices.push_back(func_device);
                    }
                    function++;
                }
            }
        }
        
        return max_subordinate_bus;
    }
    
public:
    PCIEnumerator() : last_bus_number(0) {
        // Initialize simulated devices to match the example topology, actually this is a cfg write
        setupExampleTopology();
    }
    
    void setupExampleTopology() {
        // Helper function to setup a device
        auto setupDevice = [this](uint8_t bus, uint8_t dev, uint8_t func, 
                                 uint16_t vendor, uint16_t device, uint8_t header_type) {
            // Vendor ID and Device ID (offset 0x00)
            uint32_t addr = makeConfigAddress(bus, dev, func, 0x00);
            config_space[addr] = ((uint32_t)device << 16) | vendor;
            
            // Header Type (offset 0x0C contains cache line size, latency timer, header type, BIST)
            addr = makeConfigAddress(bus, dev, func, 0x0C);
            config_space[addr] = ((uint32_t)header_type << 16);
        };
        
        // Bridge A (Bus 0, Device 0, Function 0) - PCI-to-PCI Bridge
        setupDevice(0, 0, 0, 0x0001, 0x1234, 0x01);
        
        // Bridge B (Bus 0, Device 1, Function 0) - PCI-to-PCI Bridge  
        setupDevice(0, 1, 0, 0x0002, 0x5678, 0x01);
        
        // Bridge C (Bus 1, Device 0, Function 0) - PCI-to-PCI Bridge
        setupDevice(1, 0, 0, 0x0003, 0x9ABC, 0x01);
        
        // Bridge D (Bus 2, Device 0, Function 0) - PCI-to-PCI Bridge
        setupDevice(2, 0, 0, 0x0004, 0xDEF0, 0x01);
        
        // Bridge E (Bus 2, Device 1, Function 0) - PCI-to-PCI Bridge
        setupDevice(2, 1, 0, 0x0005, 0x1111, 0x01);
        
        // Endpoint devices on Bus 3 (Dev 0, Func 0 and 1) - Multifunction
        setupDevice(3, 0, 0, 0x0006, 0x2222, 0x80); // Multifunction endpoint
        setupDevice(3, 0, 1, 0x0007, 0x3333, 0x00); // Single function endpoint
        
        // Endpoint device on Bus 4 (Dev 0, Func 0)
        setupDevice(4, 0, 0, 0x0008, 0x4444, 0x00);
        
        // Bridge F (Bus 5, Device 0, Function 0)
        setupDevice(5, 0, 0, 0x0009, 0x5555, 0x01);
        
        // Bridge G (Bus 6, Device 0, Function 0)
        setupDevice(6, 0, 0, 0x000A, 0x6666, 0x01);
        
        // Bridge H (Bus 6, Device 1, Function 0)
        setupDevice(6, 1, 0, 0x000B, 0x7777, 0x01);
        
        // Bridge I (Bus 6, Device 2, Function 0)
        setupDevice(6, 2, 0, 0x000C, 0x8888, 0x01);
        
        // Endpoint device on Bus 7 (Dev 0, Func 0)
        setupDevice(7, 0, 0, 0x000D, 0x9999, 0x00);
        
        // Bridge J (Bus 8, Device 0, Function 0)
        setupDevice(8, 0, 0, 0x000E, 0xAAAA, 0x01);
        
        // PCI devices on Bus 9 (connected to Express PCI Bridge J)
        setupDevice(9, 0, 0, 0x000F, 0xBBBB, 0x00);
        setupDevice(9, 1, 0, 0x0010, 0xCCCC, 0x00);
        setupDevice(9, 2, 0, 0x0011, 0xDDDD, 0x00);
        
        // Endpoint device on Bus 10 (Dev 0, Func 0)
        setupDevice(10, 0, 0, 0x0012, 0xEEEE, 0x00);
    }
    
    void enumerateSystem() {
        std::cout << "=== PCI-E BUS ENUMERATION PROCESS ===" << std::endl;
        std::cout << "Starting depth-first enumeration..." << std::endl;
        
        discovered_devices.clear();
        last_bus_number = 0;
        
        // Start enumeration from bus 0
        uint8_t max_bus = enumerateBus(0);
        
        std::cout << "\n=== ENUMERATION COMPLETE ===" << std::endl;
        std::cout << "Maximum bus number discovered: " << (int)max_bus << std::endl;
        std::cout << "Total devices discovered: " << discovered_devices.size() << std::endl;
    }
    
    void printTopology() {
        std::cout << "\n=== DISCOVERED PCI TOPOLOGY ===" << std::endl;
        
        // Group devices by bus
        std::map<uint8_t, std::vector<std::shared_ptr<PCIDevice>>> buses;
        for (auto& device : discovered_devices) {
            buses[device->bus].push_back(device);
        }
        
        for (auto& [bus_num, devices] : buses) {
            std::cout << "\nBus " << (int)bus_num << ":" << std::endl;
            for (auto& device : devices) {
                std::cout << "  " << device->toString() << std::endl;
            }
        }
    }
    
    void printBridgeTree() {
        std::cout << "\n=== BRIDGE HIERARCHY ===" << std::endl;
        printBridgeTreeRecursive(0, 0, "");
    }
    
private:
    void printBridgeTreeRecursive(uint8_t bus_num, int depth, const std::string& prefix) {
        // Find bridges on this bus
        for (auto& device : discovered_devices) {
            if (device->bus == bus_num && device->is_bridge) {
                std::cout << prefix << "├─ Bridge " << device->toString() << std::endl;
                
                // Find devices on the secondary bus
                bool hasDevices = false;
                for (auto& child : discovered_devices) {
                    if (child->bus == device->secondary_bus && !child->is_bridge) {
                        if (!hasDevices) {
                            hasDevices = true;
                        }
                        std::cout << prefix << "│  └─ " << child->toString() << std::endl;
                    }
                }
                
                // Recursively print child bridges
                printBridgeTreeRecursive(device->secondary_bus, depth + 1, prefix + "│  ");
            }
        }
    }
};

int pcie_main() {
    PCIEnumerator enumerator;
    
    std::cout << "PCI-E Bus Enumeration Implementation" << std::endl;
    std::cout << "=====================================" << std::endl;
    
    // Perform the enumeration
    enumerator.enumerateSystem();
    
    // Print results
    enumerator.printTopology();
    enumerator.printBridgeTree();
    
    return 0;
}