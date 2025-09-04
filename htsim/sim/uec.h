// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef UEC_H
#define UEC_H

#include <memory>
#include <tuple>
#include <list>
#include <unordered_map>

#include "buffer_reps.h"
#include "eventlist.h"
#include "trigger.h"
#include "uecpacket.h"
#include "circular_buffer.h"
#include "modular_vector.h"
#include "pciemodel.h"
#include "oversubscribed_cc.h"
#include "metric.h"

#define timeInf 0
// min RTO bound in us
//  *** don't change this default - override it by calling UecSrc::setMinRTO()
#define DEFAULT_UEC_RTO_MIN 100

static const unsigned uecMaxInFlightPkts = 1 << 14;
class UecPullPacer;
class UecSink;
class UecSrc;
class UecLogger;



class ConnectionInfo {
public:
    simtime_picosec stop_time;
    CircularBufferREPS<int> *buffer;
    int cwnd;
    int plb_entropy;
    int flowlet_entropy;
    std::vector<uint16_t> ev_skip_bitmap;
    int working_path_ecmp_mp;

};

// UecNIC aggregates UecSrcs that are on the same NIC.  It round
// robins between active srcs when we're limited by the sending
// linkspeed due to outcast (or just at startup) - this avoids
// building an output queue like the old NDP simulator did, and so
// better models what happens in a h/w NIC.
class UecNIC : public EventSource, public NIC {
    struct PortData {
        simtime_picosec send_end_time;
        bool busy;
        mem_b last_pktsize;
    };
    struct CtrlPacket {
        UecBasePacket* pkt;
        UecSrc* src;
        UecSink* sink;
    };
public:
    UecNIC(id_t src_num, EventList& eventList, linkspeed_bps linkspeed, uint32_t ports);

    // handle traffic sources.
    const Route* requestSending(UecSrc& src);
    void startSending(UecSrc& src, mem_b pkt_size, const Route* rt);
    void cantSend(UecSrc& src);

    // handle control traffic from receivers.
    // only one of src or sink must be set
    void sendControlPacket(UecBasePacket* pkt, UecSrc* src, UecSink* sink);
    uint32_t findFreePort();
    void doNextEvent();

    linkspeed_bps linkspeed() const {return _linkspeed;}

    int activeSources() const { return _active_srcs.size(); }
    virtual const string& nodename() const {return _nodename;}
    list<UecSrc*> _active_srcs;

private:
    void sendControlPktNow();
    uint32_t sendOnFreePortNow(simtime_picosec endtime, const Route* rt);
    list<struct CtrlPacket> _control;
    mem_b _control_size;

    linkspeed_bps _linkspeed;
    int _num_queued_srcs;

    // data related to the NIC ports
    vector<struct PortData> _ports;
    uint32_t _rr_port;  // round robin last port we sent on
    uint32_t _no_of_ports;
    uint32_t _busy_ports;

    int _ratio_data, _ratio_control, _crt;

    string _nodename;
};

// Packets are received on ports, but then passed to the Src for handling
class UecSrcPort : public PacketSink {
public:
    UecSrcPort(UecSrc& src, uint32_t portnum);
    void setRoute(const Route& route);
    inline const Route* route() const {return _route;}
    virtual void receivePacket(Packet& pkt);
    virtual const string& nodename();
private:
    UecSrc& _src;
    uint8_t _port_num;
    const Route* _route;  // we're only going to support ECMP_HOST for now.
};

class UecSrc : public EventSource, public TriggerTarget {
public:
    struct Stats {
        uint64_t sent;
        uint64_t timeouts;
        uint64_t nacks;
        uint64_t pulls;
        uint64_t rts_nacks;
    };
    UecSrc(TrafficLogger* trafficLogger, EventList& eventList, UecNIC& nic, uint32_t no_of_ports, bool rts = false);
    void delFromSendTimes(simtime_picosec time, UecDataPacket::seq_t seq_no);
    // Registers the metrics for the flow using the DataCollector singleton.
    enum CCEventType {
        QUICK_ADAPT,
        FAST_INCREASE,
        FAIR_DECREASE,
        PROPORTIONAL_DECREASE,
        FAIR_INCREASE,
        PROPORTIONAL_INCREASE,
        NACK_DECREASE,
        TIMEOUT_DECREASE,
        NO_CHANGE,
        ADDITIVE_INCREASE,
        MULTIPLICATIVE_DECREASE,
    };

