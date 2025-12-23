#include "test.h"
#include "peq_test.h"
#include "pcie_scan.h"
#include "pcie_vc.h"
#include "pcie_arb.h"
#include "top.h"
#include "gen.h"
#include "gtest/gtest.h"
#include <tlm_utils/simple_target_socket.h>

using ::testing::EmptyTestEventListener;
using ::testing::InitGoogleTest;
using ::testing::Test;
using ::testing::TestEventListeners;
using ::testing::TestInfo;
using ::testing::TestPartResult;
using ::testing::UnitTest;


int sc_main(int argc, char* argv[])
{
    InitGoogleTest(&argc, argv);

    peq_test_main();

    //pcie_scan_main();

    //pcie_vc_main();

    //pcie_arb_main();

    //Qos qos;
    //qos.in(0, 1);
    //qos.in(1, 2);
    //qos.in(2, 3);
    //qos.in(3, 4);
    //qos.in(4, 5);
    //qos.in(5, 6);
    //qos.in(6, 7);
    //qos.in(7, 8);
    //qos.in(0, 9);
    //qos.in(1, 10);
    //qos.in(2, 11);
    //qos.in(3, 12);
    //qos.in(4, 13);
    //qos.in(5, 14);
    //qos.in(6, 15);
    //qos.in(7, 16);

    //qos.sp_sch();
    //qos.rr_sch();

    // tlm test
    //top srv_top("srv_top");
    //Top_LT top_lt("top_lt");
    //Top_AT top_at("top_at");
    //DMA_Top  dma_top("dma_top");
    sc_start();
    //sc_start(20, sc_core::SC_NS);
    std::cout << "done" << std::endl;

    return 0;
}