#include "basic/Pin.hpp"
#include "basic/Wire.hpp"
#include "simulator/Event.hpp"

template class Pin<1>;
template class Pin<2>;
template class Pin<3>;
template class Pin<4>;
template class Pin<5>;
template class Pin<8>;
template class Pin<16>;
template class Pin<32>;

template class Wire<1>;
template class Wire<2>;
template class Wire<3>;
template class Wire<4>;
template class Wire<5>;
template class Wire<8>;
template class Wire<16>;
template class Wire<32>;

template class WireUpdateEvent<1>;
template class WireUpdateEvent<2>;
template class WireUpdateEvent<3>;
template class WireUpdateEvent<4>;
template class WireUpdateEvent<5>;
template class WireUpdateEvent<8>;
template class WireUpdateEvent<16>;
template class WireUpdateEvent<32>;
