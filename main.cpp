#include "test.h"
#include "gtest/gtest.h"

using ::testing::EmptyTestEventListener;
using ::testing::InitGoogleTest;
using ::testing::Test;
using ::testing::TestEventListeners;
using ::testing::TestInfo;
using ::testing::TestPartResult;
using ::testing::UnitTest;

int main(int argc, char* argv[])
{
    InitGoogleTest(&argc, argv);

    ::test_main();

    ycl::test_main();

    test::test_main();


    return 0;
}