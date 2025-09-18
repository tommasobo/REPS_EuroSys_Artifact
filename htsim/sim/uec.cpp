// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "uec.h"
#include <math.h>
#include <cstdint>
#include "circular_buffer.h"
#include "uec_logger.h"
#include "pciemodel.h"
#include "failuregenerator.h"
#include "data_collector.h"

using namespace std;

// Static stuff
bool UecSrc::_use_timeouts = true;
bool UecSink::_use_timeouts = true;
// _path_entropy_size is the number of paths we spray across.  If you don't set it, it will default
// to all paths.
uint32_t UecSrc::_path_entropy_size = 256;
int UecSrc::_global_node_count = 0;
bool UecSrc::_shown = false;
bool UecSrc::mprdma_fast_recovery = false;
bool UecSrc::_trim_disbled = false;
/* _min_rto can be tuned using setMinRTO. Don't change it here.  */
simtime_picosec UecSrc::_min_rto = timeFromUs((uint32_t)DEFAULT_UEC_RTO_MIN);

mem_b UecSink::_bytes_unacked_threshold = 16384;
int UecSink::TGT_EV_SIZE = 7;

bool UecSink::_model_pcie = false;

/* if you change _credit_per_pull, fix pktTime in the Pacer too - this assumes one pull per MTU */
UecBasePacket::pull_quanta UecSink::_credit_per_pull = 8;  // uints of typically 512 bytes

/* this default will be overridden from packet size*/
uint16_t UecSrc::_hdr_size = 64;
uint16_t UecSrc::_mss = 4096;
uint16_t UecSrc::_mtu = _mss + _hdr_size;

bool UecSrc::_mixed_lb_traffic = false;
int UecSrc::_num_mp_flows = 1;
bool UecSrc::_collect_data = false;
bool UecSrc::_save_rtt = false; 
bool UecSrc::_connections_mapping = false; 


bool UecSrc::_debug = false;

bool UecSrc::_sender_based_cc = false;

bool UecSrc::_receiver_based_cc = true;
bool UecSink::_oversubscribed_cc = false; // can only be enabled when receiver_based_cc is set to true

UecSrc::Sender_CC UecSrc::_sender_cc_algo = UecSrc::NSCC;
UecSrc::LoadBalancing_Algo UecSrc::_load_balancing_algo = UecSrc::BITMAP;

linkspeed_bps UecSrc::_reference_network_linkspeed = 0; // set by initNsccParams
simtime_picosec UecSrc::_reference_network_rtt = timeFromUs(12u); 
mem_b UecSrc::_reference_network_bdp = 0; // set by initNsccParams
linkspeed_bps UecSrc::_network_linkspeed = 0; // set by initNsccParams
simtime_picosec UecSrc::_network_rtt = 0; // set by initNsccParams
mem_b UecSrc::_network_bdp = 0; // set by initNsccParams
double UecSrc::_scaling_factor_a = 1; //for 400Gbps. cf. spec must be set to BDP/(100Gbps*12us)
double UecSrc::_scaling_factor_b = 0; // Needs to be inialized in initNscc
uint32_t UecSrc::_qa_scaling = 1; //quick adapt scaling - how much of the achieved bytes should we use as new CWND?
double UecSrc::_gamma = 0.8; //used for aggressive decrease
uint32_t UecSrc::_pi = 5000 * _scaling_factor_a;//pi = 5 * mtu_size for 400Gbps; proportional increase constant
double UecSrc::_alpha = UecSrc::_scaling_factor_a * 1000 * 4000 / timeFromUs(6u);
double UecSrc::_fi = 1; //fair_increase constant
double UecSrc::_fi_scale = .25 * UecSrc::_scaling_factor_a;

double UecSrc::_ecn_alpha = 0.125;
double UecSrc::_delay_alpha = 0.0125;//0.125;

simtime_picosec UecSrc::_adjust_period_threshold = timeFromUs(12u);
simtime_picosec UecSrc::_target_Qdelay = timeFromUs(6u);
uint32_t UecSrc::_adjust_bytes_threshold = (simtime_picosec)32000*_target_Qdelay/timeFromUs(12u);
double UecSrc::_qa_threshold = 4 * UecSrc::_target_Qdelay; 

double UecSrc::_eta = 0;
bool UecSrc::_enable_qa_gate = false;
bool UecSrc::_enable_avg_ecn_over_path = false;
bool UecSrc::_enable_fast_loss_recovery = false;

bool UecSrc::use_exp_avg_ecn = true;
// The params in the comments are those mentioned in smartt sigcomm24
// submission. These produced oscillations (fast increase then mult/fair
// decrease, then fast increase again...), the updated parameters don't.
double UecSrc::fast_increase_scaling_factor = 1;      // 2
double UecSrc::prop_increase_scaling_factor = 2;      // 2
double UecSrc::fair_increase_scaling_factor = 0.02;   // 0.006
double UecSrc::fair_decrease_scaling_factor = 0.5;    // 0.8
double UecSrc::mult_decrease_scaling_factor = 1;      // 2
double UecSrc::target_rtt_scaling_factor = 1.5;
bool UecSrc::use_fast_increase = true;

void UecSrc::initNsccParams(simtime_picosec network_rtt,
                            linkspeed_bps linkspeed){
    _reference_network_linkspeed = speedFromGbps(100);
    _reference_network_rtt = timeFromUs(12u); 
    _reference_network_bdp = timeAsSec(_reference_network_rtt)*(_reference_network_linkspeed/8);

    _network_linkspeed = linkspeed;
    _network_rtt = network_rtt; 
    _network_bdp = timeAsSec(_network_rtt)*(_network_linkspeed/8);

    _target_Qdelay = timeFromUs(6u);

    _qa_threshold = 4 * _target_Qdelay; 

    _scaling_factor_a = (double)_network_bdp/(double)_reference_network_bdp;
    _scaling_factor_b = (double)_target_Qdelay/(double)_reference_network_rtt; // no unit

    _alpha = 4.0*_scaling_factor_a*_scaling_factor_b*_mss/_target_Qdelay; // bytes/picosec
    _fi = 5*_scaling_factor_a;
    _eta = 0.15*_mss*_scaling_factor_a;

    _qa_scaling = 1; //quick adapt scaling - how much of the achieved bytes should we use as new CWND?
    _gamma = 0.8; //used for aggressive decrease
    _pi = 5000*_scaling_factor_a; // proportional increase constant
    _fi_scale = .25*_scaling_factor_a;

    _delay_alpha = 0.0125;

    _adjust_period_threshold = _reference_network_rtt;
    _adjust_bytes_threshold = (uint32_t)(16000*_scaling_factor_b);

    // Deprecated parameters, will be removed in the future.
    // constants for when FairDecrease is used
    // _fd = 0.8; //fair_decrease constant
    // Only in effect when fair decrease is enabled
    _ecn_alpha = 0.125;
    // _ecn_thresh = 0.3;

    cout << "Initializing static NSCC parameters:"
        << " _reference_network_linkspeed=" << _reference_network_linkspeed
        << " _reference_network_rtt=" << _reference_network_rtt
        << " _reference_network_bdp=" << _reference_network_bdp
        << " _target_Qdelay=" << _target_Qdelay
        << " _network_linkspeed=" << _network_linkspeed
        << " _network_rtt=" << _network_rtt
        << " _network_bdp=" << _network_bdp
        << " _qa_threshold=" << _qa_threshold
        << " _scaling_factor_a=" << _scaling_factor_a
        << " _scaling_factor_b=" << _scaling_factor_b
        << " _alpha=" << _alpha
        << " _fi=" << _fi
        << " _eta=" << _eta
        << " _qa_scaling=" << _qa_scaling
        << " _gamma=" << _gamma
        << " _pi=" << _pi
        << " _fi_scale=" << _fi_scale
        << " _delay_alpha=" << _delay_alpha 
        << " _adjust_period_threshold=" << _adjust_period_threshold
        << " _adjust_bytes_threshold=" << _adjust_bytes_threshold
        << endl;




}

void UecSrc::initNscc(mem_b cwnd, simtime_picosec peer_rtt) {
    _base_rtt = peer_rtt;
    _base_bdp = timeAsSec(_base_rtt)*(_nic.linkspeed()/8);
    _bdp = _base_bdp;
    _maxwnd =  1.5*_bdp;
    if (cwnd == 0) {
        _cwnd = _maxwnd;
    } else {
        _cwnd = cwnd;
    }

    cout << "Initialize per-instance NSCC parameters:"
        << " flowid " << _flow.flow_id()
        << " _base_rtt=" << _base_rtt
        << " _base_bdp=" << _base_bdp
        << " _bdp=" << _bdp
        << " _maxwnd=" << _maxwnd
        << " _cwnd=" << _cwnd
        << endl;
    
}

void UecSrc::initRccc(mem_b cwnd, simtime_picosec peer_rtt) {
    _base_rtt = peer_rtt;
    _base_bdp = timeAsSec(_base_rtt)*(_nic.linkspeed()/8);
    _bdp = _base_bdp;
    _maxwnd =  1.5*_bdp;
    if (cwnd == 0) {
        _cwnd = _maxwnd;
    } else {
        _cwnd = cwnd;
    }

    cout << "Initialize per-instance RCCC parameters:"
        << " flowid " << _flow.flow_id()
        << " _base_rtt=" << _base_rtt
        << " _base_bdp=" << _base_bdp
        << " _bdp=" << _bdp
        << " _maxwnd=" << _maxwnd
        << " _cwnd=" << _cwnd
        << endl;
}


flowid_t UecSrc::_debug_flowid = UINT32_MAX;

#define INIT_PULL 1000000000  // needs to be large enough we don't map
                            // negative pull targets (where
                            // credit_spec > backlog) to less than
                            // zero and suffer underflow.  Real
                            // implementations will properly handle
                            // modular wrapping.

/*
scaling_factor_a = current_BDP/100Gbps*net_base_rtt //12us scaling_factor_b = 12/target_Qdelay
beta = 5*scaling_factor_a
gamma = 0.15* scaling_factor_a
alpha = 4.0* scaling_factor_a*scaling_factor_b/base_rtt gamma_g = 0.8
*/

////////////////////////////////////////////////////////////////
//  UEC NIC
////////////////////////////////////////////////////////////////

UecNIC::UecNIC(id_t src_num, EventList& eventList, linkspeed_bps linkspeed, uint32_t ports)
    : EventSource(eventList, "uecNIC"), NIC(src_num)  {
    _nodename = "uecNIC" + to_string(src_num);
    _control_size = 0;
    _linkspeed = linkspeed;
    _num_queued_srcs = 0;
    _no_of_ports = ports;
    _ports.resize(_no_of_ports);
    for (uint32_t p = 0; p < _no_of_ports; p++) {
        _ports[p].send_end_time = 0;
        _ports[p].last_pktsize = 0;
        _ports[p].busy = false;
    }
    _busy_ports = 0;
    _rr_port = rand()%_no_of_ports; // start on a random port
    _ratio_data = 1;
    _ratio_control = 10;
    _crt = 0;
}

// srcs call request_sending to see if they can send now.  If the
// answer is no, they'll be called back when it's time to send.
const Route* UecNIC::requestSending(UecSrc& src) {
    if (UecSrc::_debug) {
        cout << src.nodename() << " requestSending at "
             << timeAsUs(EventList::getTheEventList().now()) << endl;
    }
    if (_busy_ports == _no_of_ports) {
        // we're already sending on all ports
        /*
        if (_num_queued_srcs == 0 && _control.empty()) {
            // need to schedule the callback
            eventlist().sourceIsPending(*this, _send_end_time);
        }
        */
        _num_queued_srcs += 1;
        _active_srcs.push_back(&src);
        return NULL;
    }
    assert(/*_num_queued_srcs == 0 &&*/ _control.empty());
    uint32_t portnum = findFreePort();
    return src.getPortRoute(portnum);
}

uint32_t UecNIC::findFreePort() {
    assert(_busy_ports < _no_of_ports);
    do {
        _rr_port = (_rr_port + 1) % _no_of_ports;

    } while (_ports[_rr_port].busy);
    return _rr_port;
}

uint32_t UecNIC::sendOnFreePortNow(simtime_picosec endtime, const Route* rt) {
    if (rt) {
        assert(_ports[_rr_port].busy == false);
    } else {
        _rr_port = findFreePort();
    }
    _ports[_rr_port].send_end_time = endtime;
    _ports[_rr_port].busy = true;
    _busy_ports++;
    eventlist().sourceIsPending(*this, endtime);
    return _rr_port;
}

// srcs call startSending when they are allowed to actually send
void UecNIC::startSending(UecSrc& src, mem_b pkt_size, const Route* rt) {
    if (UecSrc::_debug) {
        cout << src.nodename() << " startSending at "
             << timeAsUs(EventList::getTheEventList().now()) << endl;
    }

    
    if (_num_queued_srcs > 0) {
        UecSrc* queued_src = _active_srcs.front();
        _active_srcs.pop_front();
        _num_queued_srcs--;
        assert(_num_queued_srcs >= 0);
        assert(queued_src == &src);
    }

    simtime_picosec endtime = eventlist().now() + (pkt_size * 8 * timeFromSec(1.0)) / _linkspeed;
    sendOnFreePortNow(endtime, rt);
}

// srcs call cantSend when they previously requested to send, and now its their turn, they can't for
// some reason.
void UecNIC::cantSend(UecSrc& src) {
    if (UecSrc::_debug) {
        cout << src.nodename() << " cantSend at " << timeAsUs(EventList::getTheEventList().now())
             << endl;
    }

    if (_num_queued_srcs == 0 && _control.empty()) {
        // it was an immediate send, so nothing to do if we can't send after all
        return;
    }
    if (_num_queued_srcs > 0) {
        _num_queued_srcs--;

        UecSrc* queued_src = _active_srcs.front();
        _active_srcs.pop_front();

        assert(queued_src == &src);
        assert(_busy_ports < _no_of_ports);

        if (_num_queued_srcs > 0) {
            // give the next src a chance.
            queued_src = _active_srcs.front();
            const Route* route = queued_src->getPortRoute(findFreePort());
            queued_src->timeToSend(*route);
            return;
        }
    }
    if (!_control.empty()) {
        // need to send a control packet, since we didn't manage to send a data packet.
        sendControlPktNow();
    }
}

void UecNIC::sendControlPacket(UecBasePacket* pkt, UecSrc* src, UecSink* sink) {
    assert((src || sink) && !(src && sink));
    
    _control_size += pkt->size();
    CtrlPacket cp = {pkt, src, sink};
    _control.push_back(cp);

    if (UecSrc::_debug) {
        cout << "NIC " << this << " request to send control packet of type " << pkt->str()
             << " control queue size " << _control_size << " " << _control.size() << endl;
    }

    if (_busy_ports == _no_of_ports) {
        // all ports are busy
        if (UecSrc::_debug) {
            cout << "NIC sendControlPacket " << this << " already sending on all ports\n";
        }
    } else {
        // send now!
        sendControlPktNow();
    }
}

// actually do the send of a queued control packet
void UecNIC::sendControlPktNow() {
    assert(!_control.empty());
    assert(_busy_ports != _no_of_ports);
    
    CtrlPacket cp = _control.front();
    _control.pop_front();
    UecBasePacket* p = cp.pkt;

    simtime_picosec endtime  = eventlist().now() + (p->size() * 8 * timeFromSec(1.0)) / _linkspeed;
    uint32_t port_to_use = sendOnFreePortNow(endtime, NULL);
    if (UecSrc::_debug)
        cout << "NIC " << this << " send control of size " << p->size() << " at "
             << timeAsUs(eventlist().now()) << endl;

    _control_size -= p->size();
    assert(p->route() == NULL);
    const Route* route;
    if (cp.src)
        route = cp.src->getPortRoute(port_to_use);
    else
        route = cp.sink->getPortRoute(port_to_use);
    p->set_route(*route);
    p->sendOn();
}


void UecNIC::doNextEvent() {
    // doNextEvent should be called every time a packet will have finished being sent
    uint32_t last_port = _no_of_ports;
    for (uint32_t p = 0; p < _no_of_ports; p++) {
        if (_ports[p].busy && _ports[p].send_end_time == eventlist().now()) {
            last_port = p;
            break;
        }
    }
    assert(last_port != _no_of_ports);
    _busy_ports--;
    _ports[last_port].busy = false;

    if (UecSrc::_debug)
        cout << "NIC " << this << " doNextEvent at " << timeAsUs(eventlist().now()) << endl;

    if (_num_queued_srcs > 0 && !_control.empty()) {
        _crt++;

        if (_crt >= (_ratio_control + _ratio_data))
            _crt = 0;

        if (UecSrc::_debug) {
            cout << "NIC " << this << " round robin time between srcs " << _num_queued_srcs
                 << " and control " << _control.size() << " " << _crt;
        }

        if (_crt < _ratio_data) {
            // it's time for the next source to send
            UecSrc* queued_src = _active_srcs.front();
            const Route* route = queued_src->getPortRoute(findFreePort());
            queued_src->timeToSend(*route);

            if (UecSrc::_debug)
                cout << " send data " << endl;

            return;
        } else {
            sendControlPktNow();
            return;
        }
    }

    if (_num_queued_srcs > 0) {
        UecSrc* queued_src = _active_srcs.front();
        const Route* route = queued_src->getPortRoute(findFreePort());
        queued_src->timeToSend(*route);

        if (UecSrc::_debug)
            cout << "NIC " << this << " send data ONLY " << endl;
    } else if (!_control.empty()) {
        sendControlPktNow();
    }
}



