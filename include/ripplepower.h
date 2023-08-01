#ifndef RIPPLEFILTER_H
#define RIPPLEFILTER_H

#include <vector>
#include <stdint.h>

class RipplePower
{
public:
    RipplePower(std::vector<int> ripple_channels);
    // ~RipplePower();

public:
    void new_data(std::vector<int16_t> data);


private:
    std::vector<int> ripple_channels;
};

#endif // RIPPLEFILTER_H