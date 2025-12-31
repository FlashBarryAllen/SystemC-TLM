## ISLIP (Input-Output) Module Documentation

This module implements the ISLIP (Input-Output) scheduling algorithm for managing input/output operations in a SystemC TLM environment. It provides mechanisms for efficient resource allocation and data flow control between different components.

### Overview
The ISLIP module is designed to facilitate high-performance communication in SystemC TLM models. It includes several sub-modules that handle various aspects of the scheduling process, ensuring optimal utilization of resources.
1. **fifo**: Implements a first-in-first-out queue for managing data packets.
2. **pim**: Implements a parallel input manager for handling incoming data based on based on virtual output queuing (voq).
3. **rrm**: Implements a round-robin manager for coordinating input/output operations.
4. **islip_basic**: Implements basic iSLIP scheduling algorithm based on the rrm.
5. **islip_sp**: Implements prioritied iSLIP scheduling algorithm based basic iSLIP.
6. **islip_threshold**: Implements threshold iSLIP scheduling algorithm based on basic iSLIP.
7. **islip_wrr**: Implements weighted round-robin iSLIP scheduling algorithm based on basic iSLIP.

### ISLIP Architecture
The ISLIP module architecture consists of multiple interconnected components that work together to implement the scheduling algorithm.Suppose N x N input-output ports are present in the system. So every input port has N voqs for output. The main process include:
1. **request**: If a voq in an input port has cell, then request is generated to the corresponding output port. The max requests are N x N.
2. **grant**: If an output port has recieved a request signal, then grant is generated to the corresponding input port.
3. **accept**: If an input port has received a grant signal, then accept is generated.

### Components
The ISLIP module consists of the following key components:
1. **input**: Represents the network nodes involved in data exchange.
2. **output**: Implements the bus architecture for data routing.

### Features
- Efficient scheduling algorithm for input/output operations.
- Modular design for easy integration with other SystemC TLM components.
- Support for high-speed data transfer and low latency communication.
### Usage
To use the ISLIP module, include it in your SystemC TLM project and instantiate the required components as needed. Configure the parameters according to your system requirements and integrate it with your existing architecture.
### Example