////////////////////////////////////////////////////////////////
//  UEC SRC PORT
////////////////////////////////////////////////////////////////
UecSrcPort::UecSrcPort(UecSrc& src, uint32_t port_num)
    : _src(src), _port_num(port_num) {
}

void UecSrcPort::setRoute(const Route& route) {
    _route = &route;
}

void UecSrcPort::receivePacket(Packet& pkt) {
    _src.receivePacket(pkt, _port_num);
}

const string& UecSrcPort::nodename() {
    return _src.nodename();
}

////////////////////////////////////////////////////////////////
//  UEC SRC
////////////////////////////////////////////////////////////////

UecSrc::UecSrc(TrafficLogger* trafficLogger, EventList& eventList, UecNIC& nic, uint32_t no_of_ports, bool rts)
    : EventSource(eventList, "uecSrc"), _nic(nic), _flow(trafficLogger) {
    _node_num = _global_node_count++;
    _nodename = "uecSrc " + to_string(_node_num);

    _no_of_ports = no_of_ports;
    _ports.resize(no_of_ports);
    for (uint32_t p = 0; p < _no_of_ports; p++) {
        _ports[p] = new UecSrcPort(*this, p);
    }
        
    _rtx_timeout_pending = false;
    _rtx_timeout = timeInf;
    _rto_timer_handle = eventlist().nullHandle();

    _flow_logger = NULL;

    _rtt = _min_rto;

    _mdev = 0;
    _rto = _min_rto;
    _logger = NULL;

    _maxwnd = 50 * _mtu;
    _cwnd = _maxwnd;
    _flow_size = 0;
    _done_sending = false;
    _backlog = 0;
    _rtx_backlog = 0;
    _pull_target = INIT_PULL;
    _pull = INIT_PULL;
    _credit = _maxwnd;
    _speculating = true;
    _in_flight = 0;
    _highest_sent = 0;
    _send_blocked_on_nic = false;
    _no_of_paths = _path_entropy_size;
    _path_random = rand() % 0xffff;  // random upper bits of EV
    _path_xor = rand() % _no_of_paths;
    _current_ev_index = 0;

    //must be at least two, to allow us to encode assumed_bad state.
    _max_penalty = 15;
    _ev_skip_count = 0;
    _ev_bad_count = 0;
    _last_rts = 0;

    // stats for debugging
    _new_packets_sent = 0;
    _rtx_packets_sent = 0;
    _rts_packets_sent = 0;
    _bounces_received = 0;
    _acks_received = 0;


    
    


    if (_load_balancing_algo == BITMAP){
        nextEntropy = &UecSrc::nextEntropy_bitmap;
        processEv = &UecSrc::processEv_bitmap;
        _crt_path = 0;
        
        // reset path penalties
        _ev_skip_bitmap.resize(_no_of_paths);
        for (uint32_t i = 0; i < _no_of_paths; i++) {
            _ev_skip_bitmap[i] = 0;
        }
    } else if (_load_balancing_algo == REPS){
        nextEntropy = &UecSrc::nextEntropy_REPS;
        processEv = &UecSrc::processEv_REPS;
        _crt_path = 0;
    } else if (_load_balancing_algo == OBLIVIOUS ){
        nextEntropy = &UecSrc::nextEntropy_oblivious;
        processEv = &UecSrc::processEv_oblivious;
        _crt_path = 0;
    } else if (_load_balancing_algo == INCREMENTAL ){
        nextEntropy = &UecSrc::nextEntropy_incremental;
        processEv = &UecSrc::processEv_incremental;
        _crt_path = 0;
    } else if (_load_balancing_algo == MPRDMA ){
        nextEntropy = &UecSrc::nextEntropy_mprdma;
        processEv = &UecSrc::processEv_mprdma;
        _crt_path = 0;
    } else if (_load_balancing_algo == FLOWLET ){
        nextEntropy = &UecSrc::nextEntropy_flowlet;
        processEv = &UecSrc::processEv_flowlet;
        flowlet_entropy = _node_num % _no_of_paths;
    } else if (_load_balancing_algo == PLB ){
        nextEntropy = &UecSrc::nextEntropy_plb;
        processEv = &UecSrc::processEv_plb;
        plb_entropy = rand() % _no_of_paths;
    } else if (_load_balancing_algo == MP ){
        nextEntropy = &UecSrc::nextEntropy_mp;
        processEv = &UecSrc::processEv_mp;
        working_path_ecmp_mp = _node_num % _no_of_paths;
    } else if (_load_balancing_algo == FREEZING ){
        nextEntropy = &UecSrc::nextEntropy_freezing;
        processEv = &UecSrc::processEv_freezing;
        _crt_path = 0;
    } else if (_load_balancing_algo == ECMP){
        nextEntropy = &UecSrc::nextEntropy_ecmp;
        processEv = &UecSrc::processEv_ecmp;
        working_path_ecmp_mp = _node_num % _no_of_paths;
    }

    if (_mixed_lb_traffic && _node_num % 10 == 0) {
        nextEntropy = &UecSrc::nextEntropy_ecmp;
        processEv = &UecSrc::processEv_ecmp;
        background_traffic = true;
        _crt_path = 0;
        printf("Mixed load balancing traffic\n");
    }

    // by default, end silently
    _end_trigger = 0;

    _dstaddr = UINT32_MAX;
    //_route = NULL;
    _mtu = Packet::data_packet_size();
    _mss = _mtu - _hdr_size;

    _debug_src = UecSrc::_debug;
    _bdp = 0;
    _base_rtt = 0;

    _fi_count = 0;

    if (_sender_based_cc) {
        switch (_sender_cc_algo) {
            case DCTCP:
                updateCwndOnAck = &UecSrc::updateCwndOnAck_DCTCP;
                updateCwndOnNack = &UecSrc::updateCwndOnNack_DCTCP;
                break;
            case MPRDMA_CC:
                updateCwndOnAck = &UecSrc::updateCwndOnAck_MPRDMA;
                updateCwndOnNack = &UecSrc::updateCwndOnNack_MPRDMA;
                break;
            case NSCC:
                updateCwndOnAck = &UecSrc::updateCwndOnAck_NSCC;
                updateCwndOnNack = &UecSrc::updateCwndOnNack_NSCC;
                break;
            case SMARTT:
                updateCwndOnAck = &UecSrc::updateCwndOnAck_SMARTT;
                updateCwndOnNack = &UecSrc::updateCwndOnNack_SMARTT;
                smartt_main_loop = &UecSrc::smartt_vanilla_main_loop;
                break;
            case SMARTT_ECN_AIMD:
                updateCwndOnAck = &UecSrc::updateCwndOnAck_SMARTT;
                updateCwndOnNack = &UecSrc::updateCwndOnNack_SMARTT;
                smartt_main_loop = &UecSrc::smartt_ecn_aimd_main_loop;
                break;
            case SMARTT_ECN_AIFD:
                updateCwndOnAck = &UecSrc::updateCwndOnAck_SMARTT;
                updateCwndOnNack = &UecSrc::updateCwndOnNack_SMARTT;
                smartt_main_loop = &UecSrc::smartt_ecn_aifd_main_loop;
                break;
            case SMARTT_ECN_FIMD:
                updateCwndOnAck = &UecSrc::updateCwndOnAck_SMARTT;
                updateCwndOnNack = &UecSrc::updateCwndOnNack_SMARTT;
                smartt_main_loop = &UecSrc::smartt_ecn_fimd_main_loop;
                break;
            case SMARTT_ECN_FIFD:
                updateCwndOnAck = &UecSrc::updateCwndOnAck_SMARTT;
                updateCwndOnNack = &UecSrc::updateCwndOnNack_SMARTT;
                smartt_main_loop = &UecSrc::smartt_ecn_fifd_main_loop;
                break;
            case SMARTT_RTT:
                updateCwndOnAck = &UecSrc::updateCwndOnAck_SMARTT;
                updateCwndOnNack = &UecSrc::updateCwndOnNack_SMARTT;
                smartt_main_loop = &UecSrc::smartt_rtt_main_loop;
                break;
            case CONSTANT:
                updateCwndOnAck = &UecSrc::dontUpdateCwndOnAck;
                updateCwndOnNack = &UecSrc::dontUpdateCwndOnNack;
                break;
            default:
                cout << "Unknown CC algo specified " << _sender_cc_algo << endl;
                assert(0);
        }
    }
    //if (_node_num == 2) _debug_src = true; // use this to enable debugging on one flow at a
    // time
    _received_bytes = 0;
    _recvd_bytes = 0;

    _highest_recv_seqno = 0;
    _highest_rtx_sent = 0;

}


void UecSrc::delFromSendTimes(simtime_picosec time, UecDataPacket::seq_t seq_no) {
    //cout << eventlist().now() << " flowid " << _flow.flow_id() << " _send_times.erase " << time << " for " << seq_no << endl;
    auto snd_seq_range = _send_times.equal_range(time);
    auto snd_it = snd_seq_range.first;
    while (snd_it != snd_seq_range.second) {
        if (snd_it->second == seq_no) {
            _send_times.erase(snd_it);
            break;
        } else {
            ++snd_it;
        }
    }
}

void UecSrc::connectPort(uint32_t port_num,
                          Route& routeout,
                          Route& routeback,
                          UecSink& sink,
                          simtime_picosec start_time) {
    _ports[port_num]->setRoute(routeout);
    //_route = &routeout;

    _route_fail = &routeout;

    if (port_num == 0) {
        _sink = &sink;
        //_flow.set_id(get_id());  // identify the packet flow with the UEC source that generated it
        _flow._name = _name;

        if (start_time != TRIGGER_START) {
            eventlist().sourceIsPending(*this, start_time);
        }
    }
    assert(_sink == &sink);
    
    _sink->connectPort(port_num, *this, routeback);

    _src_dst_flowid =
        std::to_string(_srcaddr) + "_" + std::to_string(_dstaddr) + "_" + std::to_string(flowId());

    _src_dst =
        std::to_string(_srcaddr) + "_" + std::to_string(_dstaddr);
    
}

void UecSrc::receivePacket(Packet& pkt, uint32_t portnum) {

    FAILURE_GENERATOR->addRandomPacketDrop(pkt, this);
    FAILURE_GENERATOR->dropRandomPacket(pkt);


    if (FAILURE_GENERATOR->simNICFailures(this, NULL, pkt)) {
        // Return if using a timeout, otherwise transform it in a trim and mark it as failed
        if (_use_timeouts) {
            pkt.free();
            return;
        } else {
            if (!pkt.header_only()) {
                pkt.strip_payload(64);
            }
            pkt.is_failed = true;
        }
    }

    switch (pkt.type()) {
        case UECDATA: {
            _bounces_received++;
            // TBD - this is likely a Back-to-sender packet
            cout << "UecSrc::receivePacket receive UECDATA packets\n";

            abort();
        }
        case UECRTS: {
            cout << "UecSrc::receivePacket receive UECRTS packets\n";

            abort();
        }
        case UECACK: {
            processAck((const UecAckPacket&)pkt);
            if (_collect_data) {
                _list_cwd.push_back(std::make_pair(eventlist().now() / 1000, _cwnd));
            }
            // Remove
            /* if ((_load_balancing_algo == FREEZING) && !circular_buffer_reps->isFrozenMode()) {
                if (eventlist().now() > 150000000) {
                    circular_buffer_reps->setFrozenMode(true);
                    circular_buffer_reps->can_exit_frozen_mode = eventlist().now() +  circular_buffer_reps->exit_freeze_after;
                    printf("%s started freezing mode1 at %lu (can exit at %lu) - %d\n", _name.c_str(), eventlist().now() / 1000, circular_buffer_reps->can_exit_frozen_mode / 1000, circular_buffer_reps->isFrozenMode()); 
                    printf("Last Max RTT %lu - RTO Start %lu - Time is %lu - Base RTT is %lu -- Max %lu at %lu\n", _last_rto_max_rtt/1000, _last_rto_start/1000, eventlist().now()/1000, _base_rtt/1000, _max_rtt_seen/1000, _when__max_rtt_seen/1000);
                }  
            } */
            pkt.free();
            return;
        }
        case UECNACK: {
            processNack((const UecNackPacket&)pkt);
            pkt.free();
            return;
        }
        case UECPULL: {
            processPull((const UecPullPacket&)pkt);
            pkt.free();
            return;
        }
        default: {
            cout << "UecSrc::receivePacket receive default\n";

            abort();
        }
    }
}

mem_b UecSrc::handleAckno(UecDataPacket::seq_t ackno) {
    auto i = _tx_bitmap.find(ackno);
    if (i == _tx_bitmap.end()) {
        // The ackno is either in tx_bitmap or in rtx_queue
        // or in neither, but never in both.
        // Hence, if it's not in _tx_bitmap, check if it's 
        // in _rtx_queue and remove and correct. 
        // If ackno is in neither, there is nothing else
        // to do here.
        auto rtx_i = _rtx_queue.find(ackno);
        if (rtx_i != _rtx_queue.end()) {
            // packet was in RTX queue
            mem_b pkt_size = rtx_i->second;
            _rtx_queue.erase(rtx_i);
            _rtx_backlog -= pkt_size;
            _in_flight += pkt_size; // don't double count - we decremented when we marked for rtx
            if (_debug_src) {
                cout << "found pkt " << ackno << " in rtx queue\n";
            }
        }
        return 0;
    } else {
        // If ackno is in tx_bitmap, it means we have recentely 
        // send out an packet, either for the first time or
        // an rtx packet. Since the current ack tells us that
        // it has been received already, we can remove it from 
        // _tx_bitmap.
        simtime_picosec send_time = i->second.send_time;

        mem_b pkt_size = i->second.pkt_size;
        
        if (_debug_src)
            cout << _flow.str() << " " << _nodename << " handleAck " << ackno << " flow " << _flow.str() << endl;
        if(_flow.flow_id() == _debug_flowid ) {
              cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " handleAck ackno " << ackno
                   << endl;
        } 

        _tx_bitmap.erase(i);
        // _send_times.erase(send_time);
        delFromSendTimes(send_time, ackno);

        if (send_time == _rto_send_time) {
            recalculateRTO();
        }

        return pkt_size;
    }


    abort(); // dead code below
}

mem_b UecSrc::handleCumulativeAck(UecDataPacket::seq_t cum_ack) {
    mem_b newly_acked = 0;

    // free up anything cumulatively acked
    while (!_rtx_queue.empty()) {
        auto seqno = _rtx_queue.begin()->first;

        if (seqno < cum_ack) {
            mem_b pkt_size = _rtx_queue.begin()->second;
            _rtx_queue.erase(_rtx_queue.begin());
            _rtx_backlog -= pkt_size;
            _in_flight += pkt_size; // don't double count - we decremented when we marked for rtx
        } else {
            break;
        }
    }

    auto i = _tx_bitmap.begin();
    while (i != _tx_bitmap.end()) {
        auto seqno = i->first;
        // cumulative ack is next expected packet, not yet received
        if (seqno >= cum_ack) {
            // nothing else acked
            break;
        }
        simtime_picosec send_time = i->second.send_time;

        newly_acked += i->second.pkt_size;

        if (_debug_src)
            cout << _flow.str() << " " << _nodename << " handleCumAck " << seqno << " flow " << _flow.str() << endl;
        if(_flow.flow_id() == _debug_flowid ){
            cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " handleCumulativeAck seqno " << seqno
                << endl;
        }  
        _tx_bitmap.erase(i);
        i = _tx_bitmap.begin();
        // _send_times.erase(send_time);
        delFromSendTimes(send_time, seqno);
        if (send_time == _rto_send_time) {
            recalculateRTO();
        }
    }
    return newly_acked;
}

void UecSrc::handlePull(UecBasePacket::pull_quanta pullno) {
    if (pullno > _pull) {
        UecBasePacket::pull_quanta extra_credit = pullno - _pull;
        _credit += UecBasePacket::unquantize(extra_credit);
        if (_credit > _maxwnd)
            _credit = _maxwnd;
        _pull = pullno;
    }
    if(_flow.flow_id() == _debug_flowid){
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " credit " << _credit << endl; 
    }
}

