#pragma once

#include "simulator/SimulationTime.hpp"

#include "components/BasicComponent.hpp"
#include <iostream>

class NOTGate : public BasicComponent
{
public:
    NOTGate(std::string name);
    static constexpr const char* TypeName = "NOTGate";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(SimulationTime current_time, Simulator& simulator) override;
protected:
    bool inertialDelay() const override { return true; }
};

class ANDGate : public BasicComponent
{
public:
    ANDGate(std::string name);
    static constexpr const char* TypeName = "ANDGate";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(SimulationTime current_time, Simulator& simulator) override;
protected:
    bool inertialDelay() const override { return true; }
};

class NANDGate : public BasicComponent
{
public:
    NANDGate(std::string name);
    static constexpr const char* TypeName = "NANDGate";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(SimulationTime current_time, Simulator& simulator) override;
protected:
    bool inertialDelay() const override { return true; }
};

class ORGate : public BasicComponent
{
public:
    ORGate(std::string name);
    static constexpr const char* TypeName = "ORGate";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(SimulationTime current_time, Simulator& simulator) override;
protected:
    bool inertialDelay() const override { return true; }
};

class NORGate : public BasicComponent
{
public:
    NORGate(std::string name);
    static constexpr const char* TypeName = "NORGate";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(SimulationTime current_time, Simulator& simulator) override;
protected:
    bool inertialDelay() const override { return true; }
};

class XORGate : public BasicComponent
{
public:
    XORGate(std::string name);
    static constexpr const char* TypeName = "XORGate";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(SimulationTime current_time, Simulator& simulator) override;
protected:
    bool inertialDelay() const override { return true; }
};
