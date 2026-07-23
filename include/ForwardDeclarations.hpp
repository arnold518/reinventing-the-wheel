#pragma once

#include <cstddef>

class Simulator;
class Component;
class IOComponent;
class BasicComponent;
class ComponentBuilder;
class PinBase;
class WireBase;
template<size_t WIDTH = 1> class Pin;
template<size_t WIDTH = 1> class Wire;

class Event;
class ComponentEvalEvent;
template<size_t WIDTH = 1> class WireUpdateEvent;

namespace circuit {
class BuildContext;
class BuildManifest;
class BuildProfile;
class ComponentCatalog;
}