    // Register all the metrics that are gonna be collected for this object. 
    void registerMetrics();
    /* 
    Register per flow metrics (flow size, flow start time, flow end time, flow completion time, base rtt,
    target rtt, rto, bdp and max cwnd)
    */
    void logMetricFlow();
    // For each receiving packet at the sender, logs: rtt, ackedBytes, isNack and if the packet has ECN.
    void logMetricAck(simtime_picosec rtt, int ackedBytes, bool isNack, bool hasECN);
    // For each change in the cwnd, log its value and the event that triggered it.
    void logMetricCCEvent(CCEventType cc_action, uint64_t cwnd);
    // For each change in the cwnd, log its value.
    void logMetricCwnd(uint64_t);

    static void disableFairDecrease();
    /**
     * Initialize global NSCC parameters.
     */
    static void initNsccParams(simtime_picosec network_rtt, linkspeed_bps linkspeed);
    /**
     * Initialize per-connection NSCC parameters.
     */
    void initNscc(mem_b cwnd, simtime_picosec peer_rtt=UecSrc::_network_rtt);
    /**
     * Initialize per-connection RCCC parameters.
     */
    void initRccc(mem_b cwnd,simtime_picosec peer_rtt=UecSrc::_network_rtt);

    void logFlowEvents(FlowEventLogger& flow_logger) { _flow_logger = &flow_logger; }
    virtual void connectPort(uint32_t portnum, Route& routeout, Route& routeback, UecSink& sink, simtime_picosec start);
    const Route* getPortRoute(uint32_t port_num) const {return _ports[port_num]->route();}
    UecSrcPort* getPort(uint32_t port_num) {return _ports[port_num];}
    void timeToSend(const Route& route);
    void receivePacket(Packet& pkt, uint32_t portnum);
    void doNextEvent();
    void setSrc(uint32_t src) { _srcaddr = src; }
    void setDst(uint32_t dst) { _dstaddr = dst; }
    const Route *get_route() { return _route_fail; }
    UecSink *get_sink() { return _sink; }
    std::vector<const Route *> get_paths() { return _paths; }
    static void setMinRTO(uint32_t min_rto_in_us) {
        _min_rto = timeFromUs((uint32_t)min_rto_in_us);
    }
    static void set_use_exp_avg_ecn(bool value) { use_exp_avg_ecn = value; }
    static bool get_use_exp_avg_ecn() { return use_exp_avg_ecn; }
    static void set_fast_increase_scaling_factor(double value) { fast_increase_scaling_factor = value; }
    static void set_prop_increase_scaling_factor(double value) { prop_increase_scaling_factor = value; }
    static void set_fair_increase_scaling_factor(double value) { fair_increase_scaling_factor = value; }
    static void set_fair_decrease_scaling_factor(double value) { fair_decrease_scaling_factor = value; }
    static void set_mult_decrease_scaling_factor(double value) { mult_decrease_scaling_factor = value; }
    static double get_fast_increase_scaling_factor() { return fast_increase_scaling_factor; }
    static double get_prop_increase_scaling_factor() { return prop_increase_scaling_factor; }
    static double get_fair_increase_scaling_factor() { return fair_increase_scaling_factor; }
    static double get_fair_decrease_scaling_factor() { return fair_decrease_scaling_factor; }
    static double get_mult_decrease_scaling_factor() { return mult_decrease_scaling_factor; }
    

    void setCwnd(mem_b cwnd) {
        //_maxwnd = cwnd;
        _cwnd = cwnd;
    }
    void setMaxWnd(mem_b maxwnd) {
        //_maxwnd = cwnd;
        _maxwnd = maxwnd;
    }

    void boundBaseRTT(simtime_picosec network_rtt){
        _base_rtt = network_rtt;
        _bdp = timeAsUs(_base_rtt) * _nic.linkspeed() / 8000000;
        _maxwnd =  1.5*_bdp;

        if (!_shown){
            cout << "Bound base RTT: _bdp " << _bdp << " _maxwnd " << _maxwnd << " _base_rtt " << timeAsUs(_base_rtt) << endl;
            _shown = true;
        }
    }
    mem_b maxWnd() const { return _maxwnd; }

    const Stats& stats() const { return _stats; }

    void setEndTrigger(Trigger& trigger);
    // called from a trigger to start the flow.
    virtual void activate();
    static uint32_t _path_entropy_size;  // now many paths do we include in our path set
    static int _global_node_count;
    static simtime_picosec _min_rto;
    static uint16_t _hdr_size;
    static uint16_t _mss;  // does not include header
    static uint16_t _mtu;  // does include header

