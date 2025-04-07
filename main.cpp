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

    ::test_main();
    ycl::test_main();
    test::test_main();

    // tlm test
    top srv_top("srv_top");
    sc_start(20, sc_core::SC_NS);
    std::cout << "done" << std::endl;

    return 0;
}