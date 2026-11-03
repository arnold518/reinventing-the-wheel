#pragma once
// Explicit opt-in for a compact model composed from smaller proven models.
// The public family still has exactly one behavioral implementation. This
// capability does not authorize retaining ordinary structural gate children.
class BehavioralComposition {
public:
    virtual ~BehavioralComposition()=default;
    virtual void validateBehavioralComposition() const=0;
};