    static bool _sender_based_cc;
    static bool _receiver_based_cc;
    simtime_picosec _last_rto_max_rtt = 0;
    simtime_picosec _last_rto_start = 0;
    simtime_picosec _max_rtt_seen = 0;
    simtime_picosec _when__max_rtt_seen = 0;
    int num_pkts = 0;
    static bool _trim_disbled;

    enum Sender_CC {
        DCTCP,
        NSCC,
        MPRDMA_CC,
        CONSTANT,
        SMARTT,
        SMARTT_ECN_AIMD,
        SMARTT_ECN_AIFD,
        SMARTT_ECN_FIMD,
        SMARTT_ECN_FIFD,
        SMARTT_RTT
    };
    enum LoadBalancing_Algo { BITMAP, REPS, OBLIVIOUS, MIXED, FLOWLET, MPRDMA, INCREMENTAL, PLB, MP, ECMP, FREEZING};
    enum PathFeedback {PATH_GOOD,PATH_ECN,PATH_NACK,PATH_TIMEOUT};
    enum EvState {STATE_GOOD,STATE_SKIP,STATE_ASSUMED_BAD};
    static Sender_CC _sender_cc_algo;
    static LoadBalancing_Algo _load_balancing_algo;

    static bool _enable_qa_gate;
    static bool _enable_avg_ecn_over_path;
    static bool mprdma_fast_recovery;

    simtime_picosec last_ecn = 0;
    std::unordered_map<uint64_t, uint16_t> hashmap_entropy;

    static bool _enable_fast_loss_recovery;
    bool sent_one = false;

    void setHashId(int from, int to, flowid_t flow_id) { 
        // Convert the numbers to strings
        std::string str1 = std::to_string(from);
        std::string str2 = std::to_string(to);

        // Concatenate the strings
        std::string concatenated = str1 + str2;

        // Convert the concatenated string back to an integer
        _src_dest_id_hash = std::stoi(concatenated);
    }

    virtual const string& nodename() { return _nodename; }
    inline void setFlowId(flowid_t flow_id) { _flow.set_flowid(flow_id); }
    void setFlowsize(uint64_t flow_size_in_bytes);
    mem_b flowsize() { return _flow_size; }
    inline PacketFlow* flow() { return &_flow; }

    inline flowid_t flowId() const { return _flow.flow_id(); }
    PacketFlow get_flow() { return _flow; }
    std::vector<int> get_path_ids() { return _path_ids; }

    vector<int> _path_ids;    
    uint32_t from = -1;
    uint32_t to = -2;
    vector<const Route *> _paths;

    // status for debugging
    uint32_t _new_packets_sent;
    uint32_t _rtx_packets_sent;
    uint32_t _rts_packets_sent;
    uint32_t _bounces_received;
    uint32_t _acks_received;
    uint32_t _src_dest_id_hash;

    simtime_picosec last_received_ack_timestamp = 0;

    static bool _debug;
    static bool _shown;
    bool _debug_src;
    bool debug() const { return _debug_src; }

   private:
    UecNIC& _nic;
    uint32_t _no_of_ports;
    vector <UecSrcPort*> _ports;
    struct sendRecord {
        // need a constructor to be able to put this in a map
        sendRecord(mem_b psize, simtime_picosec stime) : pkt_size(psize), send_time(stime){};
        mem_b pkt_size;
        simtime_picosec send_time;
    };
    UecLogger* _logger;
    TrafficLogger* _pktlogger;
    FlowEventLogger* _flow_logger;
    Trigger* _end_trigger;

    // TODO in-flight packet storage - acks and sacks clear it
    // list<UecDataPacket*> _activePackets;

    // we need to access the in_flight packet list quickly by sequence number, or by send time.
    map<UecDataPacket::seq_t, sendRecord> _tx_bitmap;
    multimap<simtime_picosec, UecDataPacket::seq_t> _send_times;

    map<UecDataPacket::seq_t, mem_b> _rtx_queue;
  
