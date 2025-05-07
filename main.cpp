#include "test.h"
#include "top.h"
#include "gtest/gtest.h"

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

    Qos qos;
    qos.in(0, 1);
    qos.in(1, 2);
    qos.in(2, 3);
    qos.in(3, 4);
    qos.in(4, 5);
    qos.in(5, 6);
    qos.in(6, 7);
    qos.in(7, 8);
    qos.in(0, 9);
    qos.in(1, 10);
    qos.in(2, 11);
    qos.in(3, 12);
    qos.in(4, 13);
    qos.in(5, 14);
    qos.in(6, 15);
    qos.in(7, 16);

    qos.sch();

    // tlm test
    top srv_top("srv_top");
    sc_start(20, sc_core::SC_NS);
    std::cout << "done" << std::endl;

    return 0;
}