bool UecSrc::checkFinished(UecDataPacket::seq_t cum_ack) {
    // cum_ack gives the next expected packet
    if (_done_sending) {
        // if (UecSrc::_debug) cout << _nodename << " checkFinished done sending " << " cum_acc "
        // << cum_ack << " mss " << _mss << " c*m " << cum_ack * _mss << endl;
        return true;
    }
    if (_debug_src)
        cout << _flow.str() << " " << _nodename << " checkFinished "
             << " cum_acc " << cum_ack << " mss " << _mss << " RTS sent " << _rts_packets_sent
             << " total bytes " << ((int64_t)cum_ack - _rts_packets_sent) * _mss << " flow_size "
             << _flow_size << " done_sending " << _done_sending << endl;

    if ((((mem_b)cum_ack - _rts_packets_sent) * _mss) >= _flow_size) {
        cout << "Flow " << _name << " flowId " << flowId() << " " << _nodename << " finished at "
             << timeAsUs(eventlist().now()-_flow_start_time) << " flowSize " << _flow_size << " total packets " << cum_ack << " RTS "
             << _rts_packets_sent << " total bytes " << ((mem_b)cum_ack - _rts_packets_sent) * _mss
             << " in_flight now " << _in_flight << " cwnd " << _cwnd << " sim time " << timeAsUs(eventlist().now()) << 
             " bg traffic " << background_traffic << endl;
        _speculating = false;
        

        if (_collect_data) {

            std::string base_dir = SAVE_DATA_FOLDER;
            
            // CWD
            std::string file_name = (base_dir + "/cwd/cwd" + _name + "_" + ".txt");
            std::ofstream MyFileCWD(file_name, std::ios::trunc);

            for (const auto& p : _list_cwd) {
                MyFileCWD << p.first << "," << p.second << std::endl;
            }

            MyFileCWD.close();


            // ECN
            file_name = (base_dir + "/ecn/ecn" + _name + "_" + ".txt");
            std::ofstream MyFileECN(file_name, std::ios::trunc);

            for (const auto& p : _list_ecn_received) {
                MyFileECN << p.first << "," << p.second << std::endl;
            }

            MyFileECN.close();

            // PSN Over Time
            file_name = (base_dir + "/psn_over_time/psn_over_time" + _name + "_" + ".txt");
            std::ofstream MyFilePSN(file_name, std::ios::trunc);

            for (const auto& p : _list_psn_over_time) {
                MyFilePSN << p << std::endl;
            }

            MyFilePSN.close();

            // NACK
            file_name = (base_dir + "/nack/nack" + _name + "_" + ".txt");
            std::ofstream MyFileNACK(file_name, std::ios::trunc);

            for (const auto& p : _list_nack) {
                MyFileNACK << p.first << "," << p.second << std::endl;
            }

            MyFileNACK.close();

            // RTT
            file_name = (base_dir + "/rtt/rtt" + _name + "_" + ".txt");
            std::ofstream MyFileRTT(file_name, std::ios::trunc);

            for (const auto& p : _list_rtt) {
                MyFileRTT << get<0>(p) << "," << get<1>(p) << "," << get<2>(p) << "," << get<3>(p)
                        << "," << get<4>(p) << "," << get<5>(p) << std::endl;
            }

            MyFileRTT.close();
        }


        if (_connections_mapping) {
            CONNECTION_INFO_MAP[_src_dst].buffer = circular_buffer_reps;
            CONNECTION_INFO_MAP[_src_dst].cwnd = _cwnd;
            CONNECTION_INFO_MAP[_src_dst].plb_entropy = plb_entropy;
            CONNECTION_INFO_MAP[_src_dst].flowlet_entropy = flowlet_entropy;
            CONNECTION_INFO_MAP[_src_dst].ev_skip_bitmap = _ev_skip_bitmap;
            CONNECTION_INFO_MAP[_src_dst].stop_time = eventlist().now();
            CONNECTION_INFO_MAP[_src_dst].working_path_ecmp_mp = working_path_ecmp_mp;
        }
        

        if (_end_trigger) {
            _end_trigger->activate();
        }
        /* if (_flow_logger) {
            _flow_logger->logEvent(_flow, *this, FlowEventLogger::FINISH, _flow_size, cum_ack);
        } */
        _done_sending = true;

        return true;
    }
    return false;
}

void UecSrc::processAck(const UecAckPacket& pkt) {
    _nic.logReceivedCtrl(pkt.size());
    
    auto cum_ack = pkt.cumulative_ack();

    if (_collect_data) {
        _list_psn_over_time.push_back(pkt.acked_psn());
    }

    if (flowId() == _debug_flowid) {
        printf("Received ACK at %d - %d - %d\n", eventlist().now(), cum_ack, _flow_size);
    }

    //handle flight_size based on recvd_bytes in packet.
    uint64_t newly_recvd_bytes = 0;

    if (pkt.recvd_bytes() > _recvd_bytes){
        newly_recvd_bytes = pkt.recvd_bytes() - _recvd_bytes;
        _recvd_bytes = pkt.recvd_bytes();

        _achieved_bytes += newly_recvd_bytes;
        _received_bytes += newly_recvd_bytes;
        _bytes_ignored += newly_recvd_bytes;
    }

    if (_debug_src) {
        cout << "processAck " << cum_ack << " ref_epsn " << pkt.acked_psn() << " recvd_bytes " << _recvd_bytes << " newly_recvd_bytes " << newly_recvd_bytes << endl;
    }


    /* if ("Uec_0_55" == _name) {
        cout << "processAck " << cum_ack << " ref_epsn " << pkt.acked_psn() << " recvd_bytes " << _recvd_bytes << " newly_recvd_bytes " << newly_recvd_bytes << endl;
    } */
    _acks_received++;

    //decrease flightsize.
    _in_flight -= newly_recvd_bytes;
    // We cannot run this next line's check here since 
    // _in_flight could be corrected (increased) in either
    // handleCumulativeAck or handleAckno.
    // assert(_in_flight >= 0);

    if (_sender_based_cc && pkt.rcv_wnd_pen() < 255) {
            sint64_t window_decrease = newly_recvd_bytes - newly_recvd_bytes * pkt.rcv_wnd_pen() / 255;
            _cwnd = max(_cwnd-window_decrease, (mem_b)_mtu);
    }

    //compute RTT sample
    auto acked_psn = pkt.acked_psn();
    auto i = _tx_bitmap.find(acked_psn);
    uint32_t ooo = pkt.ooo();

    if (pkt.ecn_echo()) {
        last_ecn = eventlist().now();
        if (_collect_data) {
            _list_ecn_received.push_back(std::make_pair(eventlist().now() / 1000, 1));
        }
    }

    mem_b pkt_size;
    simtime_picosec delay;
    simtime_picosec send_time = 0;
    if (i != _tx_bitmap.end()) {
        //auto seqno = i->first;
        send_time = i->second.send_time;
        pkt_size = i->second.pkt_size;
        _raw_rtt = eventlist().now() - send_time;
        if (_collect_data) {
            _list_rtt.push_back(std::make_tuple(eventlist().now() / 1000, _raw_rtt / 1000, 1, 1,
                                            _base_rtt / 1000, _target_Qdelay / 1000));
        }
        num_pkts++;
        if (_save_rtt) {
            printf("RTT: %ld - FlowSize %d\n", _raw_rtt, _flow_size);
        }
        if (eventlist().now() > _last_rto_start) {
            _last_rto_max_rtt = 0;
            num_pkts = 0;
            _last_rto_start = eventlist().now() + _rto * 2;
            //printf("UecSrc %s last_rto_start %ld at %lu\n", _name.c_str(), _last_rto_start / 1000, eventlist().now() /1000);
        }
        if (_raw_rtt > _last_rto_max_rtt) {
            _last_rto_max_rtt = _raw_rtt;
        }
        if (_raw_rtt > _max_rtt_seen) {
            _max_rtt_seen = _raw_rtt;
            _when__max_rtt_seen = eventlist().now();
        }
        update_base_rtt(_raw_rtt, pkt_size);
        if(_raw_rtt >= _base_rtt) {
            update_delay(_raw_rtt, true, pkt.ecn_echo());
            delay = _raw_rtt - _base_rtt; 
        }else{
            delay = get_avg_delay();
        }
    } else {
        // this can happen when the ACK arrives later than a cumulative ACK covering the NACKed
        // packet.
        if (UecSrc::_debug)
            cout << "Can't find send record for seqno " << acked_psn << endl;

        pkt_size = _mtu;
        delay = get_avg_delay();
    }

    handleCumulativeAck(cum_ack);

    if (_debug_src)
        cout << "At " << timeAsUs(eventlist().now()) << " " << _flow.str() << " " << _nodename << " processAck cum_ack: " << cum_ack << " flow " << _flow.str() << endl;

    auto ackno = pkt.ref_ack();

    uint64_t bitmap = pkt.bitmap();

    if (_debug_src)
        cout << "    ref_ack: " << ackno << " bitmap: " << bitmap << endl;

    while (bitmap > 0) {
        if (bitmap & 1) {
            if (_debug_src)
                cout << "    Sack " << ackno << " flow " << _flow.str() << endl;

            handleAckno(ackno);
            if (_highest_recv_seqno < ackno){
                _highest_recv_seqno = ackno;
            }
        }
        ackno++;
        bitmap >>= 1;
    }

    // We ran both potential _in_flight correcting functions
    // now check if we are in the negative.
    //assert(_in_flight >= 0);

    _loss_counter --;

    

    average_ecn_bytes(pkt_size,newly_recvd_bytes, pkt.ecn_echo());

    if(_flow.flow_id() == _debug_flowid ){
        cout <<  timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " track_avg_rtt " << timeAsUs(get_avg_delay())
            << " rtt " << timeAsUs(_raw_rtt) << " skip " << pkt.ecn_echo()  << " ev " << pkt.ev()
            << " _exp_avg_ecn " << _exp_avg_ecn
            << " cum_ack " << cum_ack
            << " bitmap_base " << pkt.ref_ack()
            << " ooo " << ooo
            << " cwnd " << _cwnd/get_avg_pktsize()
            << " _achieved_bytes " << _achieved_bytes
            << " acked_psn " << acked_psn
            << " sending_time " << timeAsUs(send_time)
            << endl;
    }


    mprdma_previous_cwnd = _cwnd;
    if (_sender_based_cc){
        /*if (pkt.ecn_echo()){
            (this->*updateCwndOnAck)(pkt.ecn_echo(), delay, pkt_size);
            (this->*updateCwndOnAck)(false, delay, newly_recvd_bytes - pkt_size);
        }
        else */
        (this->*updateCwndOnAck)(pkt.ecn_echo(), delay, newly_recvd_bytes);
    }

    if (_load_balancing_algo == MPRDMA) {
        if (_cwnd - mprdma_previous_cwnd > _mtu-66 || _cwnd > _in_flight + (_mtu-66)) {
            mprdma_can_send = true;
            //printf("MPRDMA %s CAN SEDN\n", nodename().c_str());
        } else {
            mprdma_can_send = false;
            //printf("%s Can not Send Because cwnd %d - in flight %d\n", nodename().c_str(), _cwnd, _in_flight);
        }
        //fflush(stdout);
    }
    
    (this->*processEv)(pkt.ev(), pkt.ecn_echo() ? PATH_ECN:PATH_GOOD);

    if (_debug_src) {
        cout << "At " << timeAsUs(eventlist().now()) << " " << _flow.str() << " " << _nodename << " processAck: " << cum_ack << " flow " << _flow.str() << " cwnd " << _cwnd << " flightsize " << _in_flight << " delay " << timeAsUs(delay) << " newlyrecvd " << newly_recvd_bytes << " skip " << pkt.ecn_echo() << " raw rtt " << _raw_rtt <<  " ecn avg " << _exp_avg_ecn << endl;
    }

    if (_sender_based_cc && _enable_fast_loss_recovery) {
        fastLossRecovery(ooo, cum_ack);
    }

    stopSpeculating();

    if (checkFinished(cum_ack)) {
        return;
    }

    sendIfPermitted();
}

/* 
    Register per flow metrics (flow size, flow start time, flow end time, flow completion time, base rtt,
    target rtt, rto, bdp and max cwnd)
*/
void UecSrc::logMetricFlow() {
    uint64_t start_time = static_cast<uint64_t>(timeAsNs(_flow_start_time));
    uint64_t end_time = static_cast<uint64_t>(timeAsNs(eventlist().now()));
    uint64_t fct_ns = end_time - start_time;
    simtime_picosec target_rtt = target_rtt_scaling_factor * _base_rtt;
    if (_sender_cc_algo == NSCC) {
        target_rtt = _base_rtt + _target_Qdelay;
    }
    _flow_metric->LogData({
        _src_dst_flowid,
        std::to_string(_flow_size),
        std::to_string(start_time),
        std::to_string(end_time),
        std::to_string(fct_ns),
        std::to_string((simtime_picosec)timeAsNs(_base_rtt)),
        std::to_string((simtime_picosec)timeAsNs(target_rtt)),
        std::to_string((simtime_picosec)timeAsNs(_rto)),
        std::to_string(_bdp),
        std::to_string(_maxwnd),
    });
}

// For each receiving packet at the sender, logs: rtt, ackedBytes, isNack and if the packet has ECN.
void UecSrc::logMetricAck(simtime_picosec rtt, int ackedBytes, bool isNack, bool hasECN) {
    _ack_metric->LogData({std::to_string((simtime_picosec)timeAsNs(rtt)),
                          std::to_string(ackedBytes), std::to_string(isNack),
                          std::to_string(hasECN)});
}

// For each change in the cwnd, log its value and the event that triggered it.
void UecSrc::logMetricCCEvent(CCEventType cc_action, uint64_t cwnd) {
    _cc_event_metric->LogData({std::to_string(cc_action), std::to_string(cwnd)});
}

// For each change in the cwnd, log its value.
void UecSrc::logMetricCwnd(uint64_t cwnd) {
    _cwnd_metric->LogData({std::to_string(cwnd)});
}

void UecSrc::updateCwndOnAck_DCTCP(bool skip, simtime_picosec rtt, mem_b newly_acked_bytes) {
    cout << timeAsUs(eventlist().now()) << " DCTCP start " << _name << " cwnd " << _cwnd
         << " with params skip " << skip << " acked bytes " << newly_acked_bytes << endl;

    if (skip == false)  // additive increase, 1 PKT /RTT
    {
        _cwnd += newly_acked_bytes * _mtu / _cwnd;

    } else {  // multiplicative decrease, done per mark, more aggressive than DCTCP (less smoothing)
              // but much simpler and more responsive since we don't need to track alpha.
        _cwnd -= newly_acked_bytes / 3;
        _cwnd = max((mem_b)_mtu, _cwnd);
    }
}

void UecSrc::updateCwndOnNack_MPRDMA(bool skip, mem_b nacked_bytes) {
    _cwnd -= nacked_bytes;
    _cwnd = max(_cwnd, (mem_b)_mtu);
}


void UecSrc::updateCwndOnAck_MPRDMA(bool skip, simtime_picosec rtt, mem_b newly_acked_bytes) {
    if (skip == false)  // additive increase, 1 PKT /RTT
    {
        if (mprdma_fast_recovery) {
            fast_increase(newly_acked_bytes, rtt);
        }
        _cwnd += newly_acked_bytes * _mtu / _cwnd;
        if (_cwnd > _maxwnd)
            _cwnd = _maxwnd;

    } else {  // multiplicative decrease, done per mark, more aggressive than DCTCP (less smoothing)
              // but much simpler and more responsive since we don't need to track alpha.
        _cwnd -= newly_acked_bytes / 4;
        _fi_count = 0;
        _cwnd = max((mem_b)_mtu, _cwnd);
    }
}

void UecSrc::updateCwndOnNack_DCTCP(bool skip, mem_b nacked_bytes) {
    _cwnd -= nacked_bytes;
    _cwnd = max(_cwnd, (mem_b)_mtu);
}

bool UecSrc::quick_adapt(bool is_loss, simtime_picosec avgqdelay) {
    if (_receiver_based_cc)
        return false;

    if (_debug_src){
        cout << "At " << timeAsUs(eventlist().now()) << " " << _flow.str() << " quickadapt called is loss "<< is_loss << " delay " << avgqdelay << " qa_endtime " << timeAsUs(_qa_endtime) << " trigger qa " << _trigger_qa << endl;
    }
    if (eventlist().now()>_qa_endtime){
        bool qa_gate = true;
        if (_enable_qa_gate ){
            qa_gate = (_achieved_bytes < _maxwnd/8);
        }
        if (_qa_endtime != 0 && (_trigger_qa || is_loss || (avgqdelay > _qa_threshold)) && qa_gate ){
            if (_debug_src) {
                cout << "At " << timeAsUs(eventlist().now()) << " " << _flow.str() << " running quickadapt, CWND is " << _cwnd << " setting it to " << _achieved_bytes <<  endl;
            }

            if (_cwnd < _achieved_bytes){
                if (_debug_src) {
                    cout << "This shouldn't happen: QUICK ADAPT MIGHT INCREASE THE CWND" << endl;
                }
            }
            else {
                _cwnd = max(_achieved_bytes, (mem_b)_mtu); //* _qa_scaling;
                if(_flow.flow_id() == _debug_flowid)
                    cout <<timeAsUs(eventlist().now()) <<" flowid " << _flow.flow_id()<< " quick_adapt  _cwnd " << _cwnd << " is_loss " << is_loss << endl;
                _bytes_to_ignore = _in_flight;
                _bytes_ignored = 0;
                _trigger_qa = false;
                return true;
            }
        }
        _achieved_bytes = 0;
        _qa_endtime = eventlist().now() + _base_rtt + _target_Qdelay;
    }
    return false;
}