    void startFlow();
    bool isSpeculative();
    void sendIfPermitted();
    mem_b sendPacket(const Route& route);
    mem_b sendNewPacket(const Route& route);
    mem_b sendRtxPacket(const Route& route);
    void sendRTS();
    void createSendRecord(UecDataPacket::seq_t seqno, mem_b pkt_size);
    void queueForRtx(UecBasePacket::seq_t seqno, mem_b pkt_size);
    void recalculateRTO();
    void startRTO(simtime_picosec send_time);
    void clearRTO();   // timer just expired, clear the state
    void cancelRTO();  // cancel running timer and clear state
    void smartt_update_fast_increase_state(bool skip, simtime_picosec delay);
    bool smartt_should_fast_increase();
    void smartt_fast_increase();
    bool smartt_check_and_do_quick_adapt(bool skip, bool trimmed);
    void smartt_quick_adapt(bool trimmed);
    void smartt_update_wtd_state(bool skip);
    bool smartt_wtd_can_decrease();
    void clamp_cwnd();

    void (UecSrc::*smartt_main_loop)(bool skip, simtime_picosec delay);
    void smartt_vanilla_main_loop(bool skip, simtime_picosec delay);
    void smartt_ecn_aimd_main_loop(bool skip, simtime_picosec delay);
    void smartt_ecn_aifd_main_loop(bool skip, simtime_picosec delay);
    void smartt_ecn_fimd_main_loop(bool skip, simtime_picosec delay);
    void smartt_ecn_fifd_main_loop(bool skip, simtime_picosec delay);
    void smartt_rtt_main_loop(bool skip, simtime_picosec delay);

    // not used, except for debugging timer issues
    void checkRTO() {
        if (_rtx_timeout_pending)
            assert(_rto_timer_handle != eventlist().nullHandle());
        else
            assert(_rto_timer_handle == eventlist().nullHandle());
    }

    
    void rtxTimerExpired();
    UecBasePacket::pull_quanta computePullTarget();
    void handlePull(UecBasePacket::pull_quanta pullno);
    mem_b handleAckno(UecDataPacket::seq_t ackno);
    mem_b handleCumulativeAck(UecDataPacket::seq_t cum_ack);
    void processAck(const UecAckPacket& pkt);
    void processNack(const UecNackPacket& pkt);
    void processPull(const UecPullPacket& pkt);
    void fastLossRecovery(uint32_t ooo, UecBasePacket::seq_t cum_ack);

    //added for NSCC
    void quick_adapt(bool trimmed);
    void updateCwndOnAck_NSCC(bool skip, simtime_picosec delay, mem_b newly_acked_bytes);
    void updateCwndOnNack_NSCC(bool skip, mem_b nacked_bytes);

    void updateCwndOnAck_SMARTT(bool skip, simtime_picosec delay, mem_b newly_acked_bytes);
    void updateCwndOnNack_SMARTT(bool skip, mem_b nacked_bytes);

    void updateCwndOnAck_DCTCP(bool skip, simtime_picosec delay, mem_b newly_acked_bytes);
    void updateCwndOnNack_DCTCP(bool skip, mem_b nacked_bytes);
    
    void updateCwndOnAck_MPRDMA(bool skip, simtime_picosec delay, mem_b newly_acked_bytes);
    void updateCwndOnNack_MPRDMA(bool skip, mem_b nacked_bytes);

    void dontUpdateCwndOnAck(bool skip, simtime_picosec delay, mem_b newly_acked_bytes);
    void dontUpdateCwndOnNack(bool skip, mem_b nacked_bytes);

    void (UecSrc::*updateCwndOnAck)(bool skip, simtime_picosec delay, mem_b newly_acked_bytes);
    void (UecSrc::*updateCwndOnNack)(bool skip, mem_b nacked_bytes);

    uint16_t nextEntropy_bitmap();
    uint16_t nextEntropy_REPS();
    uint16_t nextEntropy_oblivious();
    uint16_t nextEntropy_incremental();
    uint16_t nextEntropy_mixed();
    uint16_t nextEntropy_plb();
    uint16_t nextEntropy_mp();
    uint16_t nextEntropy_ecmp();
    uint16_t nextEntropy_freezing();
    uint16_t nextEntropy_flowlet();
    uint16_t nextEntropy_mprdma();

    void processEv_bitmap(uint16_t path_id, PathFeedback feedback);
    void processEv_REPS(uint16_t path_id, PathFeedback feedback);
    void processEv_oblivious(uint16_t path_id, PathFeedback feedback);
    void processEv_incremental(uint16_t path_id, PathFeedback feedback);
    void processEv_mixed(uint16_t path_id, PathFeedback feedback);
    void processEv_plb(uint16_t path_id, PathFeedback feedback);
    void processEv_mp(uint16_t path_id, PathFeedback feedback);
    void processEv_ecmp(uint16_t path_id, PathFeedback feedback);
    void processEv_freezing(uint16_t path_id, PathFeedback feedback);
    void processEv_flowlet(uint16_t path_id, PathFeedback feedback);
    void processEv_mprdma(uint16_t path_id, PathFeedback feedback);

