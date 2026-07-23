#pragma once

#include "basic/LogicValue.hpp"
#include <cstddef>
#include <vector>

// Optional capability shared by structural and behavioral register-file
// implementations.  Consumers use this interface instead of down-casting to a
// particular implementation selected by a build profile.
class RegisterStateView {
public:
    virtual ~RegisterStateView() = default;

    /**
     * Return the architectural register image for target_time.
     *
     * The caller must first select target_time on the owning Simulator.
     * Structural implementations observe their child pins at that selected
     * circuit state; compact implementations may use target_time to reconstruct
     * otherwise-hidden state from their own history.
     */
    virtual std::vector<std::vector<LogicValue>> getRegisterStateAtTime(
        size_t target_time) const = 0;
};
