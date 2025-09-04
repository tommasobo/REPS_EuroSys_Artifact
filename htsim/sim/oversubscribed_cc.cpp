#include "oversubscribed_cc.h"
#include "uec.h"

simtime_picosec OversubscribedCC::_update_interval = timeFromUs(18u);
double OversubscribedCC::_target_congestion = 0.3;
double OversubscribedCC::_Ai = .05;
double OversubscribedCC::_Md = 0.75;
double OversubscribedCC::_min_rate = 0.01;

OversubscribedCC::OversubscribedCC(EventList& eventList,UecPullPacer* pacer)
    : EventSource(eventList, "OversubscribedCC"),
	_pullPacer(pacer){
	_rate = 1;
	_g = 0;
	_received_bytes = 0;
	_ecn_bytes = 0;
	_received = 0;
	_old_received = 0;
	_ecn = 0;
	_old_ecn = 0;
	_trimmed = 0;
	_old_trimmed = 0;

	eventList.sourceIsPendingRel(*this,(simtime_picosec)((0.75+drand()/2)*_update_interval));
}

void
OversubscribedCC::doNextEvent(){
    //decay_aggregate_congestion();
    doCongestionControl();
    eventlist().sourceIsPendingRel(*this,(simtime_picosec)((0.75+drand()/2)*_update_interval));
}

void 
OversubscribedCC::doCongestionControl(){
    int total_packets = _received - _old_received;
    int ecn = _ecn - _old_ecn;
    int trimmed = _trimmed - _old_trimmed;
    _old_received = _received;
    _old_ecn = _ecn;
    _old_trimmed = _trimmed;

    assert (ecn+trimmed<=total_packets);
    double fraction = 0;

    if (total_packets-trimmed!=0)
        fraction = (double)ecn / (total_packets - trimmed);

    _g = _g * 4/8 + 4 * fraction / 8; 

    if (_g>_target_congestion)
        _rate = _rate * (1 - (_g-_target_congestion)* _Md);
    else
        _rate += _Ai;

    if (_rate < _min_rate)
        _rate = _min_rate;
    if (_rate > 1)
        _rate = 1;

    if (UecSrc::_debug)
        cout << "At "<< timeAsUs(eventlist().now()) << " oversubscribed cc " << _g << " rate " << _rate << " flow " << _pullPacer->get_id() << endl;

    _pullPacer->updatePullRate(UecPullPacer::OVERSUBSCRIBED_CC, _rate);
}
