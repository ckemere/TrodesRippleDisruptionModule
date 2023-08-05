#ifndef MODULE_DEFINES_H
#define MODULE_DEFINES_H

#define SAMPLES_PER_SECOND 1500
#define DEFAULT_RIPPLE_THRESHOLD 3.0
#define DEFAULT_NUM_ACTIVE_CHANNELS 1
#define DEFAULT_MINIMUM_STIM_ISI 200.0
#define DEFAULT_MAXIMUM_STIM_RATE 2.0

struct RippleParameters {
    double ripple_threshold;
    unsigned int num_active_channels;
    unsigned int minimum_stim_isi; // in samples
    unsigned int minimum_stim_average_isi; // in samples
    bool post_detection_delay; 
};

#endif