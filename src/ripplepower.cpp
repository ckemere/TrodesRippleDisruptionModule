#include "ripplepower.h"

RipplePower::RipplePower(std::vector<int> ripple_channels):
    ripple_channels(ripple_channels), current_data(ripple_channels.size())
{
}

void RipplePower::new_data(std::vector<int16_t> data)
{
    for (int i=0; i < ripple_channels.size(); i++) {
        current_data[i] = data[ripple_channels[i]];
    }
}