// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include "compositequeue.h"
#include <math.h>
#include <iostream>
#include <sstream>
#include "ecn.h"

static int global_queue_id=0;
bool CompositeQueue::_collect_data = false;
bool CompositeQueue::scenario_micro_failures = false;
bool CompositeQueue::_use_timeouts = true;

#define DEBUG_QUEUE_ID -1 // set to queue ID to enable debugging

CompositeQueue::CompositeQueue(linkspeed_bps bitrate, mem_b maxsize, EventList& eventlist, 
                               QueueLogger* logger, uint16_t trim_size, bool disable_trim)
    : Queue(bitrate, maxsize, eventlist, logger)
{
    _disable_trim = disable_trim;
    _trim_size = trim_size;
    _ratio_high = 100000;
    _ratio_low = 1;
    _crt = 0;
    _num_headers = 0;
    _num_packets = 0;
    _num_acks = 0;
    _num_nacks = 0;
    _num_pulls = 0;
    _num_drops = 0;
    _num_stripped = 0;
    _num_bounced = 0;
    _ecn_minthresh = maxsize*2; // don't set ECN by default
    _ecn_maxthresh = maxsize*2; // don't set ECN by default

    _return_to_sender = false;

    _queuesize_high = _queuesize_low = 0;
    _serv = QUEUE_INVALID;
    stringstream ss;
    ss << "compqueue(" << bitrate/1000000 << "Mb/s," << maxsize << "bytes)";
    _nodename = ss.str();
    _queue_id = global_queue_id++;
    if (_queue_id == DEBUG_QUEUE_ID)
        cout << "queueid " << _queue_id << " bitrate " << bitrate/1000000 << "Mb/s," << endl;
}

void CompositeQueue::beginService(){
    if (!_enqueued_high.empty()&&!_enqueued_low.empty()){
        _crt++;

        if (_crt >= (_ratio_high+_ratio_low))
            _crt = 0;

        if (_crt< _ratio_high) {
            _serv = QUEUE_HIGH;
            eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_high.back()));
        } else {
            assert(_crt < _ratio_high+_ratio_low);
            _serv = QUEUE_LOW;
            eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_low.back()));      
        }
        return;
    }

    if (!_enqueued_high.empty()) {
        _serv = QUEUE_HIGH;
        eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_high.back()));
    } else if (!_enqueued_low.empty()) {
        _serv = QUEUE_LOW;
        eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_low.back()));
    } else {
        assert(0);
        _serv = QUEUE_INVALID;
    }
}

bool CompositeQueue::decide_ECN() {
    //ECN mark on deque
    if (_queuesize_low > _ecn_maxthresh) {
        return true;
    } else if (_queuesize_low > _ecn_minthresh) {
        uint64_t p = (0x7FFFFFFF * (_queuesize_low - _ecn_minthresh))/(_ecn_maxthresh - _ecn_minthresh);
        if ((uint64_t)random() < p) {
            return true;
        }
    }
    return false;
}

void CompositeQueue::completeService(){
    Packet* pkt;
    if (_serv==QUEUE_LOW){
        assert(!_enqueued_low.empty());
        pkt = _enqueued_low.pop();
        _queuesize_low -= pkt->size();

        //ECN mark on deque
        if (decide_ECN()) {
            pkt->set_flags(pkt->flags() | ECN_CE);
        }
        if (_queue_id == DEBUG_QUEUE_ID) {
            cout << timeAsUs(eventlist().now()) <<" name " <<_nodename <<" _queuesize_low " 
                << _queuesize_low*8/((_bitrate/1000000.0)) <<" _queueid " << _queue_id << " switch " << _switch->getID() 
                << " ecn " << decide_ECN() 
                << " _queuesize_high " << _queuesize_high*8/((_bitrate/1000000.0))
                << endl;    

        }
        if (_logger) _logger->logQueue(*this, QueueLogger::PKT_SERVICE, *pkt);
        _num_packets++;
    } else if (_serv==QUEUE_HIGH) {
        assert(!_enqueued_high.empty());
        pkt = _enqueued_high.pop();
        _queuesize_high -= pkt->size();
        if (_logger) _logger->logQueue(*this, QueueLogger::PKT_SERVICE, *pkt);
        if (pkt->type() == NDPACK)
            _num_acks++;
        else if (pkt->type() == NDPNACK)
            _num_nacks++;
        else if (pkt->type() == NDPPULL)
            _num_pulls++;
        else {
            //cout << "Hdr: type=" << pkt->type() << endl;
            _num_headers++;
            //ECN mark on deque of a header, if low priority queue is still over threshold
//            if (decide_ECN()) {
//                pkt->set_flags(pkt->flags() | ECN_CE);
//            }
        }
    } else {
        assert(0);
    }
    
    pkt->flow().logTraffic(*pkt,*this,TrafficLogger::PKT_DEPART);
    pkt->sendOn();

    //_virtual_time += drainTime(pkt);
  
    _serv = QUEUE_INVALID;
  
    if (!_enqueued_high.empty()||!_enqueued_low.empty())
        beginService();
}