    inline EvState ev_state(uint16_t path) const { 
        if (_ev_skip_bitmap[path]==0) 
            return STATE_GOOD; 
        else if (_ev_skip_bitmap[path]==_max_penalty) 
            return STATE_ASSUMED_BAD; 
        else 
            return STATE_SKIP;
    }

    uint16_t (UecSrc::*nextEntropy)();
    void (UecSrc::*processEv)(uint16_t path_id, PathFeedback feedback);

    bool checkFinished(UecDataPacket::seq_t cum_ack);

    Stats _stats;
    UecSink* _sink;

    // unlike in the NDP simulator, we maintain all the main quantities in bytes
    mem_b _flow_size;
    bool _done_sending;  // make sure we only trigger once
    mem_b _backlog;      // how much we need to send, not including retransmissions
    mem_b _rtx_backlog;
    mem_b _cwnd;
    mem_b _maxwnd;
    UecBasePacket::pull_quanta _pull_target;
    UecBasePacket::pull_quanta _pull;
    mem_b _credit;  // receive request credit in pull_quanta, but consume it in bytes
    inline mem_b credit() const;
    void stopSpeculating();
    void spendCredit(mem_b pktsize);
    UecDataPacket::seq_t _highest_sent;
    UecDataPacket::seq_t _highest_rtx_sent;
    mem_b _in_flight;
    mem_b _bdp;
    bool _send_blocked_on_nic;
    bool _speculating;

    // Original SMaRTT extra parameters
    bool need_quick_adapt = false;
    double exp_avg_ecn_value = 0.3;
    double exp_avg_alpha = 0.05;
    double exp_avg_ecn = 0;
    uint32_t target_window;
    static bool use_exp_avg_ecn;
    uint32_t counter_consecutive_good_bytes = 0;
    bool increasing = false;
    static double fast_increase_scaling_factor;
    static double prop_increase_scaling_factor;
    static double fair_increase_scaling_factor;
    static double fair_decrease_scaling_factor;
    static double mult_decrease_scaling_factor;
    static double target_rtt_scaling_factor;
    static bool use_fast_increase;
    uint64_t previous_window_end = 0;
    uint32_t saved_acked_bytes = 0;
    const Route *_route_fail;

public:
    static linkspeed_bps _reference_network_linkspeed; 
    static simtime_picosec _reference_network_rtt; 
    static mem_b _reference_network_bdp; 
    static linkspeed_bps _network_linkspeed; 
    static simtime_picosec _network_rtt; 
    static mem_b _network_bdp; 
    // Smarttrack parameters
    static uint32_t _qa_scaling; 
    static simtime_picosec _target_Qdelay;
    static double _gamma;
    static uint32_t _pi;
    static double _alpha;
    // static double _scaling_c;
    // static double _fd;
    static double _fi;
    static double _fi_scale;
    static double _scaling_factor_a;
    static double _scaling_factor_b;
    static double _eta;
    static double _qa_threshold; 
    static double _ecn_alpha;
    static bool _save_rtt; 
    static bool _connections_mapping;
    static bool _mixed_lb_traffic;
    static int _num_mp_flows;
    static bool _collect_data;
    static double _delay_alpha;
    static bool _use_timeouts;
    // static double _ecn_thresh;
    static uint32_t _adjust_bytes_threshold;
    static simtime_picosec _adjust_period_threshold;
    //debug
    static flowid_t _debug_flowid;
private:
    bool quick_adapt(bool is_loss, simtime_picosec avgqdelay);
    void fair_increase(uint32_t newly_acked_bytes);
    void proportional_increase(uint32_t newly_acked_bytes,simtime_picosec delay);
    void fast_increase(uint32_t newly_acked_bytes,simtime_picosec delay);
    // void fair_decrease(bool can_decrease, uint32_t newly_acked_bytes);
    void multiplicative_decrease(uint32_t newly_acked_bytes);
    void fulfill_adjustment();
    void mark_packet_for_retransmission(UecBasePacket::seq_t psn, uint16_t pktsize);
    void update_delay(simtime_picosec delay, bool update_avg, bool skip);
    void update_base_rtt(simtime_picosec raw_rtt, uint16_t packet_size);
    simtime_picosec get_avg_delay();
    uint16_t get_avg_pktsize();
    void average_ecn_bytes(uint32_t pktsize, uint32_t newly_acked_bytes, bool skip);