void UecSrc::fair_increase(uint32_t newly_acked_bytes){
    _inc_bytes += _fi * _mtu * newly_acked_bytes; //increase by 16Million!
}

void UecSrc::proportional_increase(uint32_t newly_acked_bytes,simtime_picosec delay){
    fast_increase(newly_acked_bytes, delay);
    if (_increase)
        return;
    
    //make sure targetQdelay > delay;
    assert(_target_Qdelay > delay);

    _inc_bytes += (uint32_t)round(_alpha * (_target_Qdelay - delay) * (double)newly_acked_bytes);

    fair_increase(newly_acked_bytes);
}

void UecSrc::fast_increase(uint32_t newly_acked_bytes,simtime_picosec delay){
    if (delay < timeFromUs(1u)){
        _fi_count += newly_acked_bytes;
        if (_fi_count > _cwnd || _increase){
            _cwnd += newly_acked_bytes * 0.5;

            if (_cwnd > _maxwnd)
                _cwnd = _maxwnd;
            _increase = true;
            return;
        }
    }
    else  {
        _fi_count = 0;
    }
    _increase = false;
}


void UecSrc::multiplicative_decrease(uint32_t newly_acked_bytes){
    _increase = false;
    _fi_count = 0;
    simtime_picosec avg_delay = get_avg_delay();
    if (avg_delay > _target_Qdelay){
        if (eventlist().now() - _last_dec_time > _base_rtt){
            _cwnd *= max(1-_gamma*(avg_delay-_target_Qdelay)/avg_delay, 0.5);/*_max_md_jump instead of 1*/
            _last_dec_time = eventlist().now();
        }
        // fair_decrease(can_decrease, newly_acked_bytes);
    }
}

void UecSrc::fulfill_adjustment(){
    assert(_bdp > 0);
    if (_debug_src) {
        cout << "Running fulfill adjustment cwnd " << _cwnd << " inc " << _inc_bytes << " bdp " << _bdp << endl;
    }
    _cwnd += min((mem_b)_adjust_bytes_threshold, _inc_bytes / _cwnd); 

    _inc_bytes = 0;

    //unclear what received_bytes this is referring to. 
    _received_bytes = 0;
    _last_adjust_time = eventlist().now();
}

void UecSrc::mark_packet_for_retransmission(UecBasePacket::seq_t psn, uint16_t pktsize){
    _in_flight -= pktsize;
    //assert (_in_flight>=0);
    if (_sender_cc_algo != CONSTANT) {
        
        _cwnd = max(_cwnd - pktsize, (mem_b)_mtu);
    }
    
    //_rtx_count ++;
}

void UecSrc::dontUpdateCwndOnAck(bool skip, simtime_picosec delay, mem_b newly_acked_bytes) {
}


void UecSrc::updateCwndOnAck_NSCC(bool skip, simtime_picosec delay, mem_b newly_acked_bytes) {
    // bool can_decrease = _exp_avg_ecn > _ecn_thresh;

    if (_bytes_ignored < _bytes_to_ignore && skip)
        return;

    simtime_picosec avg_delay = get_avg_delay();

    if (quick_adapt(false,avg_delay))
        return;

    if (!skip && delay >= _target_Qdelay) {
        fair_increase(newly_acked_bytes);
        if (_flow.flow_id() == _debug_flowid || UecSrc::_debug) {
            cout << timeAsUs(eventlist().now()) <<" flowid " << _flow.flow_id()<< " " << _flow.str() << " fair_increase  _nscc_cwnd " << _cwnd << endl;
        }
    } else if (!skip && delay < _target_Qdelay) {
        proportional_increase(newly_acked_bytes,delay);
        if (_flow.flow_id() == _debug_flowid || UecSrc::_debug) {
            cout << timeAsUs(eventlist().now()) <<" flowid " << _flow.flow_id()<< " " << _flow.str() << " proportional_increase _nscc_cwnd " << _cwnd << endl;
        }
    } else if (skip && delay >= _target_Qdelay) {    
        multiplicative_decrease(newly_acked_bytes);
        if (_flow.flow_id() == _debug_flowid || UecSrc::_debug) {
            cout << timeAsUs(eventlist().now()) <<" flowid " << _flow.flow_id()<< " " << _flow.str() << " multiplicative_decrease _nscc_cwnd " << _cwnd << endl;
        }
    } else if (skip && delay < _target_Qdelay) {
        // fair_decrease(can_decrease,newly_acked_bytes);
        // if (_flow.flow_id() == _debug_flowid || UecSrc::_debug) {
        //     cout << timeAsUs(eventlist().now()) <<" flowid " << _flow.flow_id()<< " " << _flow.str() << " fair_decrease _nscc_cwnd " << _cwnd
        //         <<" mtu " << _mtu
        //         << "_maxwnd " << _maxwnd << endl;
        // }
        // FD is out.
        // NOOP, just switch path
    }

    // if ( _received_bytes > _adjust_bytes_threshold || eventlist().now() - _last_adjust_time > _adjust_period_threshold ) {
    if ( _received_bytes > _adjust_bytes_threshold || eventlist().now() - _last_adjust_time > _adjust_period_threshold ) {
        if (_flow.flow_id() == _debug_flowid || UecSrc::_debug) {
            cout << timeAsUs(eventlist().now()) <<" flowid " << _flow.flow_id()<<  " " << _flow.str() << " fulfill_adjustmentx _nscc_cwnd " << _cwnd
                << " inc_bytes " << _inc_bytes
                << endl;
        }
        fulfill_adjustment();
        if (_flow.flow_id() == _debug_flowid || UecSrc::_debug) {
            cout << timeAsUs(eventlist().now()) <<" flowid " << _flow.flow_id()<< " " << _flow.str() << " fulfill_adjustment _nscc_cwnd " << _cwnd << endl;
        }
    }

    if (eventlist().now() - _last_eta_time > _adjust_period_threshold ) {
        _cwnd += _eta;
        _last_eta_time = eventlist().now();
        if (_flow.flow_id() == _debug_flowid) {
            cout << timeAsUs(eventlist().now()) <<" flowid " << _flow.flow_id()<< " " << _flow.str() << " eta _nscc_cwnd " << _cwnd << " target_q_delay " <<timeAsUs(_target_Qdelay) << endl;
        }
    }

    if (_cwnd < _mtu)
        _cwnd = _mtu;

    if (_cwnd > _maxwnd)
        _cwnd = _maxwnd;
    if (_flow.flow_id() == _debug_flowid)
        cout << timeAsUs(eventlist().now()) <<" flowid " << _flow.flow_id()<< " final _nscc_cwnd " << _cwnd << " _basertt " << timeAsUs(_base_rtt)<< endl;
}

void UecSrc::updateCwndOnNack_NSCC(bool skip, mem_b nacked_bytes) {
    _cwnd -= nacked_bytes;
    if (_cwnd < _mtu)
        _cwnd = _mtu;

    _trigger_qa = true;
    if (_bytes_ignored >= _bytes_to_ignore)
        quick_adapt(true, get_avg_delay());    
}

void UecSrc::smartt_update_fast_increase_state(bool skip, simtime_picosec delay) {
    if (delay <= timeFromUs(1u) && !skip) {
        counter_consecutive_good_bytes += _mtu;
    } else {
        target_window = _cwnd;
        counter_consecutive_good_bytes = 0;
        increasing = false;
    }
}

bool UecSrc::smartt_should_fast_increase()
{
    return ((increasing || counter_consecutive_good_bytes > target_window) && use_fast_increase);
}

void UecSrc::smartt_fast_increase()
{
    if (_bytes_ignored > _bytes_to_ignore)
    {
        _cwnd += fast_increase_scaling_factor * _mtu;
        _cwnd = min(_cwnd, _maxwnd);
    }

    increasing = true;
}

bool UecSrc::smartt_check_and_do_quick_adapt(bool skip, bool trimmed) {
    // Checks if we have recvd acks after a previous quick adapt. If so then
    // checks if we need to do quick adapt now.
    if (_bytes_ignored >= _bytes_to_ignore)
    {
        smartt_quick_adapt(false);
    }

    // Checks if we have recvd acks after a previous quick adapt. If not, we
    // shouldn't make cwnd adjustments and wait for quick adapt changes to
    // relfect in the network. Returns true if cwnd adjustments are allowed.
    if (_bytes_ignored < _bytes_to_ignore && skip) {
        return false;
    }
    return true;
}

void UecSrc::smartt_quick_adapt(bool trimmed) {
    if (eventlist().now() >= _qa_endtime) {
        previous_window_end = _qa_endtime;
        saved_acked_bytes = _achieved_bytes;
        _achieved_bytes = 0;
        uint64_t _target_rtt = _base_rtt + _base_rtt / 2;
        _qa_endtime = eventlist().now() + _target_rtt;
        // Enable Fast Drop
        if ((trimmed || need_quick_adapt) && previous_window_end != 0) {

            _cwnd = max((double)(saved_acked_bytes * 1),
                        (double)_mtu);

            // Reset counters, update logs.
            _bytes_to_ignore = _in_flight;
            _bytes_ignored = 0;
            need_quick_adapt = false;
        }
    }
}

void UecSrc::smartt_ecn_aimd_main_loop(bool skip, simtime_picosec delay) {
    bool can_decrease_exp_avg = smartt_wtd_can_decrease();
    if(!skip) {
        _cwnd +=
            ((double)_mtu / _cwnd) * (fair_increase_scaling_factor * _network_bdp);  // TODO: Ideally this should be 1, currently it is 0.2
    }
    else {
        if (can_decrease_exp_avg) {
            _cwnd -= _mtu * fair_decrease_scaling_factor;  // TODO: Ideally this should be 0.5
        }
    }
}

void UecSrc::smartt_ecn_aifd_main_loop(bool skip, simtime_picosec delay) {
    bool can_decrease_exp_avg = smartt_wtd_can_decrease();
    if(!skip) {
        _cwnd +=
            ((double)_mtu / _cwnd) * (fair_increase_scaling_factor * _network_bdp);
    }
    else {
        if (can_decrease_exp_avg) {
            _cwnd -= (static_cast<double>(_cwnd) / _bdp * _mtu * fair_decrease_scaling_factor);
        }
    }
}

void UecSrc::smartt_ecn_fimd_main_loop(bool skip, simtime_picosec delay) {
    bool can_decrease_exp_avg = smartt_wtd_can_decrease();
    if(!skip) {
        _cwnd +=
            ((double)_mtu / _cwnd) * (fair_increase_scaling_factor * _bdp);
    }
    else {
        if (can_decrease_exp_avg) {
            _cwnd -= _mtu * fair_decrease_scaling_factor;
        }
    }
}

void UecSrc::smartt_ecn_fifd_main_loop(bool skip, simtime_picosec delay) {
        bool can_decrease_exp_avg = smartt_wtd_can_decrease();
    if(!skip) {
        _cwnd +=
            ((double)_mtu / _cwnd) * (fair_increase_scaling_factor * _bdp);
    }
    else {
        if (can_decrease_exp_avg) {
            _cwnd -= (static_cast<double>(_cwnd) / _bdp * _mtu * fair_decrease_scaling_factor);
        }
    }
}

void UecSrc::smartt_rtt_main_loop(bool skip, simtime_picosec delay) {
    bool can_decrease_exp_avg = smartt_wtd_can_decrease();
    uint32_t rtt = delay + _base_rtt;
    uint32_t _target_rtt = target_rtt_scaling_factor * _base_rtt;

    //  Case 1 Hybrid Based Increase || RTT Increase
    if (!skip && rtt < _target_rtt) {
        _cwnd += (min(uint32_t((((_target_rtt - rtt) / (double)rtt) *
                                prop_increase_scaling_factor * _mtu * (_mtu / (double)_cwnd))),
                        uint32_t(_mtu)));

        _cwnd +=
            ((double)_mtu / _cwnd) * (fair_increase_scaling_factor * _bdp);
    }

    //  Case 2 Hybrid Based Decrease || RTT Decrease
    else if (skip && rtt > _target_rtt) {
        if (can_decrease_exp_avg) {
            _cwnd -= min(((mult_decrease_scaling_factor * ((rtt - (double)_target_rtt) / rtt) *
                            _mtu) +
                            _cwnd / (double)_bdp * fair_decrease_scaling_factor * _mtu),
                            (double)_mtu / 1);
        }
    }

    //  Case 3 Gentle Decrease (Window based)
    else if (skip && rtt < _target_rtt) {
    }

    //  Case 4 Do nothing but fairness
    else if (!skip && rtt > _target_rtt) {
        _cwnd +=
            ((double)_mtu / _cwnd) * (fair_increase_scaling_factor * _bdp);
    }

    else {
    }
}

void UecSrc::smartt_vanilla_main_loop(bool skip, simtime_picosec delay) {
    bool can_decrease_exp_avg = smartt_wtd_can_decrease();
    uint32_t rtt = delay + _base_rtt;
    uint32_t _target_rtt = target_rtt_scaling_factor * _base_rtt;

    //  Case 1 Hybrid Based Increase || RTT Increase
    if (!skip && rtt < _target_rtt) {
        _cwnd += (min(uint32_t((((_target_rtt - rtt) / (double)rtt) *
                                prop_increase_scaling_factor * _mtu * (_mtu / (double)_cwnd))),
                        uint32_t(_mtu)));

        _cwnd +=
            ((double)_mtu / _cwnd) * (fair_increase_scaling_factor * _bdp);
    }

    //  Case 2 Hybrid Based Decrease || RTT Decrease
    else if (skip && rtt > _target_rtt) {
        if (can_decrease_exp_avg) {
            _cwnd -= min(((mult_decrease_scaling_factor * ((rtt - (double)_target_rtt) / rtt) *
                            _mtu) +
                            _cwnd / (double)_bdp * fair_decrease_scaling_factor * _mtu),
                            (double)_mtu / 1);
        }
    }

    //  Case 3 Gentle Decrease (Window based)
    else if (skip && rtt < _target_rtt) {
        if (can_decrease_exp_avg) {
            _cwnd -= (static_cast<double>(_cwnd) / _bdp * _mtu * fair_decrease_scaling_factor);
        }
    }

    //  Case 4 Do nothing but fairness
    else if (!skip && rtt > _target_rtt) {
        _cwnd +=
            ((double)_mtu / _cwnd) * (fair_increase_scaling_factor * _bdp);
    }

    else {
    }
}

void UecSrc::smartt_update_wtd_state(bool skip) {
    // skip = ECN mark
    exp_avg_ecn = exp_avg_alpha * skip + (1 - exp_avg_alpha) * exp_avg_ecn;
}

bool UecSrc::smartt_wtd_can_decrease() {
    if (!use_exp_avg_ecn) {
        return true;
    }
    if (exp_avg_ecn > exp_avg_ecn_value) {
        return true;
    }
    return false;
}

void UecSrc::updateCwndOnAck_SMARTT(bool skip, simtime_picosec delay, mem_b newly_acked_bytes) {
    smartt_update_wtd_state(skip);
    smartt_update_fast_increase_state(skip, delay);
    bool cwnd_adjust_allowed = smartt_check_and_do_quick_adapt(skip, false);

    if (cwnd_adjust_allowed) {
        if (smartt_should_fast_increase()) {
            smartt_fast_increase();
        } else {
            (this->*smartt_main_loop)(skip, delay);
        }
    }
    clamp_cwnd();
}

void UecSrc::clamp_cwnd() {
    if (_cwnd < _mtu)
        _cwnd = _mtu;

    if (_cwnd > _maxwnd)
        _cwnd = _maxwnd;
}


void UecSrc::updateCwndOnNack_SMARTT(bool skip, mem_b nacked_bytes) {
    _achieved_bytes += 64; // ACK Size, hardcoded for now
    if (_bytes_ignored >= _bytes_to_ignore) {
        _cwnd -= nacked_bytes;
        _cwnd = max(_cwnd, (mem_b)_mtu);
        need_quick_adapt = true;
        smartt_quick_adapt(true);
    }
}

void UecSrc::dontUpdateCwndOnNack(bool skip, mem_b nacked_bytes) {
}

void UecSrc::update_base_rtt(simtime_picosec raw_rtt, uint16_t packet_size){
    if (_base_rtt > _raw_rtt && packet_size == _mtu) {
        _base_rtt = _raw_rtt;
        _bdp = timeAsUs(_raw_rtt) * _nic.linkspeed() / 8000000; 
        _maxwnd = 1.5 * _bdp;
        
        if (UecSrc::_debug)
            cout << "Reinit BDP and MAXWND to "  << _bdp << " " << _maxwnd << " in pkts " << _maxwnd/_mtu << endl;
    }
}

void UecSrc::update_delay(simtime_picosec raw_rtt, bool update_avg, bool skip){
    simtime_picosec delay = _raw_rtt - _base_rtt;
    if(update_avg){

        if(skip == false && delay > _target_Qdelay){
            _avg_delay = _delay_alpha * _base_rtt*0.25 + (1-_delay_alpha) * _avg_delay;
        }else{
            if (delay > 5*_base_rtt)
            {
                double r = 0.0125;
                _avg_delay = r * delay + (1 - r) * _avg_delay;
            }
            else
            {
                _avg_delay = _delay_alpha * delay + (1 - _delay_alpha) * _avg_delay;
            }
        }
    }
    if (_debug_src) {
        cout << "Update delay with sample " << timeAsUs(delay) << " avg is " << timeAsUs(_avg_delay) << " base rtt is " << _base_rtt << endl;
    }
}

