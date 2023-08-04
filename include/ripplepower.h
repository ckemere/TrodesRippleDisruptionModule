#ifndef RIPPLEFILTER_H
#define RIPPLEFILTER_H

#include <vector>
#include <stdint.h>

class RipplePower
{
public:
    RipplePower(std::vector<unsigned int>);
    // ~RipplePower();
    void reset(std::vector<unsigned int>);

public:
    void new_data(std::vector<int16_t> data);

    std::vector<double> output;

private:
    std::vector<unsigned int> ripple_channels;
};

#endif // RIPPLEFILTER_H