    // entropy value calculation
    uint16_t _no_of_paths;       // must be a power of 2
    uint16_t _path_random;       // random upper bits of EV, set at startup and never changed
    uint16_t _path_xor;          // random value set each time we wrap the entropy values - XOR with
                                 // _current_ev_index
    uint16_t _current_ev_index;  // count through _no_of_paths and then wrap.  XOR with _path_xor to
                                 // get EV
    vector<uint16_t> _ev_skip_bitmap;  // paths scores for load balancing
    uint16_t _max_penalty;             // max value we allow in _path_penalties (typically 1 or 2).
    uint16_t _ev_skip_count;
    uint16_t _ev_bad_count;
    vector<uint16_t> _ev_bitmap_failed;  // paths scores for load balancing
    bool background_traffic = false;

    // RTT estimate data for RTO and sender based CC.
    simtime_picosec _rtt, _mdev, _rto, _raw_rtt;
    bool _rtx_timeout_pending;       // is the RTO running?
    simtime_picosec _rto_send_time;  // when we sent the oldest packet that the RTO is waiting on.
    simtime_picosec _rtx_timeout;    // when the RTO is currently set to expire
    simtime_picosec _last_rts;       // time when we last sent an RTS (or zero if never sent)
    EventList::Handle _rto_timer_handle;


    //used to drive ACK clock
    uint64_t _recvd_bytes;

    // Smarttrack sender based CC variables.
    simtime_picosec _base_rtt;
    mem_b _base_bdp;
    mem_b _achieved_bytes = 0;
    //used to trigger SmartTrack fulfill
    mem_b _received_bytes = 0;
    uint32_t _fi_count = 0;
    bool _trigger_qa = false;
    simtime_picosec _qa_endtime = 0;
    uint32_t _bytes_to_ignore = 0;
    uint32_t _bytes_ignored = 0;
    uint32_t _inc_bytes = 0;
    double _exp_avg_ecn = 0;
    simtime_picosec _avg_delay = 0;

    // LB
    int flowlet_entropy = 0;
    simtime_picosec last_pkt_flowlet = 0;
    simtime_picosec flowlet_timeout = 0;
    simtime_picosec flowlet_timeout_wait = 0;
    int next_mp_to_use = 0;
    int mprdma_previous_cwnd = 0;
    bool mprdma_can_send = false;
    int mprdma_entropy = -1;
    int plb_entropy = 0;
    int working_path_ecmp_mp = 0;
    int plb_congested_rounds = 0;
    int plb_rounds_threshold = 0;
    int plb_congested_rounds_threshold = 0;
    int plb_congested = 0;
    int plb_delivered = 0;
    int plb_k = 0;
    simtime_picosec plb_last_rtt = 0;
    simtime_picosec plb_timeout_wait = 0;
    bool plb_timeout = false;

    CircularBufferREPS<int> *circular_buffer_reps;

    simtime_picosec _last_eta_time = 0;
    

    simtime_picosec _last_adjust_time = 0;
    bool _increase = false;
    simtime_picosec _last_dec_time = 0;
    uint32_t _highest_recv_seqno;
    bool _loss_recovery_mode = false;
    uint32_t _recovery_seqno = 0;
    uint32_t _loss_counter = 0;

    uint16_t _crt_path;
    list<uint16_t> _next_pathid;
    list<uint16_t> _knowngood_pathid;

    // Connectivity
    PacketFlow _flow;
    simtime_picosec _flow_start_time;
    string _nodename;
    int _node_num;
    uint32_t _srcaddr;
    uint32_t _dstaddr;


    // Old logging
    list<std::tuple<simtime_picosec, bool, uint64_t, uint64_t>> _received_ecn; // list of packets received
    unsigned _nack_rtx_pending;
    vector<pair<simtime_picosec, uint64_t>> _list_cwd;
    vector<tuple<simtime_picosec, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t>> _list_rtt;
    vector<pair<simtime_picosec, uint64_t>> _list_nack;
    vector<pair<simtime_picosec, uint64_t>> _list_ecn_received;
    vector<pair<simtime_picosec, uint64_t>> _list_valid_entropies;

    vector<pair<simtime_picosec, uint64_t>> _list_new_exploration;
    vector<pair<simtime_picosec, uint64_t>> _list_valid_entropy;
    vector<uint64_t> _list_psn;
    vector<pair<simtime_picosec, int>> _list_max_distance_over_time;
    vector<pair<simtime_picosec, double>> _list_ratio_over_time;
    vector<pair<simtime_picosec, double>> _list_ratio_over_rtt;
    vector<pair<simtime_picosec, uint64_t>> _list_invalid_entropy;
    std::string _src_dst;                // Identifier for the flow.
    vector<uint64_t> _list_psn_over_time;