simtime_picosec UecSrc::get_avg_delay(){
    return _avg_delay;
}

uint16_t UecSrc::get_avg_pktsize(){
    return _mss;  // does not include header
}
void UecSrc::average_ecn_bytes(uint32_t pktsize, uint32_t newly_acked_bytes, bool skip) {
    //double value = (double)skip * pktsize / newly_acked_bytes;
    //_exp_avg_ecn = _ecn_alpha * value + (1 - _ecn_alpha) * _exp_avg_ecn;

    _exp_avg_ecn = _ecn_alpha * skip + (1 - _ecn_alpha) * _exp_avg_ecn;

    int32_t nab = newly_acked_bytes;
    nab -= pktsize;

    while (nab > 0) {
        nab -= _mtu;          
        _exp_avg_ecn = (1 - _ecn_alpha) * _exp_avg_ecn;
    }
}

void UecSrc::fastLossRecovery(uint32_t ooo, UecBasePacket::seq_t cum_ack) {
    uint16_t avg_size = get_avg_pktsize();
    int threshold = min(_cwnd, _maxwnd);
    threshold = max(threshold, 5*avg_size);
    if(_flow.flow_id() == _debug_flowid ){
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " rtx_threshold " << threshold/avg_size
            << " ooo " << ooo
            << " _highest_rtx_sent " << _highest_rtx_sent
            << " cwnd_in_pkts " << _cwnd/avg_size
            << " cum_ack " << cum_ack
            << endl;
    }
    if (cum_ack >= _recovery_seqno && _loss_recovery_mode){
        _loss_recovery_mode = false;
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " exit_loss " <<endl;
    }
    if(ooo < threshold/avg_size && !_loss_recovery_mode)
        return;
    if (!_loss_recovery_mode){
        _loss_recovery_mode = true;
        _loss_counter = 0;
        _recovery_seqno = _highest_recv_seqno;
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " enter_loss " <<endl;
    }

    // move the packet to the RTX queue
    for (UecBasePacket::seq_t rtx_seqno = cum_ack; rtx_seqno < _recovery_seqno &&  _loss_counter < _cwnd/get_avg_pktsize() ; rtx_seqno ++ ){
        if (rtx_seqno < _highest_rtx_sent)
            continue;
        auto i = _tx_bitmap.find(rtx_seqno);
        if (i == _tx_bitmap.end())
        {
            // this means this packet seqno has been acked.
            continue;
        }

        if (_rtx_queue.find(rtx_seqno) != _rtx_queue.end()){
            continue;
        }
        if(_flow.flow_id() == _debug_flowid ){
            cout <<  timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " rtx_seqno " << rtx_seqno
                << " _highest_recv_seqno "<< _highest_recv_seqno
                << " recovery_seqno " << _recovery_seqno
                << endl;
        }       

        mem_b pkt_size = i->second.pkt_size;
        assert(pkt_size >= _hdr_size); // check we're not seeing NACKed RTS packets.
        auto seqno = i->first;
        simtime_picosec send_time = i->second.send_time;
        _tx_bitmap.erase(i);
        assert(_tx_bitmap.find(seqno) == _tx_bitmap.end()); // xxx remove when working

        _in_flight -= pkt_size;

        // _send_times.erase(send_time);
        delFromSendTimes(send_time, rtx_seqno);
        _highest_rtx_sent = seqno+1;
        _loss_counter ++;
        queueForRtx(seqno, pkt_size);

        if (send_time == _rto_send_time)
        {
            recalculateRTO();
        }
        // penalizePath(ev, 1);
    }
    sendIfPermitted();
}

void UecSrc::processNack(const UecNackPacket& pkt) {
    _nic.logReceivedCtrl(pkt.size());

    // auto pullno = pkt.pullno();
    // handlePull(pullno);

    auto nacked_seqno = pkt.ref_ack();
    if (_debug_src) {
        cout << _flow.str() << " " << _nodename << " processNack nacked: " << nacked_seqno << " flow " << _flow.str()
             << endl;
    }

    if (_collect_data) {
        _list_nack.push_back(std::make_pair(eventlist().now() / 1000, 1));
    }

    uint16_t ev = pkt.ev();
    // what should we do when we get a NACK with ECN_ECHO set?  Presumably ECE is superfluous?
    // bool ecn_echo = pkt.ecn_echo();

    // move the packet to the RTX queue
    auto i = _tx_bitmap.find(nacked_seqno);
    if (i == _tx_bitmap.end()) {
        if (_debug_src)
            cout << _flow.str() << " " << "Didn't find NACKed packet in _active_packets flow " << _flow.str() << endl;

        // this abort is here because this is unlikely to happen in
        // simulation - when it does, it is usually due to a bug
        // elsewhere.  But if you discover a case where this happens
        // for real, remove the abort and uncomment the return below.
        //abort();
        // this can happen when the NACK arrives later than a cumulative ACK covering the NACKed
        // packet. 
        return;
    }

    mem_b pkt_size = i->second.pkt_size;

    assert(pkt_size >= _hdr_size);  // check we're not seeing NACKed RTS packets.
    if (pkt_size == _hdr_size) {
        _stats.rts_nacks++;
    }

    auto seqno = i->first;
    simtime_picosec send_time = i->second.send_time;

    _raw_rtt = eventlist().now() - send_time;
    if(_raw_rtt > _base_rtt)
        update_delay(_raw_rtt, false, true);

    if(_enable_avg_ecn_over_path){
        _exp_avg_ecn = _ecn_alpha * 1.0  + (1 - _ecn_alpha) * _exp_avg_ecn;
    }
    if(_flow.flow_id() == _debug_flowid){
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " ev " << ev 
            << " seqno " << seqno
            << " trimming " << endl;
    }
    if (_sender_based_cc){
        (this->*updateCwndOnNack)(ev, pkt_size);
    }

    if (_debug_src)
        cout << _flow.str() << " " << _nodename << " erasing send record, seqno: " << seqno << " flow " << _flow.str()
             << endl;
    _tx_bitmap.erase(i);
    assert(_tx_bitmap.find(seqno) == _tx_bitmap.end());  // xxx remove when working

    _in_flight -= pkt_size;
    //assert(_in_flight >= 0);

    // _send_times.erase(send_time);
    delFromSendTimes(send_time, seqno);

    stopSpeculating();
    queueForRtx(seqno, pkt_size);

    if (send_time == _rto_send_time) {
        recalculateRTO();
    }

    (this->*processEv)(ev, PATH_NACK);

    sendIfPermitted();
}

void UecSrc::processPull(const UecPullPacket& pkt) {
    _nic.logReceivedCtrl(pkt.size());

    auto pullno = pkt.pullno();
    if (_debug_src)
        cout << timeAsUs(eventlist().now()) << " flow " << _flow.str() << " " << _nodename << " processPull " << pullno << " flow " << _flow.str() << " SP " << pkt.is_slow_pull() << endl;
    if (_flow.flow_id() == _debug_flowid){
        cout << timeAsUs(eventlist().now())<< " flowid " << _flow.flow_id() << " processPull " << pullno  << " SP " << pkt.is_slow_pull() << endl;
    }
    stopSpeculating();
    handlePull(pullno);
    sendIfPermitted();
}

void UecSrc::doNextEvent() {
    // a timer event fired.  Can either be a timeout, or the timed start of the flow.
    if (_rtx_timeout_pending) {
        clearRTO();
        assert(_logger == 0);

        if (_logger)
            _logger->logUec(*this, UecLogger::UEC_TIMEOUT);

        rtxTimerExpired();
    } else {
        if (_debug_src)
            cout << _flow.str() << " " << "Starting flow " << _name << endl;
        startFlow();
    }
}

void UecSrc::setFlowsize(uint64_t flow_size_in_bytes) {
    _flow_size = flow_size_in_bytes;
}

void UecSrc::startFlow() {
    _flow_start_time = eventlist().now();
    //_cwnd = _maxwnd;
    _credit = _maxwnd;


    if (CONNECTION_INFO_MAP.find(_src_dst) == CONNECTION_INFO_MAP.end() || !_connections_mapping) {
        circular_buffer_reps = new CircularBufferREPS<int>(CircularBufferREPS<int>::repsBufferSize);
    } else {
        circular_buffer_reps = CONNECTION_INFO_MAP[_src_dst].buffer;
        CONNECTION_INFO_MAP[_src_dst].buffer = circular_buffer_reps;
        _cwnd = CONNECTION_INFO_MAP[_src_dst].cwnd;
        plb_entropy = CONNECTION_INFO_MAP[_src_dst].plb_entropy;
        flowlet_entropy = CONNECTION_INFO_MAP[_src_dst].flowlet_entropy;
        _ev_skip_bitmap = CONNECTION_INFO_MAP[_src_dst].ev_skip_bitmap;
        working_path_ecmp_mp = CONNECTION_INFO_MAP[_src_dst].working_path_ecmp_mp;
    }
    

    if (_debug_src)
        cout << _flow.str() << " " << "startflow " << _flow._name << " CWND " << _cwnd << " at "
             << timeAsUs(eventlist().now()) << " flow " << _flow.str() << endl;

    cout << "Flow " << _name << " flowId " << flowId() << " " << _nodename << " starting at "
         << timeAsUs(eventlist().now()) << endl;


    /* if (_flow_logger) {
        _flow_logger->logEvent(_flow, *this, FlowEventLogger::START, _flow_size, 0);
    } */
    clearRTO();
    _in_flight = 0;
    _pull_target = INIT_PULL;
    _pull = INIT_PULL;
    _last_rts = 0;
    // backlog is total amount of data we expect to send, including headers
    _backlog = ceil(((double)_flow_size) / _mss) * _hdr_size + _flow_size;
    _rtx_backlog = 0;
    _send_blocked_on_nic = false;
    flowlet_timeout = 0.5 * _base_rtt;
    while (_send_blocked_on_nic == false && credit() > 0 && _backlog > 0) {
        if (_debug_src) {
            cout << _flow.str() << " " << "requestSending 0 "<< endl;
        }

        const Route *route = _nic.requestSending(*this);
        if (_flow.flow_id() == _debug_flowid){
            cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " requestSending " << _nic.activeSources()
                << endl;
        }
        if (route) {
            // if we're here, there's no NIC queue
            mem_b sent_bytes = sendNewPacket(*route);
            if (sent_bytes > 0) {
                _nic.startSending(*this, sent_bytes, route);
            } else {
                _nic.cantSend(*this);
            }
        } else {
            _send_blocked_on_nic = true;
            return;
        }
    }
}

mem_b UecSrc::credit() const {
    return _credit;
}

void UecSrc::spendCredit(mem_b pktsize) {
    if (_receiver_based_cc){
        assert(_credit > 0);
        _credit -= pktsize;
    }
}

void UecSrc::stopSpeculating() {
    // this doesn't really do a lot, except prevent us retransmitting
    // on an RTO before we've heard back from the receiver
    if (_speculating) {
        _speculating = false;
            if (_credit > 0)
            _credit = 0;

        if (_flow.flow_id() == _debug_flowid){
            cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " stopSpeculating _credit " << _credit << endl;
        }
    }
}

UecBasePacket::pull_quanta UecSrc::computePullTarget() {
    if (!_receiver_based_cc)
        return 0;

    mem_b pull_target = _backlog + _rtx_backlog;
    //mem_b pull_target = _backlog;

    if (_sender_based_cc) {
        if (pull_target > _cwnd + _mtu) {
            pull_target = _cwnd + _mtu;
        }
    }

    if (pull_target > _maxwnd) {
        pull_target = _maxwnd;
    }

    pull_target -= _credit;

    if (_speculating && pull_target < _mtu && _backlog >0)//always request at least an MTU of credit if we have a backlog, regardless of how much credit we have already have. Saves our bacon for short transfers 
        pull_target = _mtu;
        
    if(_flow.flow_id() == _debug_flowid){
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " _credit " << _credit 
            << " pull_target " << _pull_target << endl;
    }

    if (_nic.activeSources()>1)
        pull_target /= _nic.activeSources();

    pull_target += UecBasePacket::unquantize(_pull);

    UecBasePacket::pull_quanta quant_pull_target = UecBasePacket::quantize_ceil(pull_target);

    if (_debug_src) {
        cout << timeAsUs(eventlist().now()) << " " << _flow.str() << " " << " " << nodename()
             << " pull_target: " << UecBasePacket::unquantize(quant_pull_target) << " beforequant "
             << pull_target << " pull " << UecBasePacket::unquantize(_pull) << " diff "
             << UecBasePacket::unquantize(quant_pull_target - _pull) << " credit "
             << _credit
             << " backlog " << _backlog << " rtx_backlog " << _rtx_backlog << " active sources "
             << _nic.activeSources() << " cwnd " << _cwnd << " maxwnd " << _maxwnd
             << endl;
    }
    return quant_pull_target;
}

void UecSrc::sendIfPermitted() {
    // send if the NIC, credit and window allow.           

    if (_receiver_based_cc && credit() <= 0) {
        // can send if we have *any* credit, but we don't                                                                                                         
        return;
    }

    //cout << timeAsUs(eventlist().now()) << " " << nodename() << " FOO " << _cwnd << " " << _in_flight << endl;                                                  

    if (_sender_based_cc) {
        if ((_cwnd <= _in_flight && !_loss_recovery_mode) || (_loss_recovery_mode && _rtx_queue.empty())) {
            return;
        }
    }

    if (_rtx_queue.empty()) {
        if (_backlog == 0) {
            return;
        }
    }
    if (_flow.flow_id() == _debug_flowid)
    {
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() <<" sendIfPermitted requestSending _send_blocked_on_nic "<< _send_blocked_on_nic
            << " activesenders " << _nic.activeSources() << endl;
    }
    if (_send_blocked_on_nic) {
        // the NIC already knows we want to send       
        if (_flow.flow_id() == _debug_flowid){
            for(auto it = _nic._active_srcs.begin(); it != _nic._active_srcs.end(); ++it) {
                UecSrc* queued_src = *it; 
                cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() <<" sendIfPermitted block" << queued_src->flow()->flow_id() << " _nic " << _nic._src_id << endl;;
            } 
        }
        return;
    }

    // we can send if the NIC lets us.                                                                                                                            
    if (_debug_src)
        cout << _flow.str() << " " << "requestSending 1\n";
    if (_flow.flow_id() == _debug_flowid)
    {
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() <<" sendIfPermitted requestSending " << endl;
    }
    const Route* route = _nic.requestSending(*this);
    if (route) {
        mem_b sent_bytes = sendPacket(*route);
        if (sent_bytes > 0) {
            _nic.startSending(*this, sent_bytes, route);
            sendIfPermitted();
        } else {
            _nic.cantSend(*this);
        }
    } else {
        // we can't send yet, but NIC will call us back when we can                                                                                               
        _send_blocked_on_nic = true;
        return;
    }
}


// if sendPacket got called, we have already asked the NIC for
// permission, and we've already got both credit and cwnd to send, so
// we will likely be sending something (sendNewPacket can return 0 if
// we only had speculative credit we're not allowed to use though)
mem_b UecSrc::sendPacket(const Route& route) {
    if (_rtx_queue.empty()) {
        return sendNewPacket(route);
    } else {
        return sendRtxPacket(route);
    }
}

void UecSrc::startRTO(simtime_picosec send_time) {
    if (!_rtx_timeout_pending) {
        // timer is not running - start it
        _rtx_timeout_pending = true;
        _rtx_timeout = send_time + _rto;
        _rto_send_time = send_time;

        if (_rtx_timeout < eventlist().now())
            _rtx_timeout = eventlist().now();

        if (_debug_src)
            cout << "Start timer at " << timeAsUs(eventlist().now()) << " source " << _flow.str()
                 << " expires at " << timeAsUs(_rtx_timeout) << " flow " << _flow.str() << endl;

        _rto_timer_handle = eventlist().sourceIsPendingGetHandle(*this, _rtx_timeout);
        if (_rto_timer_handle == eventlist().nullHandle()) {
            // this happens when _rtx_timeout is past the configured simulation end time.
            _rtx_timeout_pending = false;
            if (_debug_src)
                cout << "Cancel timer because too late for flow " << _flow.str() << endl;
        }
    } else {
        // timer is already running
        if (send_time + _rto < _rtx_timeout) {
            // RTO needs to expire earlier than it is currently set
            cancelRTO();
            startRTO(send_time);
        }
    }
}

void UecSrc::clearRTO() {
    // clear the state
    _rto_timer_handle = eventlist().nullHandle();
    _rtx_timeout_pending = false;

    if (_debug_src)
        cout << "Clear RTO " << timeAsUs(eventlist().now()) << " source " << _flow.str() << endl;
}

void UecSrc::cancelRTO() {
    if (_rtx_timeout_pending) {
        // cancel the timer
        eventlist().cancelPendingSourceByHandle(*this, _rto_timer_handle);
        clearRTO();
    }
}