void CompositeQueue::doNextEvent() {
    completeService();
}

void CompositeQueue::receivePacket(Packet& pkt)
{
    FAILURE_GENERATOR->dropRandomPacket(pkt);

    if (_queue_id == DEBUG_QUEUE_ID)
    {
        cout << timeAsUs(eventlist().now()) << " name " << _nodename << " arrive "
             << _queuesize_low * 8 / ((_bitrate / 1000000.0)) << " _queueid " << _queue_id << " switch " << _switch->getID() 
             <<" flowid " << pkt.flow_id() << " ev " << pkt.pathid()<< endl;
    }
    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_ARRIVE);
    if (_logger) _logger->logQueue(*this, QueueLogger::PKT_ARRIVE, pkt);


    // Only for Microscopic Benchmark
    if (scenario_micro_failures) {
        if ("compqueue(400000Mb/s,415000bytes)LS0->US1(0)" == _nodename && pkt.size() > 100 && eventlist().now() > 100000000 && eventlist().now() < 200000000) {
            printf("Dropping a PKT");
            pkt.free();
            return;
        }

        if ("compqueue(400000Mb/s,415000bytes)LS0->US3(0)" == _nodename && pkt.size() > 100 && eventlist().now() > 350000000 && eventlist().now() < 600000000) {
            printf("Dropping a PKT");
            pkt.free();
            return;
        }

        if (eventlist().now() > 650000000) {
            UecSrc::mprdma_fast_recovery = true;
        }
        /* if ("compqueue(400000Mb/s,415000bytes)LS0->US0(0)" == _nodename && pkt.size() > 100 && eventlist().now() > 100000000) {
            printf("Dropping a PKT1");
            pkt.free();
            return;
        }

        if ("compqueue(400000Mb/s,415000bytes)LS0->US1(0)" == _nodename && pkt.size() > 100 && eventlist().now() > 300000000) {
            printf("Dropping a PKT2");
            pkt.free();
            return;
        }

        if ("compqueue(400000Mb/s,415000bytes)LS0->US2(0)" == _nodename && pkt.size() > 100 && eventlist().now() > 500000000) {
            printf("Dropping a PKT3");
            pkt.free();
            return;
        } */

    }
    
    
    


    if (FAILURE_GENERATOR->simSwitchFailures(pkt, _switch, *this)) {

        /* printf("Dropping a PKT (%dto%d) - Size %d Entropy %d %d - Time %lu\n",pkt.from, pkt.to, pkt.size(),
            pkt.pathid(), 1, eventlist().now() / 1000); */

        FAILURE_GENERATOR->nr_dropped_packets++;
        FAILURE_GENERATOR->_list_switch_packet_drops.push_back(eventlist().now());

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


    if (!pkt.header_only()){


        if (pkt.size() > 100 && _collect_data) {

            std::string str = _nodename;

            // Find the start of the substring after the first ')'
            size_t startPos = str.find(")") + 1;

            // Find the end of the substring before the next '('
            size_t endPos = str.find("(", startPos);

            // Extract the desired substring
            std::string result = str.substr(startPos, endPos - startPos);

            std::string file_name = (SAVE_DATA_FOLDER + "/queueSize/queueSize" + result + ".txt");
            std::ofstream MyQueueFile(file_name, std::ios_base::app);
            MyQueueFile << eventlist().now() / 1000 << "," << int(_queuesize_low * 8 / (_bitrate / 1e9))  << std::endl;

            MyQueueFile.close();
        }

        if (_queuesize_low+pkt.size() <= _maxsize  || drand()<0.5) {
            //regular packet; don't drop the arriving packet

            // we are here because either the queue isn't full or,
            // it might be full and we randomly chose an
            // enqueued packet to trim
            
            if (_queuesize_low+pkt.size()>_maxsize){
                // we're going to drop an existing packet from the queue
                if (_enqueued_low.empty()){
                    //cout << "QUeuesize " << _queuesize_low << " packetsize " << pkt.size() << " maxsize " << _maxsize << endl;
                    assert(0);
                }
                //take last packet from low prio queue, make it a header and place it in the high prio queue
                Packet* booted_pkt = _enqueued_low.pop_front();
                _queuesize_low -= booted_pkt->size();
                if (_logger) _logger->logQueue(*this, QueueLogger::PKT_UNQUEUE, *booted_pkt);

                if (_disable_trim) {
                    booted_pkt->free();
                    _num_drops++;
                    /* cout << "A [ " << _enqueued_low.size() << " " << _enqueued_high.size() << " ] DROP "
                         << " flowid " << booted_pkt->flow_id()<< endl; */
                } else {
                    // cout << "A [ " << _enqueued_low.size() << " " << _enqueued_high.size() << " ] STRIP" << endl;
                    // cout << "booted_pkt->size(): " << booted_pkt->size();
                    booted_pkt->strip_payload(_trim_size);
                    // cout << "CQ trim at " << _nodename << endl;
                    _num_stripped++;
                    booted_pkt->flow().logTraffic(*booted_pkt, *this, TrafficLogger::PKT_TRIM);
                    if (_logger)
                        _logger->logQueue(*this, QueueLogger::PKT_TRIM, pkt);

                    if (_queuesize_high + booted_pkt->size() > 200 * _maxsize) {
                        if (_return_to_sender && booted_pkt->reverse_route() && booted_pkt->bounced() == false) {
                            // return the packet to the sender
                            if (_logger)
                                _logger->logQueue(*this, QueueLogger::PKT_BOUNCE, *booted_pkt);
                            booted_pkt->flow().logTraffic(pkt, *this, TrafficLogger::PKT_BOUNCE);
                            // XXX what to do with it now?
#if 0
                            printf("Bounce2 at %s\n", _nodename.c_str());
                            printf("Fwd route:\n");
                            print_route(*(booted_pkt->route()));
                            printf("nexthop: %d\n", booted_pkt->nexthop());
#endif
                            booted_pkt->bounce();
#if 0
                            printf("\nRev route:\n");
                            print_route(*(booted_pkt->reverse_route()));
                            printf("nexthop: %d\n", booted_pkt->nexthop());
#endif
                            _num_bounced++;
                            booted_pkt->sendOn();
                        } else {
                            booted_pkt->flow().logTraffic(*booted_pkt, *this, TrafficLogger::PKT_DROP);
                            booted_pkt->free();
                            if (_logger)
                                _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
                        }
                    } else {
                        _enqueued_high.push(booted_pkt);
                        _queuesize_high += booted_pkt->size();
                        if (_logger)
                            _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, *booted_pkt);
                    }
                }
            }

            //assert(_queuesize_low+pkt.size()<= _maxsize);
            Packet* pkt_p = &pkt;
            _enqueued_low.push(pkt_p);
            _queuesize_low += pkt.size();
            if (_logger) _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);
            
            if (_serv==QUEUE_INVALID) {
                beginService();
            }
            
            //cout << "BL[ " << _enqueued_low.size() << " " << _enqueued_high.size() << " ]" << endl;
            
            return;
        } else {
            if (_disable_trim) {
                if (_queue_id == DEBUG_QUEUE_ID) {
                    cout <<timeAsUs(eventlist().now()) << "B[ " << _enqueued_low.size() << " "
                         << _enqueued_high.size() << " ] DROP " << pkt.flow().flow_id() << " queue "
                         << str() << " pathid " <<pkt.pathid()<< " queueid " << _queue_id
                         << " size " << pkt.size() << endl;
                }
                pkt.free();
                _num_drops++;
                return;
            }
            //strip packet the arriving packet - low priority queue is full
            //cout << "B [ " << _enqueued_low.size() << " " << _enqueued_high.size() << " ] STRIP" << endl;
            pkt.strip_payload(_trim_size);
            //cout << "CQ trim at " << _nodename << endl;
            _num_stripped++;
            pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_TRIM);
            if (_logger) _logger->logQueue(*this, QueueLogger::PKT_TRIM, pkt);
        }
    }
    assert(pkt.header_only());
    
    if (_queuesize_high+pkt.size() > 200*_maxsize) {
        //drop header
        //cout << "drop!\n";
        if (_return_to_sender && pkt.reverse_route()  && pkt.bounced() == false) {
            //return the packet to the sender
            if (_logger) _logger->logQueue(*this, QueueLogger::PKT_BOUNCE, pkt);
            pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_BOUNCE);
            //XXX what to do with it now?
#if 0
            printf("Bounce1 at %s\n", _nodename.c_str());
            printf("Fwd route:\n");
            print_route(*(pkt.route()));
            printf("nexthop: %d\n", pkt.nexthop());
#endif
            pkt.bounce();
#if 0
            printf("\nRev route:\n");
            print_route(*(pkt.reverse_route()));
            printf("nexthop: %d\n", pkt.nexthop());
#endif
            _num_bounced++;
            pkt.sendOn();
            return;
        } else {
            if (_logger) _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
            pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_DROP);
            /* cout << "B[ " << _enqueued_low.size() << " " << _enqueued_high.size() << " ] DROP " 
                 << pkt.flow().flow_id() << endl; */
            pkt.free();
            _num_drops++;
            return;
        }
    }
    
    
    //if (pkt.type()==NDP)
    //  cout << "H " << pkt.flow().str() << endl;
    Packet* pkt_p = &pkt;
    _enqueued_high.push(pkt_p);
    _queuesize_high += pkt.size();
    if (_logger) _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);
    
    //cout << "BH[ " << _enqueued_low.size() << " " << _enqueued_high.size() << " ]" << endl;
    
    if (_serv==QUEUE_INVALID) {
        beginService();
    }
}

mem_b CompositeQueue::queuesize() const {
    return _queuesize_low + _queuesize_high;
}
