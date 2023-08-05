#include "ripplepower.h"
#include <cstring>
#include <cmath>
#include <iostream>

#define ripple_filter_len 30
#define smoothing_filter_len 33

double ripple_filter_coefs[ripple_filter_len] = {
    0.006948895259200861,
    0.006926234072844102,
    0.006103502157617292,
    0.0019015869720564772,
    -0.008189119502501823,
    -0.024718456919193797,
    -0.04490236458710298,
    -0.06261768142301635,
    -0.07007106773169292,
    -0.060682222621851856,
    -0.03208366436487348,
    0.012060663184185196,
    0.0623490476891361,
    0.10616912299101257,
    0.131657880645337,
    0.13165788064533704,
    0.10616912299101257,
    0.06234904768913609,
    0.0120606631841852,
    -0.03208366436487348,
    -0.06068222262185186,
    -0.07007106773169292,
    -0.06261768142301641,
    -0.04490236458710301,
    -0.024718456919193794,
    -0.00818911950250182,
    0.0019015869720564766,
    0.0061035021576172944,
    0.006926234072844102,
    0.006948895259200861
};

double smoothing_filter_coefs[smoothing_filter_len] = {
    0.0203770957,
    0.0108532903,
    0.0134954582,
    0.0163441640,
    0.0193546202,
    0.0224738014,
    0.0256417906,
    0.0287934511,
    0.0318603667,
    0.0347729778,
    0.0374628330,
    0.0398648671,
    0.0419196133,
    0.0435752600,
    0.0447894668,
    0.0455308624,
    0.0457801628,
    0.0455308624,
    0.0447894668,
    0.0435752600,
    0.0419196133,
    0.0398648671,
    0.0374628330,
    0.0347729778,
    0.0318603667,
    0.0287934511,
    0.0256417906,
    0.0224738014,
    0.0193546202,
    0.0163441640,
    0.0134954582,
    0.0108532903,
    0.0203770957
};

double **filter_circ_buffer;
double **smoothing_circ_buffer;

unsigned int fidx, sidx;

RipplePower::RipplePower(unsigned int num_channels):
    output(num_channels)
{
    input_length = num_channels; // total number of channels being streamed

    // Allocate a filtering circular buffer for each LFP channel. 
    // For 16 tetrodes, this is something like 2 KB for each filter.
    // It's a bit annoying as it will likely get dropped from cache, but I'm not sure how to help this.
    filter_circ_buffer = new double* [num_channels];
    smoothing_circ_buffer = new double* [num_channels];
    
    for (unsigned int ch = 0; ch < num_channels; ch++) {
        filter_circ_buffer[ch] = new double[ripple_filter_len];
        smoothing_circ_buffer[ch] = new double[smoothing_filter_len];
        memset(filter_circ_buffer[ch], 0, ripple_filter_len*sizeof(double));
        memset(smoothing_circ_buffer[ch], 0, smoothing_filter_len*sizeof(double));
    }

    fidx = 0;
    sidx = 0;
}

void RipplePower::reset(std::vector<unsigned int> chans) {
    filter_chans = chans;
    std::cerr << "Ripple channels" << filter_chans.size() << ",";
    for (int i = 0; i < filter_chans.size(); i++)
        std::cerr << " " << filter_chans[i];
    std::cerr << "." << std::endl;
}

void RipplePower::new_data(std::vector<int16_t> data)
{
    // for (int i = 0; i < data.size(); i++)
    //     output[i] = data[i];
        
    for (auto ch : filter_chans) {
            // output[ch] = data[ch];

        int new_data_point = data[ch];

        // ripple filter
        double filter_output = 0;
        filter_circ_buffer[ch][fidx] = new_data_point;
        for (unsigned int i=0; i < ripple_filter_len; i++) {
            filter_output += ripple_filter_coefs[i] * filter_circ_buffer[ch][fidx++];
            if (fidx == ripple_filter_len)
                fidx = 0;
        }

        // envelope filter
        double smoothing_output = 0;
        smoothing_circ_buffer[ch][sidx] = fabs(filter_output);
        for (unsigned int i=0; i < smoothing_filter_len; i++) {
            smoothing_output += smoothing_filter_coefs[i] * smoothing_circ_buffer[ch][sidx++];
            if (sidx == smoothing_filter_len)
                sidx = 0;
        }

        output[ch] = smoothing_output;
    }

    // For each channel, we've cycled across the loop, ending where we began.
    // Now we prepare for the next sample
    fidx = fidx + 1;
    if (fidx == ripple_filter_len)
        fidx = 0;
    sidx = sidx + 1;
    if (sidx == smoothing_filter_len)
        sidx = 0;
}