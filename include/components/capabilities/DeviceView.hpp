#pragma once
#include <string>
#include <optional>
#include <cstddef>
struct DeviceSurfaceShape {size_t width,height,stride,storage_word_bytes;};
// Optional observation metadata. This describes a device; it never advances it.
class DeviceView {
public:
    virtual ~DeviceView() = default;
    virtual std::string deviceViewKind() const = 0;
    virtual std::optional<DeviceSurfaceShape> deviceSurfaceShape() const {return std::nullopt;}
};
