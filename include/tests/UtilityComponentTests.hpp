#pragma once

#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/utility/Rewire.hpp"
#include "simulator/SimulationTest.hpp"
#include "tests/ComponentRowsTest.hpp"
#include "tests/TestHelpers.hpp"
#include <string>
#include <vector>

class RewireUnpackTest : public ComponentRowsTest<Rewire,
                                                   std::vector<Rewire::WireSpec>,
                                                   std::vector<Rewire::WireSpec>,
                                                   std::vector<Rewire::BitMap>> {
public:
    RewireUnpackTest();
};

class RewirePackTest : public ComponentRowsTest<Rewire,
                                                 std::vector<Rewire::WireSpec>,
                                                 std::vector<Rewire::WireSpec>,
                                                 std::vector<Rewire::BitMap>> {
public:
    RewirePackTest();
};

class RewireSliceTest : public ComponentRowsTest<Rewire,
                                                  std::vector<Rewire::WireSpec>,
                                                  std::vector<Rewire::WireSpec>,
                                                  std::vector<Rewire::BitMap>> {
public:
    RewireSliceTest();
};

class RewireZeroExtendTest : public ComponentRowsTest<Rewire,
                                                       std::vector<Rewire::WireSpec>,
                                                       std::vector<Rewire::WireSpec>,
                                                       std::vector<Rewire::BitMap>,
                                                       Rewire::UnmappedBitValue> {
public:
    RewireZeroExtendTest();
};

class RewireSignExtendTest : public ComponentRowsTest<Rewire,
                                                       std::vector<Rewire::WireSpec>,
                                                       std::vector<Rewire::WireSpec>,
                                                       std::vector<Rewire::BitMap>> {
public:
    RewireSignExtendTest();
};

class RewireUnmappedHighTest : public ComponentRowsTest<Rewire,
                                                         std::vector<Rewire::WireSpec>,
                                                         std::vector<Rewire::WireSpec>,
                                                         std::vector<Rewire::BitMap>,
                                                         Rewire::UnmappedBitValue> {
public:
    RewireUnmappedHighTest();
};

class RewireWidth5Test : public ComponentRowsTest<Rewire,
                                                   std::vector<Rewire::WireSpec>,
                                                   std::vector<Rewire::WireSpec>,
                                                   std::vector<Rewire::BitMap>> {
public:
    RewireWidth5Test();
};

class RewireUnmappedUnknownTest : public SimulationTest {
public:
    void setupCircuit() override;
    std::string getTestName() const override;
    size_t getRunDuration() const override;

protected:
    void buildCircuit() override {}
    void setInitialState() override;
    void verifyResults() override;
};

class RewireValidationTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class BitSplitter8Test : public ComponentRowsTest<BitSplitter<8>> {
public:
    BitSplitter8Test();
};

class BitJoiner8Test : public ComponentRowsTest<BitJoiner<8>> {
public:
    BitJoiner8Test();
};

class BitSplitter16Test : public ComponentRowsTest<BitSplitter<16>> {
public:
    BitSplitter16Test();
};

class BitJoiner32Test : public ComponentRowsTest<BitJoiner<32>> {
public:
    BitJoiner32Test();
};

class BitJoiner8UnknownTest : public SimulationTest {
public:
    void setupCircuit() override;
    std::string getTestName() const override;
    size_t getRunDuration() const override;

protected:
    void buildCircuit() override {}
    void setInitialState() override;
    void verifyResults() override;
};

class ConstantValue1HighTest : public ComponentRowsTest<ConstantValue<1, 1>, uint64_t> {
public:
    ConstantValue1HighTest();
};

class ConstantValue1Low8TriggerTest : public ComponentRowsTest<ConstantValue<1, 8>, uint64_t> {
public:
    ConstantValue1Low8TriggerTest();
};

class ConstantValue8Test : public ComponentRowsTest<ConstantValue<8, 8>, uint64_t> {
public:
    ConstantValue8Test();
};