      // Data collection for the flow.
    std::string _src_dst_flowid;                // Identifier for the flow.
    htsim::CsvMetric* _flow_metric;             // Logs the flow metrics of a given flow.
    htsim::TimeSeriesMetric* _ack_metric;       // Logs the metrics when receiving an ACK/NACK.
    htsim::TimeSeriesMetric* _cc_event_metric;  // Logs when a certain CC event happens
    htsim::TimeSeriesMetric* _cwnd_metric;      // Logs when cwnd changes
};

// Packets are received on ports, but then passed to the Sink for handling
class UecSinkPort : public PacketSink {
public:
    UecSinkPort(UecSink& sink, uint32_t portnum);
    void setRoute(const Route& route);
    inline const Route* route() const {return _route;}
    virtual void receivePacket(Packet& pkt);
    virtual const string& nodename();
private:
    UecSink& _sink;
    uint8_t _port_num;
    const Route* _route;
};

class UecSink : public DataReceiver {
   public:
    struct Stats {
        uint64_t received;
        uint64_t bytes_received;
        uint64_t duplicates;
        uint64_t out_of_order;
        uint64_t trimmed;
        uint64_t pulls;
        uint64_t rts;
        uint64_t ecn_received;
        uint64_t ecn_bytes_received;
    };

    UecSink(TrafficLogger* trafficLogger, UecPullPacer* pullPacer, UecNIC& nic, uint32_t no_of_ports);
    UecSink(TrafficLogger* trafficLogger,
             linkspeed_bps linkSpeed,
             double rate_modifier,
             uint16_t mtu,
             EventList& eventList,
             UecNIC& nic, uint32_t no_of_ports);
    void receivePacket(Packet& pkt, uint32_t port_num);

    void processData(UecDataPacket& pkt);
    void processRts(const UecRtsPacket& pkt);
    void processTrimmed(const UecDataPacket& pkt);

    void handlePullTarget(UecBasePacket::seq_t pt);

    virtual const string& nodename() { return _nodename; }
    virtual uint64_t cumulative_ack() { return _expected_epsn; }
    virtual uint32_t drops() { return 0; }

    inline flowid_t flowId() const { return _flow.flow_id(); }

    UecPullPacket* pull();

    bool shouldSack();
    uint16_t unackedPackets();
    void setEndTrigger(Trigger& trigger);

    UecBasePacket::seq_t sackBitmapBase(UecBasePacket::seq_t epsn);
    UecBasePacket::seq_t sackBitmapBaseIdeal();
    uint64_t buildSackBitmap(UecBasePacket::seq_t ref_epsn);
    UecAckPacket* sack(uint16_t path_id, UecBasePacket::seq_t seqno, UecBasePacket::seq_t acked_psn, bool ce);

    UecNackPacket* nack(uint16_t path_id, UecBasePacket::seq_t seqno);
    static bool _use_timeouts;
    uint32_t _src_dest_id_hash;
    uint32_t from = -1;
    uint32_t to = -2;
    vector<int> _path_ids;   
    vector<int> get_path_ids() { return _path_ids; }
    vector<const Route *> get_paths() { return _paths; }
    vector<const Route *> _paths;
    const Route *get_route() { return _route_fail; }
    const Route *_route_fail;
    UecBasePacket::pull_quanta backlog() {
        if (_highest_pull_target > _latest_pull)
            return _highest_pull_target - _latest_pull;
        else
            return 0;
    }
    UecBasePacket::pull_quanta slowCredit() {
        if (_highest_pull_target >= _latest_pull)
            return 0;
        else
            return _latest_pull - _highest_pull_target;
    }

    UecBasePacket::pull_quanta rtx_backlog() { return _retx_backlog; }
    const Stats& stats() const { return _stats; }
    void connectPort(uint32_t port_num, UecSrc& src, const Route& routeback);
    const Route* getPortRoute(uint32_t port_num) const {return _ports[port_num]->route();}
    UecSinkPort* getPort(uint32_t port_num) {return _ports[port_num];}
    void setSrc(uint32_t s) { _srcaddr = s; }

