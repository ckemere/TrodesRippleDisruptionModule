#ifndef RIPPLEFILTER_H
#define RIPPLEFILTER_H

#include <vector>
#include <stdint.h>

class RipplePower
{
public:
    RipplePower(unsigned int);
    // ~RipplePower();
    void reset(std::vector<unsigned int>);

public:
    void new_data(std::vector<int16_t> data);
    std::vector<double> output;

private:
    unsigned int input_length;
    std::vector<unsigned int> filter_chans;
};

#endif // RIPPLEFILTER_H