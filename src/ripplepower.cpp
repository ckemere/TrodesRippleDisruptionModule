#include "ripplepower.h"
#include <cstring>
#include <cmath>
#include <iostream>

#define ripple_filter_len 31
#define smoothing_filter_len 33

double ripple_filter_coefs[ripple_filter_len] = {
       -9.038549834012203e-03, -1.118299190093014e-02,
       -3.861084139931095e-03, -1.040834085586084e-17,
       -9.475105531395358e-03, -2.014587163241163e-02,
       -6.102267534698221e-03,  3.802492370446940e-02,
        7.585344498642127e-02,  5.612250674499941e-02,
       -2.960522547980343e-02, -1.202540911334640e-01,
       -1.303036430975862e-01, -3.271579861948655e-02,
        1.033989319600678e-01,  1.666666666666666e-01,
        1.033989319600678e-01, -3.271579861948655e-02,
       -1.303036430975862e-01, -1.202540911334640e-01,
       -2.960522547980343e-02,  5.612250674499941e-02,
        7.585344498642127e-02,  3.802492370446940e-02,
       -6.102267534698221e-03, -2.014587163241163e-02,
       -9.475105531395358e-03, -1.040834085586084e-17,
       -3.861084139931095e-03, -1.118299190093014e-02,
       -9.038549834012203e-03
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
    /* For testing - just copy input to output! */
    // for (int i = 0; i < data.size(); i++)
    //     output[i] = data[i];
        
    for (auto ch : filter_chans) {
        output[ch] = data[ch]; // testing latency

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