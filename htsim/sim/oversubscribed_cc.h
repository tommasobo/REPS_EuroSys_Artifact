// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef OVERSUBSCRIBED_CC_H
#define OVERSUBSCRIBED_CC_H

#include <memory>
#include <tuple>
#include <list>

#include "eventlist.h"
#include "trigger.h"
#include "uecpacket.h"
#include "circular_buffer.h"
#include "modular_vector.h"

class UecPullPacer;

class OversubscribedCC : public EventSource {
public:
    OversubscribedCC(EventList& eventList, UecPullPacer * pacer);

    void doNextEvent();
    void doCongestionControl();

    inline void ecn_received(mem_b size) {_ecn++;_ecn_bytes += size;}
    inline void data_received(mem_b size) {_received++;_received_bytes += size;}
    inline void trimmed_received() {_trimmed++;}

    static double _target_congestion;
    static double _Ai, _Md;
    static simtime_picosec _update_interval;    
    static double _min_rate;

    inline static void setOversubscriptionRatio(int ratio) { _min_rate = 0.75/ratio; if (_min_rate < 0.01) _min_rate = 0.01; cout << "Setting min_rate to " << _min_rate * 100 << "% of linerate" << endl;}

private:
    double _rate;//total credit rate as dictated by observed congestion, computed dynamically.
    double _g;//marked packets average.

    UecPullPacer* _pullPacer = NULL;

    uint64_t _received_bytes, _ecn_bytes;
    uint64_t _received, _old_received;
    uint64_t _trimmed, _old_trimmed;
    uint64_t _ecn, _old_ecn;
};

#endif