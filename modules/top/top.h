#include <node.h>

class top : public sc_core::sc_module {
    public:
     SC_HAS_PROCESS(top);
     top(sc_core::sc_module_name name) {
         m_clk = new sc_core::sc_clock("clk", 1, sc_core::SC_NS);
         rx_node = std::make_shared<node>("rx");
         tx_node = std::make_shared<node>("tx");
         rx_node->m_clk(*m_clk);
         tx_node->m_clk(*m_clk);
         rx_node->rx.bind(tx_node->tx);
         tx_node->rx.bind(rx_node->tx);
     }
 
    public:
     sc_core::sc_clock* m_clk;
     std::shared_ptr<node> rx_node;
     std::shared_ptr<node> tx_node;
 };