void UecSrc::processEv_mixed(uint16_t path_id, PathFeedback reason){
    processEv_bitmap(path_id,reason);
    processEv_REPS(path_id,reason);
}

void UecSrc::processEv_bitmap(uint16_t path_id, PathFeedback reason){
    // _no_of_paths must be a power of 2
    uint16_t mask = _no_of_paths - 1;
    path_id &= mask;  // only take the relevant bits for an index

    if (!_ev_skip_bitmap[path_id])
        _ev_skip_count++;

    uint8_t penalty = 0;

    if (reason == PathFeedback::PATH_ECN)
        penalty = 1;
    else if (reason == PathFeedback::PATH_NACK)
        penalty = 4;
    else if (reason == PathFeedback::PATH_TIMEOUT)
        penalty = _max_penalty;

    _ev_skip_bitmap[path_id] += penalty;
    if (_ev_skip_bitmap[path_id] > _max_penalty) {
        _ev_skip_bitmap[path_id] = _max_penalty;
    }
}

void UecSrc::processEv_REPS(uint16_t path_id, PathFeedback feedback) {
    if (feedback == PATH_GOOD){
        _next_pathid.push_back(path_id);
        if (_debug){
            cout << timeAsUs(eventlist().now()) << " " << _flow.str() << " REPS Add " << path_id << " " << _next_pathid.size() << endl;
        }
    }
}

void UecSrc::processEv_oblivious(uint16_t path_id, PathFeedback feedback) {
    return;
}

void UecSrc::processEv_incremental(uint16_t path_id, PathFeedback feedback) {
    return;
}


uint16_t UecSrc::nextEntropy_bitmap() {
    // _no_of_paths must be a power of 2
    uint16_t mask = _no_of_paths - 1;
    uint16_t entropy = (_current_ev_index ^ _path_xor) & mask;
    bool flag = false;
    int counter = 0;
    while (_ev_skip_bitmap[entropy] > 0 ) {
        if (flag == false){
            _ev_skip_bitmap[entropy]--;
            if (!_ev_skip_bitmap[entropy]){
                //assert(_ev_skip_count>0);
                _ev_skip_count--;
            }
        }

        flag = true;
        counter ++;
        if (counter > _no_of_paths){
            break;
        }
        _current_ev_index++;
        if (_current_ev_index == _no_of_paths) {
            _current_ev_index = 0;
            _path_xor = rand() & mask;
        }
        entropy = (_current_ev_index ^ _path_xor) & mask;
    }

    // set things for next time
    _current_ev_index++;
    if (_current_ev_index == _no_of_paths) {
        _current_ev_index = 0;
        _path_xor = rand() & mask;
    }

    entropy |= _path_random ^ (_path_random & mask);  // set upper bits
    return entropy;
}

uint16_t UecSrc::nextEntropy_REPS(){
	uint64_t allpathssizes = _mss * _no_of_paths;
    if (_mss * _highest_sent < min((uint64_t)_cwnd, allpathssizes)) {
        _crt_path++;
        if (_crt_path == _no_of_paths) {
            _crt_path = 0;
        }

        if (_debug) 
            cout << timeAsUs(eventlist().now()) << " " << _flow.str() << " REPS FirstWindow " << _crt_path << " highest sent " << _mss * _highest_sent << " maxwnd " << _maxwnd << " allpaths " << allpathssizes << endl;

    } else {
        if (_next_pathid.empty()) {
            assert(_no_of_paths > 0);
		    _crt_path = random() % _no_of_paths;

            if (_debug) 
                cout << timeAsUs(eventlist().now()) << " " << _flow.str() << " REPS Steady " << _crt_path << endl;

        } else {
            _crt_path = _next_pathid.front();
            _next_pathid.pop_front();

            if (_debug) 
                cout << timeAsUs(eventlist().now()) << " " << _flow.str() << " REPS Recycle " << _crt_path << " " << _next_pathid.size() << endl;

        }
    }
    return _crt_path;
}

uint16_t UecSrc::nextEntropy_mixed(){
    if (_next_pathid.empty()) {
        return nextEntropy_bitmap();
    }
    else {
        _crt_path = _next_pathid.front();
        _next_pathid.pop_front();

        if (_debug) 
            cout << timeAsUs(eventlist().now()) << " " << _flow.str() << " MIXED Recycle " << _crt_path << " " << _next_pathid.size() << endl;
    }
    return _crt_path;
}

uint16_t UecSrc::nextEntropy_oblivious(){
    return rand() % _no_of_paths;
}


uint16_t UecSrc::nextEntropy_incremental(){
    return _crt_path++ % _no_of_paths;
}

uint16_t UecSrc::nextEntropy_mp(){
    uint16_t entropy;
    entropy = next_mp_to_use;
    next_mp_to_use = (next_mp_to_use + 1) % _num_mp_flows;
    return (entropy + working_path_ecmp_mp) % _no_of_paths;
}

void UecSrc::processEv_mp(uint16_t path_id, PathFeedback feedback) {
    return;
}


uint16_t UecSrc::nextEntropy_ecmp(){
    return working_path_ecmp_mp;
}

void UecSrc::processEv_ecmp(uint16_t path_id, PathFeedback feedback) {
    return;
}


uint16_t UecSrc::nextEntropy_freezing(){
    if (circular_buffer_reps->explore_counter > 0) {
        circular_buffer_reps->explore_counter--;
        return rand() % _no_of_paths;
    }

    if (circular_buffer_reps->isFrozenMode()) {
        if (circular_buffer_reps->isEmpty()) {
            return rand() % _no_of_paths;
        } else {
            return circular_buffer_reps->remove_frozen();
        }
    } else {
        if (circular_buffer_reps->isEmpty() || circular_buffer_reps->getNumberFreshEntropies() == 0) {
            return _crt_path = rand() % _no_of_paths;
        } else {
            return circular_buffer_reps->remove_earliest_fresh();
        }
    }
}

void UecSrc::processEv_freezing(uint16_t path_id, PathFeedback feedback) {

    if (circular_buffer_reps->isFrozenMode() && eventlist().now() > circular_buffer_reps->can_exit_frozen_mode) {
        circular_buffer_reps->setFrozenMode(false);
        circular_buffer_reps->resetBuffer();
        circular_buffer_reps->explore_counter = _bdp / _mtu;
        //circular_buffer_reps->explore_counter = 8;
        printf("%s exited freezing mode at %lu\n", _name.c_str(), eventlist().now() / 1000); 
    }

    if ((feedback == PATH_GOOD) && !circular_buffer_reps->isFrozenMode()) {
        circular_buffer_reps->add(path_id);
    } else if (circular_buffer_reps->isFrozenMode() && (feedback == PATH_GOOD)) {
        circular_buffer_reps->add(path_id);
    }
}

uint16_t UecSrc::nextEntropy_plb(){
    int plb_n = 1;
    int plb_m = 1;

    if (plb_congested_rounds > plb_n) {
        plb_congested_rounds = 0;
        plb_entropy = rand() % _no_of_paths;
        printf("%s PLB reroute - New entropy is %d\n", _nodename.c_str(), plb_entropy);
    }
    return plb_entropy;
}

void UecSrc::processEv_plb(uint16_t path_id, PathFeedback feedback) {
    plb_delivered += 4096;
    if (feedback != PATH_GOOD) {
        plb_congested += 4096;
    }

    plb_k = 0.25;

    if (eventlist().now() > plb_last_rtt) {
        if (plb_congested > plb_k * plb_delivered) {
            plb_congested_rounds++;
        } else {
            plb_congested_rounds = 0;
        }  
        plb_delivered = 0;
        plb_congested = 0;
        plb_last_rtt = eventlist().now() + _base_rtt;
    }
      
}


uint16_t UecSrc::nextEntropy_flowlet(){
    
    //printf("%s - Time is %lu - Lat pkt %lu - Timeout %lu\n", _nodename.c_str(), eventlist().now() / 1000, last_pkt_flowlet / 1000, flowlet_timeout_wait / 1000);
    if (eventlist().now() > last_pkt_flowlet + flowlet_timeout && eventlist().now() > flowlet_timeout_wait) {
        flowlet_entropy = rand() % _no_of_paths;
        flowlet_timeout_wait = eventlist().now() + _rto * 1;
        printf("%s Flowlet reroute - New entropy is %d (\n", _nodename.c_str(), flowlet_entropy);
    }

    return flowlet_entropy;
}

void UecSrc::processEv_flowlet(uint16_t path_id, PathFeedback feedback) {
    last_pkt_flowlet = eventlist().now();      
}

uint16_t UecSrc::nextEntropy_mprdma(){
    
    if (mprdma_entropy == -1) { // Startup
        return rand() % _no_of_paths;
    } else {
        if (mprdma_can_send) {
            return mprdma_entropy;
        } else {
            //printf("%s MPRDMA BUG\n", nodename().c_str());
            return rand() % _no_of_paths;
        }
    }     
}

void UecSrc::processEv_mprdma(uint16_t path_id, PathFeedback feedback) {
    //printf("MPRDMA %s Received feedback %d\n", nodename().c_str(), feedback);
    if (mprdma_can_send) {
        mprdma_entropy = path_id;
    }
}

mem_b UecSrc::sendNewPacket(const Route& route) {
    if (_debug_src)
        cout << timeAsUs(eventlist().now()) << " " << _flow.str() << " " << _nodename
             << " sendNewPacket highest_sent " << _highest_sent << " h*m "
             << _highest_sent * _mss << " backlog " << _backlog << " flow "
             << _flow.str() << endl;
    assert(_backlog > 0);
    assert(((mem_b)_highest_sent - _rts_packets_sent) * _mss < _flow_size);
    mem_b full_pkt_size = _mtu;
    if (_backlog < _mtu) {
        full_pkt_size = _backlog;
    }

    // check we're allowed to send according to state machine
    if (_receiver_based_cc)
        assert(credit() > 0);
        
    spendCredit(full_pkt_size);

    _backlog -= full_pkt_size;
    assert(_backlog >= 0);
    _in_flight += full_pkt_size;
    auto ptype = UecDataPacket::DATA_PULL;
    if (_speculating) {
        ptype = UecDataPacket::DATA_SPEC;
    }
    _pull_target = computePullTarget();

    auto* p = UecDataPacket::newpkt(_flow, route, _highest_sent, full_pkt_size, ptype,
                                     _pull_target, _dstaddr);
    p->_src_dest = _src_dest_id_hash;
    //printf("%s MPRDMA SEND1 - can send %d - CWND %d - In Flight %d\n",nodename().c_str(), mprdma_can_send, _cwnd, _in_flight);
    uint16_t ev = (this->*nextEntropy)();
    p->set_pathid(ev);
    if (_mixed_lb_traffic && _node_num % 10 == 0) {
        p->no_bg = false;
    }

    hashmap_entropy[p->epsn()] = ev;

    // TODO: This is a hack to make sure we don't run out of credit
    /* _maxwnd = 626650000;
    _cwnd = _maxwnd; */

    p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATESEND);

    if (_backlog == 0 || (_receiver_based_cc && _credit < 0) || ( _sender_based_cc &&  _in_flight >= _cwnd )) 
        p->set_ar(true);
    
    createSendRecord(_highest_sent, full_pkt_size);
    if (_debug_src)
        cout << timeAsUs(eventlist().now()) << " " << _flow.str() << " sending pkt " << _highest_sent
             << " size " << full_pkt_size << " pull target " << _pull_target << " ack request " << p->ar()
             << " cwnd " << _cwnd << " ev " << ev << " in_flight " << _in_flight << endl;
    if (_flow.flow_id() == _debug_flowid)
    {
        //std::uint8_t skip_weight = _ev_skip_bitmap[ev];
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() <<" sending pkt " << _highest_sent
             << " size " << full_pkt_size << " cwnd " << _cwnd << " ev " << ev << " skip_weight " << static_cast<int>(1)
             << " in_flight " << _in_flight << " pull_target " << _pull_target << " pull " << _pull << endl;
    }
    p->sendOn();
    _highest_sent++;
    _new_packets_sent++;
    startRTO(eventlist().now());

    if (!sent_one) {
        sent_one = true;
        //printf("%s Sent first packet at %lu - CWND %lu - Base RTT %lu - BDP %lu\n", _nodename.c_str(), eventlist().now() / 1000, _cwnd, _base_rtt / 1000, _bdp);
    }

    return full_pkt_size;
}

mem_b UecSrc::sendRtxPacket(const Route& route) {
    assert(!_rtx_queue.empty());
    auto seq_no = _rtx_queue.begin()->first;
    mem_b full_pkt_size = _rtx_queue.begin()->second;
    spendCredit(full_pkt_size);

    _rtx_queue.erase(_rtx_queue.begin());
    _rtx_backlog -= full_pkt_size;
    assert(_rtx_backlog >= 0);
    _in_flight += full_pkt_size;
    _pull_target = computePullTarget();
    
    auto* p = UecDataPacket::newpkt(_flow, route, seq_no, full_pkt_size, UecDataPacket::DATA_RTX,
                                     _pull_target, _dstaddr);
    //printf("MPRDMA SEND2 - can send %d\n", mprdma_can_send);
    uint16_t ev = (this->*nextEntropy)();
    p->_src_dest = _src_dest_id_hash;
    p->set_pathid(ev);
    hashmap_entropy[p->epsn()] = ev;
    p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATESEND);

    if (_mixed_lb_traffic && _node_num % 10 == 0) {
        p->no_bg = false;
    }

    createSendRecord(seq_no, full_pkt_size);

    if (_debug_src)
        cout << timeAsUs(eventlist().now()) << " " << _flow.str() << " " << _nodename << " sending rtx pkt " << seq_no
             << " size " << full_pkt_size << " cwnd " << _cwnd
             << " in_flight " << _in_flight << " pull_target " << _pull_target << " pull " << _pull << endl;
    if (_flow.flow_id() == _debug_flowid)
    {
        uint8_t skip_weight =  _ev_skip_bitmap[ev];
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() <<" sending rtx pkt " << seq_no
             << " size " << full_pkt_size << " cwnd " << _cwnd <<" ev " << ev << " skip_weight " << static_cast<int>(skip_weight)
             << " in_flight " << _in_flight << " pull_target " << _pull_target << " pull " << _pull << endl;
    }
    p->set_ar(true);
    p->sendOn();
    _rtx_packets_sent++;
    startRTO(eventlist().now());
    return full_pkt_size;
}

void UecSrc::sendRTS() {
    if (_last_rts > 0 && eventlist().now() - _last_rts < _rtt) {
        // Don't send more than one RTS per RTT, or we can create an
        // incast of RTS.  Once per RTT is enough to restart things if we lost
        // a whole window.
        return;
    }
    if (_debug_src)
        cout << timeAsUs(eventlist().now()) << " " << _flow.str() << " " << _nodename << " sendRTS, flow " << _flow.str()
             << " epsn " << _highest_sent << " last RTS " << timeAsUs(_last_rts)
             << " in_flight " << _in_flight << " pull_target " << _pull_target << " pull " << _pull << endl;
    createSendRecord(_highest_sent, _hdr_size);
    auto* p =
        UecRtsPacket::newpkt(_flow, NULL, _highest_sent, _pull_target, _dstaddr);
    p->_src_dest = _src_dest_id_hash;
    p->set_dst(_dstaddr);
    uint16_t ev = (this->*nextEntropy)();
    p->set_pathid(ev);

    // p->sendOn();
    _nic.sendControlPacket(p, this, NULL);

    _highest_sent++;
    _rts_packets_sent++;
    _last_rts = eventlist().now();
    startRTO(eventlist().now());
}

void UecSrc::createSendRecord(UecBasePacket::seq_t seqno, mem_b full_pkt_size) {
    // assert(full_pkt_size > 64);
    if (_debug_src)
        cout << _flow.str() << " " << _nodename << " createSendRecord seqno: " << seqno << " size " << full_pkt_size
             << endl;
    assert(_tx_bitmap.find(seqno) == _tx_bitmap.end());
    _tx_bitmap.emplace(seqno, sendRecord(full_pkt_size, eventlist().now()));
    _send_times.emplace(eventlist().now(), seqno);
}

void UecSrc::queueForRtx(UecBasePacket::seq_t seqno, mem_b pkt_size) {
    assert(_rtx_queue.find(seqno) == _rtx_queue.end());
    _rtx_queue.emplace(seqno, pkt_size);
    _rtx_backlog += pkt_size;
    if (!_speculating || !_receiver_based_cc)
        sendIfPermitted();
}

