#pragma once

#include "tests/ComponentTestModel.hpp"
#include "tests/TestHelpers.hpp"
#include <string>

class RewireTest : public circuit::test::ComponentScenarioTest {
public:
    RewireTest();
};

class RewireValidationTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

#define DECLARE_UTILITY_COMPONENT_TEST(NAME) \
    class NAME : public circuit::test::ComponentScenarioTest { \
    public: \
        NAME(); \
    };

DECLARE_UTILITY_COMPONENT_TEST(BitSplitter1Test)
DECLARE_UTILITY_COMPONENT_TEST(BitSplitter2Test)
DECLARE_UTILITY_COMPONENT_TEST(BitSplitter3Test)
DECLARE_UTILITY_COMPONENT_TEST(BitSplitter4Test)
DECLARE_UTILITY_COMPONENT_TEST(BitSplitter5Test)
DECLARE_UTILITY_COMPONENT_TEST(BitSplitter8Test)
DECLARE_UTILITY_COMPONENT_TEST(BitSplitter16Test)
DECLARE_UTILITY_COMPONENT_TEST(BitSplitter32Test)

DECLARE_UTILITY_COMPONENT_TEST(BitJoiner1Test)
DECLARE_UTILITY_COMPONENT_TEST(BitJoiner2Test)
DECLARE_UTILITY_COMPONENT_TEST(BitJoiner3Test)
DECLARE_UTILITY_COMPONENT_TEST(BitJoiner4Test)
DECLARE_UTILITY_COMPONENT_TEST(BitJoiner5Test)
DECLARE_UTILITY_COMPONENT_TEST(BitJoiner8Test)
DECLARE_UTILITY_COMPONENT_TEST(BitJoiner16Test)
DECLARE_UTILITY_COMPONENT_TEST(BitJoiner32Test)

DECLARE_UTILITY_COMPONENT_TEST(ConstantValue1Test)
DECLARE_UTILITY_COMPONENT_TEST(ConstantValue2Test)
DECLARE_UTILITY_COMPONENT_TEST(ConstantValue4Test)
DECLARE_UTILITY_COMPONENT_TEST(ConstantValue8Test)
DECLARE_UTILITY_COMPONENT_TEST(ConstantValue32Test)

#undef DECLARE_UTILITY_COMPONENT_TEST