    void setHashId(int from, int to, flowid_t flow_id) { 
        // Convert the numbers to strings
        std::string str1 = std::to_string(from);
        std::string str2 = std::to_string(to);

        // Concatenate the strings
        std::string concatenated = str1 + str2;

        // Convert the concatenated string back to an integer
        _src_dest_id_hash = std::stoi(concatenated);
    }

    inline void setFlowId(flowid_t flow_id) { _flow.set_flowid(flow_id); }

    inline bool inPullQueue() const { return _in_pull; }
    inline bool inSlowPullQueue() const { return _in_slow_pull; }

    inline void addToPullQueue() { _in_pull = true; }
    inline void removeFromPullQueue() { _in_pull = false; }
    inline void addToSlowPullQueue() {
        _in_pull = false;
        _in_slow_pull = true;
    }
    inline void removeFromSlowPullQueue() {
        _in_pull = false;
        _in_slow_pull = false;
    }
    inline UecNIC* getNIC() const { return &_nic; }

    inline void setPCIeModel(PCIeModel* c){assert(_model_pcie); _pcie = c;}
    inline void setOversubscribedCC(OversubscribedCC* c){_receiver_cc = c;}

    uint16_t nextEntropy();

    UecSrc* getSrc() { return _src; }
    uint32_t getMaxCwnd() { return _src->maxWnd(); };

    PCIeModel* pcieModel() const{ return _pcie;}

    static mem_b _bytes_unacked_threshold;
    static UecBasePacket::pull_quanta _credit_per_pull;
    static int TGT_EV_SIZE;

    static bool _receiver_oversubscribed_cc; 

    // for sink logger
    inline mem_b total_received() const { return _stats.bytes_received; }
    uint32_t reorder_buffer_size();  // count is in packets

    inline UecPullPacer* pullPacer() const {return _pullPacer;}

   private:
    uint32_t _no_of_ports;
    vector <UecSinkPort*> _ports;
    uint32_t _srcaddr;
    UecNIC& _nic;
    UecSrc* _src;
    PacketFlow _flow;
    UecPullPacer* _pullPacer;
    UecBasePacket::seq_t _expected_epsn;
    UecBasePacket::seq_t _high_epsn;
    UecBasePacket::seq_t
        _ref_epsn;  // used for SACK bitmap calculation in spec, unused here for NOW.
    UecBasePacket::pull_quanta _retx_backlog;
    UecBasePacket::pull_quanta _latest_pull;
    UecBasePacket::pull_quanta _highest_pull_target;

    bool _in_pull;       // this tunnel is in the pull queue.
    bool _in_slow_pull;  // this tunnel is in the slow pull queue.


    //received payload bytes, used to decide when flow has finished.
    mem_b _received_bytes;
    uint16_t _accepted_bytes;

    //used to help the sender slide his window.
    uint64_t _recvd_bytes;
    //used for flow control in sender CC mode. 
    //decides whether to reduce cwnd at sender; will change dynamically based on receiver resource availability. 
    uint8_t _rcv_cwnd_pen;

    Trigger* _end_trigger;
    ModularVector<uint8_t, uecMaxInFlightPkts>
        _epsn_rx_bitmap;  // list of packets above a hole, that we've received

    uint32_t _out_of_order_count;
    bool _ack_request;

    uint16_t _entropy;

    //variables for PCIe model
    PCIeModel* _pcie;
    OversubscribedCC* _receiver_cc;

    Stats _stats;
    string _nodename;

public:
    static bool _oversubscribed_cc;
    static bool _model_pcie;
};

class UecPullPacer : public EventSource {
   public:
    enum reason {PCIE = 0, OVERSUBSCRIBED_CC = 1};

    UecPullPacer(linkspeed_bps linkSpeed,
                  double pull_rate_modifier,
                  uint16_t mtu,
                  EventList& eventList,
                  uint32_t no_of_ports);
    void doNextEvent();
    void requestPull(UecSink* sink);

    bool isActive(UecSink* sink);
    bool isIdle(UecSink* sink);

    inline uint16_t mtu() const {return _mtu;}
    inline linkspeed_bps linkspeed() const {return _linkspeed;}

    void updatePullRate(reason r,double relative_rate);

    simtime_picosec packettime() const {return _actualPktTime;}

   private:
    list<UecSink*> _active_senders;  // TODO priorities?
    list<UecSink*> _idle_senders;    // TODO priorities?

    const simtime_picosec _pktTime;
    simtime_picosec _actualPktTime;
    bool _active;
    
    double _rates[2];

    linkspeed_bps _linkspeed;
    uint16_t _mtu;
};

#endif  // UEC_H