void UecSrc::timeToSend(const Route& route) {
    if (_debug_src)
        cout << "timeToSend"
             << " flow " << _flow.str() << " at " << timeAsUs(eventlist().now()) << endl;

    if (_backlog == 0 && _rtx_queue.empty()) {
        _nic.cantSend(*this);
        return;
    }

    // time_to_send is called back from the UecNIC when it's time for
    // this src to send.  To get called back, the src must have
    // previously told the NIC it is ready to send by calling
    // UecNIC::requestSending()
    //
    // before returning, UecSrc needs to call either
    // UecNIC::startSending or UecNIC::cantSend from this function
    // to update the NIC as to what happened, so they stay in sync.
    _send_blocked_on_nic = false;

    mem_b full_pkt_size = _mtu;
    // how much do we want to send?
    if (_rtx_queue.empty()) {
        assert(_rtx_backlog == 0);
        // we want to send new data
        if (_backlog < _mtu) {
            full_pkt_size = _backlog;
        }
    } else {
        // we want to retransmit
        full_pkt_size = _rtx_queue.begin()->second;
    }

    if (_sender_based_cc) {
        if ((_cwnd <= _in_flight && !_loss_recovery_mode) || (_loss_recovery_mode && _rtx_queue.empty())) {
            if (_debug_src)
                cout << _flow.str() << " " << _node_num << "cantSend, limited by sender CWND " << _cwnd << " _in_flight "
                     << _in_flight << "\n";

            _nic.cantSend(*this);
            return;
        }
    }
    if (_flow.flow_id() == _debug_flowid ){
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " _receiver_based_cc " << _receiver_based_cc << " credit " << credit()
            << endl;
    }
    // do we have enough credit if we're using receiver CC?
    if (_receiver_based_cc && credit() <= 0) {
        if (_debug_src)
            cout << "cantSend"
                 << " flow " << _flow.str() << endl;
        ;
        _nic.cantSend(*this);
        return;
    }

    // OK, we're probably good to send
    mem_b bytes_sent = 0;
    if (_rtx_queue.empty()) {
        bytes_sent = sendNewPacket(route);
    } else {
        bytes_sent = sendRtxPacket(route);
    }

    // let the NIC know we sent, so it can calculate next send time.
    if (bytes_sent > 0) {
        _nic.startSending(*this, full_pkt_size, NULL);
    } else {
        _nic.cantSend(*this);
        return;
    }
    
    if (_sender_based_cc) {
        if ((_cwnd <= _in_flight && !_loss_recovery_mode) || (_loss_recovery_mode && _rtx_queue.empty())) {
            return;
        }
    }

    // do we have enough credit to send again?
    if (_receiver_based_cc && credit() <= 0) {
        return;
    }

    if (_backlog == 0 && _rtx_queue.empty()) {
        // we're done - nothing more to send.
        return;
    }

    // we're ready to send again.  Let the NIC know.
    assert(!_send_blocked_on_nic);
    if (_debug_src)
        cout << "requestSending2"
             << " flow " << _flow.str() << endl;
    ;
    const Route* newroute = _nic.requestSending(*this);
    // we've just sent - NIC will say no, but will call us back when we can send.
    assert(!newroute);
    _send_blocked_on_nic = true;
}

void UecSrc::recalculateRTO() {
    // we're no longer waiting for the packet we set the timer for -
    // figure out what the timer should be now.
    cancelRTO();
    if (_send_times.empty()) {
        // nothing left that we're waiting for
        return;
    }
    auto earliest_send_time = _send_times.begin()->first;
    startRTO(earliest_send_time);
}

void UecSrc::rtxTimerExpired() {
    assert(eventlist().now() == _rtx_timeout);
    clearRTO();

    auto first_entry = _send_times.begin();
    assert(first_entry != _send_times.end());
    auto seqno = first_entry->second;

    auto send_record = _tx_bitmap.find(seqno);
    assert(send_record != _tx_bitmap.end());
    mem_b pkt_size = send_record->second.pkt_size;

    // update flightsize?

    //_send_times.erase(first_entry);
    delFromSendTimes(send_record->second.send_time,seqno);

    if (_flow.flow_id() == _debug_flowid)
        cout << _nodename << " rtx timer expired for seqno " << seqno << " flow " << _flow.str() << " packet sent at " << timeAsUs(send_record->second.send_time) << " now time is " << timeAsUs(eventlist().now()) << endl;

    _tx_bitmap.erase(send_record);
    recalculateRTO();

    if ((_load_balancing_algo == FREEZING) && !circular_buffer_reps->isFrozenMode() && circular_buffer_reps->explore_counter == 0) {
        if (_trim_disbled && _last_rto_max_rtt < _base_rtt * 1.65) {
            circular_buffer_reps->setFrozenMode(true);
            circular_buffer_reps->can_exit_frozen_mode = eventlist().now() +  circular_buffer_reps->exit_freeze_after;
            printf("%s started freezing mode1 at %lu (can exit at %lu) - %d - Size Buffer %d\n", _name.c_str(), eventlist().now() / 1000, circular_buffer_reps->can_exit_frozen_mode / 1000, circular_buffer_reps->isFrozenMode(), circular_buffer_reps->getSize()); 
            printf("Last Max RTT %lu - RTO Start %lu - Time is %lu - Base RTT is %lu -- Max %lu at %lu\n", _last_rto_max_rtt/1000, _last_rto_start/1000, eventlist().now()/1000, _base_rtt/1000, _max_rtt_seen/1000, _when__max_rtt_seen/1000);
        } else if (!_trim_disbled) {
            circular_buffer_reps->setFrozenMode(true);
            circular_buffer_reps->can_exit_frozen_mode = eventlist().now() +  circular_buffer_reps->exit_freeze_after;
            printf("%s started freezing mode2 at %lu (can exit at %lu) - Explore Counter %d - %d - Size Buffer %d\n", _name.c_str(), eventlist().now() / 1000, circular_buffer_reps->can_exit_frozen_mode / 1000, circular_buffer_reps->explore_counter, circular_buffer_reps->isFrozenMode(), circular_buffer_reps->getSize()); 
        }   
    }


    

    if (_load_balancing_algo == PLB && eventlist().now() > plb_timeout_wait) {
        plb_timeout_wait = eventlist().now() + _rto * 1;
        plb_congested_rounds = 0;
        plb_entropy = rand() % _no_of_paths;
        printf("%s PLB reroute - New entropy is %d\n", _nodename.c_str(), plb_entropy);
    }

    if (_load_balancing_algo == BITMAP || _load_balancing_algo == MIXED) {
        _ev_bitmap_failed.push_back(hashmap_entropy[seqno]);
        (this->*processEv)(hashmap_entropy[seqno], PATH_TIMEOUT);
    }


    /* if ((_load_balancing_algo == ECMP || _load_balancing_algo == MP) && eventlist().now() > last_received_ack_timestamp) {
        last_received_ack_timestamp = eventlist().now() + (_rto * 6);
        working_path_ecmp_mp = rand() % _no_of_paths;
        printf("%s ECMP reroute - New entropy is %d\n", _nodename.c_str(), working_path_ecmp_mp);
    } */

    /* if (_load_balancing_algo == FLOWLET) {
        flowlet_timeout_wait = eventlist().now() + _rto * 1;
    } */

    if (_sender_based_cc)
        mark_packet_for_retransmission(seqno, pkt_size);

    if (!_rtx_queue.empty()) {
        // there's already a queue, so clearly we shouldn't just
        // resend right now.  But send an RTS (no more than once per
        // RTT) to cover the case where the receiver doesn't know
        // we're waiting.
        stopSpeculating();  

        queueForRtx(seqno, pkt_size);

        if (_receiver_based_cc) {
            if (_debug_src)
                cout << "sendRTS 1"
                     << " flow " << _flow.str() << endl;
            sendRTS();
        }
        return;
    }

    // there's no queue, so maybe we could just resend now?
    queueForRtx(seqno, pkt_size);

    if (_sender_based_cc) {
        if (_cwnd < pkt_size + _in_flight) {
            // window won't allow us to send yet.
            if (_debug_src)
                cout << "sendRTS 3"
                     << " flow " << _flow.str() << endl;
            sendRTS();  
            return;
        }
    }

    if (_receiver_based_cc && _credit <= 0) {
        // we don't have any credit to send.  Send an RTS (no more than once per RTT)
        // to cover the case where the receiver doesn't know to send
        // us credit
        if (_debug_src)
            cout << "sendRTS 2"
                 << " flow " << _flow.str() << endl;

        sendRTS();
        return;
    }

    // we've got enough pulled credit or window already to send this, so see if the NIC
    // is ready right now
    if (_debug_src)
        cout << "requestSending 4\n"
             << " flow " << _flow.str() << endl;

    const Route* route = _nic.requestSending(*this);
    if (route) {
        bool bytes_sent = sendRtxPacket(*route);
        if (bytes_sent > 0) {
            _nic.startSending(*this, bytes_sent, route);
        } else {
            _nic.cantSend(*this);
            return;
        }
    }
}

void UecSrc::activate() {
    startFlow();
}

void UecSrc::setEndTrigger(Trigger& end_trigger) {
    _end_trigger = &end_trigger;
};

////////////////////////////////////////////////////////////////
//  UEC SINK PORT
////////////////////////////////////////////////////////////////
UecSinkPort::UecSinkPort(UecSink& sink, uint32_t port_num)
    : _sink(sink), _port_num(port_num) {
}

void UecSinkPort::setRoute(const Route& route) {
    _route = &route;
}

void UecSinkPort::receivePacket(Packet& pkt) {
    _sink.receivePacket(pkt, _port_num);
}

const string& UecSinkPort::nodename() {
    return _sink.nodename();
}

////////////////////////////////////////////////////////////////
//  UEC SINK
////////////////////////////////////////////////////////////////

UecSink::UecSink(TrafficLogger* trafficLogger, UecPullPacer* pullPacer, UecNIC& nic, uint32_t no_of_ports)
    : DataReceiver("uecSink"),
      _nic(nic),
      _flow(trafficLogger),
      _pullPacer(pullPacer),
      _expected_epsn(0),
      _high_epsn(0),
      _retx_backlog(0),
      _latest_pull(INIT_PULL),
      _highest_pull_target(INIT_PULL),
      _received_bytes(0),
      _accepted_bytes(0),
      _recvd_bytes(0),
      _rcv_cwnd_pen(255),
      _end_trigger(NULL),
      _epsn_rx_bitmap(0),
      _out_of_order_count(0),
      _ack_request(false),
      _entropy(0)  {
    
    _nodename = "uecSink";  // TBD: would be nice at add nodenum to nodename
    _no_of_ports = no_of_ports;
    _ports.resize(no_of_ports);
    for (uint32_t p = 0; p < _no_of_ports; p++) {
        _ports[p] = new UecSinkPort(*this, p);
    }
        
    _stats = {0, 0, 0, 0, 0,0,0};
    _in_pull = false;
    _in_slow_pull = false;

    _pcie = NULL;
    _receiver_cc = NULL;
}

UecSink::UecSink(TrafficLogger* trafficLogger,
                   linkspeed_bps linkSpeed,
                   double rate_modifier,
                   uint16_t mtu,
                   EventList& eventList,
                   UecNIC& nic,
                   uint32_t no_of_ports)
    : DataReceiver("uecSink"),
      _nic(nic),
      _flow(trafficLogger),
      _expected_epsn(0),
      _high_epsn(0),
      _retx_backlog(0),
      _latest_pull(INIT_PULL),
      _highest_pull_target(INIT_PULL),
      _received_bytes(0),
      _accepted_bytes(0),
      _recvd_bytes(0),
      _rcv_cwnd_pen(255),
      _end_trigger(NULL),
      _epsn_rx_bitmap(0),
      _out_of_order_count(0),
      _ack_request(false),
      _entropy(0) {
    
    if (UecSrc::_receiver_based_cc)
        _pullPacer = new UecPullPacer(linkSpeed, rate_modifier, mtu, eventList, no_of_ports);
    else    
        _pullPacer = NULL;

    _no_of_ports = no_of_ports;
    _ports.resize(no_of_ports);
    for (uint32_t p = 0; p < _no_of_ports; p++) {
        _ports[p] = new UecSinkPort(*this, p);
    }
    _stats = {0, 0, 0, 0, 0,0,0};
    _in_pull = false;
    _in_slow_pull = false;

    _pcie = NULL;
    _receiver_cc = NULL;
}

void UecSink::connectPort(uint32_t port_num, UecSrc& src, const Route& route) {
    _src = &src;
    _ports[port_num]->setRoute(route);
    _route_fail = &route;
}

void UecSink::handlePullTarget(UecBasePacket::seq_t pt) {
    if (!UecSrc::_receiver_based_cc)
        return;

    if (_src->debug())
        cout << " UecSink " << _nodename << " src " << _src->nodename() << " handlePullTarget pt " << pt << " highest_pt " << _highest_pull_target << endl;
    if (_src->flow()->flow_id() == UecSrc::_debug_flowid ){
        cout << timeAsUs(_src->eventlist().now()) << " flowid " << _src->flow()->flow_id()  << " handlePullTarget pt " << pt << " highest_pt " << _highest_pull_target << endl;

    }
    if (pt > _highest_pull_target) {
        if (_src->debug())
            cout << "    pull target advanced\n";
        _highest_pull_target = pt;

        if (_retx_backlog == 0 && !_in_pull) {
            if (_src->debug())
                cout << "    requesting pull\n";
            _in_pull = true;
            _pullPacer->requestPull(this);
        }
    }
}

void UecSink::processData(UecDataPacket& pkt) {
    bool force_ack = false;

    //PCIeModel processing

    if (_model_pcie){
        if (!_pcie->addBacklog(pkt.size())){
            //will drop this packet!
            cout << "PCIE trim" << endl;
            //should trim this packet.
            pkt.strip_payload();
            processTrimmed(pkt);
            return;
        }
    }

    //ensure we never overflow receive bitmap.
    if (pkt.epsn() > _expected_epsn + uecMaxInFlightPkts * UecSrc::_mtu){
        abort();
    }

    if (_src->debug())
        cout << " UecSink " << _nodename << " src " << _src->nodename()
             << " processData: " << pkt.epsn() << " time " << timeAsNs(getSrc()->eventlist().now())
             << " when expected epsn is " << _expected_epsn << " size " << pkt.size() << " ooo count " << _out_of_order_count
             << " flow " << _src->flow()->str() << endl;

    _accepted_bytes += pkt.size();

    handlePullTarget(pkt.pull_target());
    if (_src->flow()->flow_id() == UecSrc::_debug_flowid)
    {
        cout << timeAsUs(_src->eventlist().now()) << " flowid " << _src->flow()->flow_id()
             << " recv " << pkt.epsn() << endl;
    }
    if (pkt.epsn() > _high_epsn) {
        // highest_received is used to bound the sack bitmap. This is a 64 bit number in simulation,
        // never wraps. In practice need to handle sequence number wrapping.
        _high_epsn = pkt.epsn();
    }

    // should send an ACK; if incoming packet is ECN marked, the ACK will be sent straight away;
    // otherwise ack will be delayed until we have cumulated enough bytes / packets.
    bool ecn = (bool)(pkt.flags() & ECN_CE);

    if (ecn){
        _stats.ecn_received++;
        _stats.ecn_bytes_received += pkt.size();

        if (_oversubscribed_cc)
            _receiver_cc->ecn_received(pkt.size());
    }

    if (pkt.epsn() < _expected_epsn || _epsn_rx_bitmap[pkt.epsn()]) {
        if (UecSrc::_debug)
            cout << _nodename << " src " << _src->nodename() << " duplicate psn " << pkt.epsn()
                 << endl;

        _stats.duplicates++;
        _nic.logReceivedData(pkt.size(), 0);

        if (_src->flow()->flow_id() == UecSrc::_debug_flowid){
            cout << timeAsUs(_src->eventlist().now()) << " flowid " << _src->flow()->flow_id()  
                << " Spurious " << pkt.epsn() <<endl;
        }
        // sender is confused and sending us duplicates: ACK straight away.
        // this code is different from the proposed hardware implementation, as it keeps track of
        // the ACK state of OOO packets.
        UecAckPacket* ack_packet =
            sack(pkt.path_id(), ecn ? pkt.epsn() : sackBitmapBase(pkt.epsn()), pkt.epsn(), ecn);
        _nic.sendControlPacket(ack_packet, NULL, this);

        _accepted_bytes = 0;  // careful about this one.
        return;
    }

    if (_received_bytes == 0) {
        force_ack = true;
    }
    // packet is in window, count the bytes we got.
    // should only count for non RTS and non trimmed packets.
    _received_bytes += pkt.size() - UecAckPacket::ACKSIZE;
    _nic.logReceivedData(pkt.size(), pkt.size());

    _recvd_bytes += pkt.size();
    if (_src->debug()) {
        cout << _nodename << " recvd_bytes: " << _recvd_bytes << endl;
    }

    assert(_received_bytes <= _src->flowsize());
    if (_src->debug() && _received_bytes >= _src->flowsize())
        cout << _nodename << " received " << _received_bytes << " at "
             << timeAsUs(EventList::getTheEventList().now()) << endl;

    if (pkt.ar()) {
        // this triggers an immediate ack; also triggers another ack later when the ooo queue drains
        // (_ack_request tracks this state)
        force_ack = true;
        _ack_request = true;
    }

    if (_src->debug())
        cout << _nodename << " src " << _src->nodename()
             << " >>    cumulative ack was: " << _expected_epsn << " flow " << _src->flow()->str()
             << endl;

    if (pkt.epsn() == _expected_epsn) {
        while (_epsn_rx_bitmap[++_expected_epsn]) {
            // clean OOO state, this will wrap at some point.
            _epsn_rx_bitmap[_expected_epsn] = 0;
            _out_of_order_count--;
        }
        if (_src->debug())
            cout << " UecSink " << _nodename << " src " << _src->nodename()
                 << " >>    cumulative ack now: " << _expected_epsn << " ooo count "
                 << _out_of_order_count << " flow " << _src->flow()->str() << endl;

        if (_out_of_order_count == 0 && _ack_request) {
            force_ack = true;
            _ack_request = false;
        }
    } else {
        _epsn_rx_bitmap[pkt.epsn()] = 1;
        _out_of_order_count++;
        _stats.out_of_order++;
    }

    if (ecn || shouldSack() || force_ack) {
        UecAckPacket* ack_packet =
            sack(pkt.path_id(), (ecn || pkt.ar()) ? pkt.epsn() : sackBitmapBase(pkt.epsn()), pkt.epsn(), ecn);

        if (_src->debug()) {
            cout << " UecSink " << _nodename << " src " << _src->nodename()
                 << " sendAckNow: " << _expected_epsn << " ref_epsn " << pkt.epsn()
                 << " ooo_count " << _out_of_order_count
                 << " recvd_bytes " << _recvd_bytes << " flow " << _src->flow()->str() 
                 << " ecn " << ecn << " shouldSack " << shouldSack() << " forceack " << force_ack << endl;
        }

        _accepted_bytes = 0;

        // ack_packet->sendOn();
        _nic.sendControlPacket(ack_packet, NULL, this);
    }
}

