#include "test.h"

namespace ycl {
    void test_main() {
        std::cout << "ycl::test_main" << std::endl;
    }
}

namespace test {
    void test_main() {
        std::cout << "test::test_main" << std::endl;
    }
}

void test_main() {
    std::cout << "test_main" << std::endl;
}