void UecSink::processTrimmed(const UecDataPacket& pkt) {
    _nic.logReceivedTrim(pkt.size());

    _stats.trimmed++;
    if (_oversubscribed_cc)
        _receiver_cc->trimmed_received();

    if (pkt.epsn() < _expected_epsn || _epsn_rx_bitmap[pkt.epsn()]) {
        if (_src->debug())
            cout << " UecSink processTrimmed got a packet we already have: " << pkt.epsn()
                 << " time " << timeAsNs(getSrc()->eventlist().now()) << " flow"
                 << _src->flow()->str() << endl;

        UecAckPacket* ack_packet = sack(pkt.path_id(), sackBitmapBase(pkt.epsn()), pkt.epsn(), false);
        //ack_packet->sendOn();
        _nic.sendControlPacket(ack_packet, NULL, this);
        return;
    }

    if (_src->debug())
        cout << " UecSink processTrimmed packet " << pkt.epsn() << " time "
             << timeAsNs(getSrc()->eventlist().now()) << " flow" << _src->flow()->str() << endl;

    handlePullTarget(pkt.pull_target());

    if (_src->debug())
        cout << "RTX_backlog++ trim: " << pkt.epsn() << " from " << getSrc()->nodename()
             << " rtx_backlog " << rtx_backlog() << " at " << timeAsUs(getSrc()->eventlist().now())
             << " flow " << _src->flow()->str() << endl;

    UecNackPacket* nack_packet = nack(pkt.path_id(), pkt.epsn());

    // nack_packet->sendOn();
    _nic.sendControlPacket(nack_packet, NULL, this);

    if (UecSrc::_receiver_based_cc && !_in_pull) {
        // source is now retransmitting, must add it to the list.
        if (_src->debug())
            cout << "PullPacer RequestPull: " << _src->flow()->str() << " at "
                 << timeAsUs(getSrc()->eventlist().now()) << endl;

        _in_pull = true;
        _pullPacer->requestPull(this);
    }
}

void UecSink::processRts(const UecRtsPacket& pkt) {
    assert(pkt.ar());
    if (_src->debug())
        cout << " UecSink " << _nodename << " src " << _src->nodename()
             << " processRts: " << pkt.epsn() << " time " << timeAsNs(getSrc()->eventlist().now()) << endl;

    handlePullTarget(pkt.pull_target());

    // what happens if this is not an actual retransmit, i.e. the host decides with the ACK that it
    // is

    if (_src->debug())
        cout << "RTX_backlog++ RTS: " << _src->flow()->str() << " rtx_backlog " << rtx_backlog()
             << " at " << timeAsUs(getSrc()->eventlist().now()) << endl;

    if (UecSrc::_receiver_based_cc && !_in_pull) {
        _in_pull = true;
        _pullPacer->requestPull(this);
    }

    bool ecn = (bool)(pkt.flags() & ECN_CE);
    assert(!ecn); // not expecting ECN set on control packets

    if (pkt.epsn() < _expected_epsn || _epsn_rx_bitmap[pkt.epsn()]) {
        if (_src->debug())
            cout << _nodename << " src " << _src->nodename() << " duplicate RTS psn " << pkt.epsn()
                 << endl;

        _stats.duplicates++;

        // sender is confused and sending us duplicates: ACK straight away.
        // this code is different from the proposed hardware implementation, as it keeps track of
        // the ACK state of OOO packets.
        UecAckPacket* ack_packet = sack(pkt.path_id(), sackBitmapBase(pkt.epsn()), pkt.epsn(), ecn);
        _nic.sendControlPacket(ack_packet, NULL, this);

        _accepted_bytes = 0;  // careful about this one.
        return;
    }


    if (pkt.epsn() == _expected_epsn) {
        while (_epsn_rx_bitmap[++_expected_epsn]) {
            // clean OOO state, this will wrap at some point.
            _epsn_rx_bitmap[_expected_epsn] = 0;
            _out_of_order_count--;
        }
        if (_src->debug())
            cout << " UecSink " << _nodename << " src " << _src->nodename()
                 << " >>    cumulative ack now: " << _expected_epsn << " ooo count "
                 << _out_of_order_count << " flow " << _src->flow()->str() << endl;

        if (_out_of_order_count == 0 && _ack_request) {
            _ack_request = false;
        }
    } else {
        _epsn_rx_bitmap[pkt.epsn()] = 1;
        _out_of_order_count++;
        _stats.out_of_order++;
    }

    UecAckPacket* ack_packet =
        sack(pkt.path_id(), (ecn || pkt.ar()) ? pkt.epsn() : sackBitmapBase(pkt.epsn()), pkt.epsn(), ecn);

    if (_src->debug())
        cout << " UecSink " << _nodename << " src " << _src->nodename()
             << " send ack now: " << _expected_epsn << " ooo count " << _out_of_order_count
             << " flow " << _src->flow()->str() << endl;

    _nic.sendControlPacket(ack_packet, NULL, this);
}

void UecSink::receivePacket(Packet& pkt, uint32_t port_num) {

    FAILURE_GENERATOR->dropRandomPacketSink(pkt);

    if (FAILURE_GENERATOR->simNICFailures(NULL, this, pkt)) {
        // Return if using a timeout, otherwise transform it in a trim and mark it as failed
        if (_use_timeouts) {
            pkt.free();
            return;
        } else {
            if (!pkt.header_only()) {
                pkt.strip_payload(64);
            }
            pkt.is_failed = true;
        }
    }

    if (FAILURE_GENERATOR->dropPacketsSwitchBER(pkt)) {
        // Return if using a timeout, otherwise transform it in a trim and mark it as failed
        if (_use_timeouts) {
            pkt.free();
            return;
        } else {
            if (!pkt.header_only()) {
                pkt.strip_payload(64);
            }
            pkt.is_failed = true;
        }
    }


    _stats.received++;
    _stats.bytes_received += pkt.size();  // should this include just the payload?

    if (_oversubscribed_cc)
        _receiver_cc->data_received(pkt.size());

    switch (pkt.type()) {
        case UECDATA:
            if (pkt.header_only()){
                processTrimmed((const UecDataPacket&)pkt);
                // cout << "UecSink::receivePacket receive trimmed packet\n";
                // assert(false);
            }else
                processData((UecDataPacket&)pkt);

            pkt.free();
            break;
        case UECRTS:
            processRts((const UecRtsPacket&)pkt);
            pkt.free();
            break;
        default:
            cout << "UecSink::receivePacket receive weird packets\n";
            abort();
    }
}

uint16_t UecSink::nextEntropy() {
    int spraymask = (1 << TGT_EV_SIZE) - 1;
    int fixedmask = ~spraymask;
    int idx = _entropy & spraymask;
    int fixed_entropy = _entropy & fixedmask;
    int ev = ++idx & spraymask;

    _entropy = fixed_entropy | ev;  // save for next pkt

    return ev;
}

UecPullPacket* UecSink::pull() {
    // called when pull pacer is ready to give another credit to this connection.
    // TODO: need to credit in multiple of MTU here.

    if (_retx_backlog > 0) {
        if (_retx_backlog > UecSink::_credit_per_pull)
            _retx_backlog -= UecSink::_credit_per_pull;
        else
            _retx_backlog = 0;

        if (UecSrc::_debug)
            cout << "RTX_backlog--: " << getSrc()->nodename() << " rtx_backlog " << rtx_backlog()
                 << " at " << timeAsUs(getSrc()->eventlist().now()) << " flow "
                 << _src->flow()->str() << endl;
    }

    _latest_pull += UecSink::_credit_per_pull;

    UecPullPacket* pkt = NULL;
    pkt = UecPullPacket::newpkt(_flow, NULL, _latest_pull, false, _srcaddr);
    pkt->_src_dest = _src_dest_id_hash;
    pkt->set_pathid(nextEntropy());

    return pkt;
}

bool UecSink::shouldSack() {
    return _accepted_bytes >= _bytes_unacked_threshold;
}

UecBasePacket::seq_t UecSink::sackBitmapBase(UecBasePacket::seq_t epsn) {
    return max((int64_t)epsn - 63, (int64_t)(_expected_epsn + 1));
}

UecBasePacket::seq_t UecSink::sackBitmapBaseIdeal() {
    uint8_t lowest_value = UINT8_MAX;
    UecBasePacket::seq_t lowest_position = 0;

    // find the lowest non-zero value in the sack bitmap; that is the candidate for the base, since
    // it is the oldest packet that we are yet to sack. on sack bitmap construction that covers a
    // given seqno, the value is incremented.
    for (UecBasePacket::seq_t crt = _expected_epsn; crt <= _high_epsn; crt++)
        if (_epsn_rx_bitmap[crt] && _epsn_rx_bitmap[crt] < lowest_value) {
            lowest_value = _epsn_rx_bitmap[crt];
            lowest_position = crt;
        }

    if (lowest_position + 64 > _high_epsn)
        lowest_position = _high_epsn - 64;

    if (lowest_position <= _expected_epsn)
        lowest_position = _expected_epsn + 1;

    return lowest_position;
}

uint64_t UecSink::buildSackBitmap(UecBasePacket::seq_t ref_epsn) {
    // take the next 64 entries from ref_epsn and create a SACK bitmap with them
    if (_src->debug())
        cout << " UecSink: building sack for ref_epsn " << ref_epsn << endl;
    uint64_t bitmap = (uint64_t)(_epsn_rx_bitmap[ref_epsn] != 0) << 63;

    for (int i = 1; i < 64; i++) {
        bitmap = bitmap >> 1 | (uint64_t)(_epsn_rx_bitmap[ref_epsn + i] != 0) << 63;
        if (_src->debug() && (_epsn_rx_bitmap[ref_epsn + i] != 0))
            cout << "     Sack: " << ref_epsn + i << endl;

        if (_epsn_rx_bitmap[ref_epsn + i]) {
            // remember that we sacked this packet
            if (_epsn_rx_bitmap[ref_epsn + i] < UINT8_MAX)
                _epsn_rx_bitmap[ref_epsn + i]++;
        }
    }
    if (_src->debug())
        cout << "       bitmap is: " << bitmap << endl;
    return bitmap;
}

UecAckPacket* UecSink::sack(uint16_t path_id, UecBasePacket::seq_t seqno, UecBasePacket::seq_t acked_psn, bool ce) {
    uint64_t bitmap = buildSackBitmap(seqno);
    UecAckPacket* pkt =
        UecAckPacket::newpkt(_flow, NULL, _expected_epsn, seqno, acked_psn, path_id, ce, _recvd_bytes,_rcv_cwnd_pen,_srcaddr);
    pkt->set_bitmap(bitmap);
    pkt->_src_dest = _src_dest_id_hash;
    pkt->set_ooo(_out_of_order_count);
    return pkt;
}

UecNackPacket* UecSink::nack(uint16_t path_id, UecBasePacket::seq_t seqno) {
    UecNackPacket* pkt = UecNackPacket::newpkt(_flow, NULL, seqno, path_id,  _recvd_bytes,_rcv_cwnd_pen,_srcaddr);
    pkt->_src_dest = _src_dest_id_hash;
    return pkt;
}

void UecSink::setEndTrigger(Trigger& end_trigger) {
    _end_trigger = &end_trigger;
};


/*static unsigned pktByteTimes(unsigned size) {
    // IPG (96 bit times) + preamble + SFD + ether header + FCS = 38B
    return max(size, 46u) + 38;
}*/

uint32_t UecSink::reorder_buffer_size() {
    uint32_t count = 0;
    // it's not very efficient to count each time, but if we only do
    // this occasionally when the sink logger runs, it should be OK.
    for (uint32_t i = 0; i < uecMaxInFlightPkts; i++) {
        if (_epsn_rx_bitmap[i])
            count++;
    }
    return count;
}

////////////////////////////////////////////////////////////////
//  UEC PACER
////////////////////////////////////////////////////////////////

// pull rate modifier should generally be something like 0.99 so we pull at just less than line rate
UecPullPacer::UecPullPacer(linkspeed_bps linkSpeed,
                             double pull_rate_modifier,
                             uint16_t mtu,
                             EventList& eventList,
                             uint32_t no_of_ports)
    : EventSource(eventList, "uecPull"),
      _pktTime((8 * mtu * 1e12 / (linkSpeed * no_of_ports))/pull_rate_modifier) {
    _active = false;
    _actualPktTime = _pktTime;
    _mtu = mtu;
    _linkspeed = linkSpeed;
    _rates[PCIE] = 1;
    _rates[OVERSUBSCRIBED_CC] = 1;
}

void UecPullPacer::doNextEvent() {
    if (_active_senders.empty() && _idle_senders.empty()) {
        _active = false;
        return;
    }

    UecSink* sink = NULL;
    UecPullPacket* pullPkt;

    if (!_active_senders.empty()) {
        sink = _active_senders.front();

        assert(sink->inPullQueue());

        _active_senders.pop_front();
        pullPkt = sink->pull();

        // TODO if more pulls are needed, enqueue again
        if (UecSrc::_debug)
            cout << "PullPacer: Active: " << sink->getSrc()->flow()->str() << " backlog "
                 << sink->backlog() << " at " << timeAsUs(eventlist().now()) << endl;
        if (sink->backlog() > 0)
            _active_senders.push_back(sink);
        else {  // this sink has had its demand satisfied, move it to idle senders list.
            _idle_senders.push_back(sink);
            sink->removeFromPullQueue();
            sink->addToSlowPullQueue();
        }
    } else {  // no active senders, we must have at least one idle sender
        sink = _idle_senders.front();
        _idle_senders.pop_front();

        if (!sink->inSlowPullQueue())
            sink->addToSlowPullQueue();

        if (UecSrc::_debug)
            cout << "PullPacer: Idle: " << sink->getSrc()->flow()->str() << " at "
                 << timeAsUs(eventlist().now()) << " backlog " << sink->backlog() << " "
                 << sink->slowCredit() << " max "
                 << UecBasePacket::quantize_floor(sink->getMaxCwnd()) << endl;
        pullPkt = sink->pull();
        pullPkt->set_slow_pull(true);

        if (sink->backlog() == 0 &&
            sink->slowCredit() < UecBasePacket::quantize_floor(sink->getMaxCwnd())) {
            // only send upto 1BDP worth of speculative credit.
            // backlog will be negative once this source starts receiving speculative credit.
            _idle_senders.push_back(sink);
        } else
            sink->removeFromSlowPullQueue();
    }

    pullPkt->flow().logTraffic(*pullPkt, *this, TrafficLogger::PKT_SEND);

    // pullPkt->sendOn();
    sink->getNIC()->sendControlPacket(pullPkt, NULL, sink);
    _active = true;

    eventlist().sourceIsPendingRel(*this, _actualPktTime);
}

void UecPullPacer::updatePullRate(reason r, double relative_rate){
    _rates[r] = relative_rate;

    _actualPktTime = _pktTime / min(_rates[PCIE],_rates[OVERSUBSCRIBED_CC]);

    if (UecSrc::_debug)
        cout << "Interpacket delay " << timeAsUs(_actualPktTime) << endl;
}

bool UecPullPacer::isActive(UecSink* sink) {
    for (auto i = _active_senders.begin(); i != _active_senders.end(); i++) {
        if (*i == sink)
            return true;
    }
    return false;
}

bool UecPullPacer::isIdle(UecSink* sink) {
    for (auto i = _idle_senders.begin(); i != _idle_senders.end(); i++) {
        if (*i == sink)
            return true;
    }
    return false;
}

void UecPullPacer::requestPull(UecSink* sink) {
    if (isActive(sink)) {
        abort();
    }
    assert(sink->inPullQueue());

    _active_senders.push_back(sink);
    // TODO ack timer

    if (!_active) {
        eventlist().sourceIsPendingRel(*this, 0);
        _active = true;
    }
}


