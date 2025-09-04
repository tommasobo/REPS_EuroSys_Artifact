// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
//#include "config.h"
#include <sstream>
#include <string.h>

#include <math.h>
#include <unistd.h>
#include "circular_buffer.h"
#include "network.h"
#include "pipe.h"
#include "eventlist.h"
#include "logfile.h"
#include "uec_logger.h"
#include "clock.h"
#include "uec.h"
#include "compositequeue.h"
#include "topology.h"
#include "connection_matrix.h"
#include "pciemodel.h"
#include "oversubscribed_cc.h"
#include "data_collector.h"


#include "fat_tree_topology.h"
#include "fat_tree_switch.h"

#include <list>
#include <sys/types.h>
#include <sys/stat.h>

// Simulation params

//#define PRINTPATHS 1

#include "main.h"

int DEFAULT_NODES = 128;
#define DEFAULT_QUEUE_SIZE 35
// #define DEFAULT_CWND 50

EventList& eventlist = EventList::getTheEventList();

void exit_error(char* progr) {
    cout << "Usage " << progr << " [-nodes N]\n\t[-conns C]\n\t[-cwnd cwnd_size]\n\t[-q queue_size]\n\t[-recv_oversub_cc] Use receiver-driven AIMD to reduce total window when trims are not last hop\n\t[-queue_type composite|random|lossless|lossless_input|]\n\t[-tm traffic_matrix_file]\n\t[-strat route_strategy (single,rand,perm,pull,ecmp,\n\tecmp_host path_count,ecmp_ar,ecmp_rr,\n\tecmp_host_ar ar_thresh)]\n\t[-log log_level]\n\t[-seed random_seed]\n\t[-end end_time_in_usec]\n\t[-mtu MTU]\n\t[-hop_latency x] per hop wire latency in us,default 1 \n\t[-disable_fd] disable fair decrease to get higher throught, \n\t[-target_q_delay x] target_queuing_delay in us, default is 6us \n\t[-switch_latency x] switching latency in us, default 0\n\t[-host_queue_type  swift|prio|fair_prio]\n\t[-logtime dt] sample time for sinklogger, etc" << endl;
    exit(1);
}

int main(int argc, char **argv) {
    Clock c(timeFromSec(5 / 100.), eventlist);
    bool param_queuesize_set = false;
    mem_b queuesize = DEFAULT_QUEUE_SIZE;
    linkspeed_bps linkspeed = speedFromMbps((double)HOST_NIC);
    int packet_size = 4150;
    uint32_t path_entropy_size = 64;
    uint32_t no_of_conns = 0, cwnd = 0, no_of_nodes = 0;
    uint32_t tiers = 3; // we support 2 and 3 tier fattrees
    uint32_t planes = 1;  // multi-plane topologies
    uint32_t ports = 1;  // ports per NIC
    bool disable_trim = false; // Disable trimming, drop instead
    uint16_t trimsize = 64; // size of a trimmed packet
    simtime_picosec logtime = timeFromMs(0.25); // ms;
    stringstream filename(ios_base::out);
    simtime_picosec hop_latency = timeFromUs((uint32_t)1);
    simtime_picosec switch_latency = timeFromUs((uint32_t)0);
    queue_type qt = COMPOSITE;
    std::string tiers_latency;

    UecSrc::_load_balancing_algo = UecSrc::MIXED;

    bool log_sink = false;
    bool log_nic = false;
    bool log_flow_events = true;

    bool log_tor_downqueue = false;
    bool log_tor_upqueue = false;
    bool log_traffic = false;
    bool log_switches = false;
    bool log_queue_usage = false;
    double ecn_thresh = 0.5; // default marking threshold for ECN load balancing

    bool param_ecn_set = false;
    bool ecn = true;
    mem_b ecn_low = 0.2 * queuesize, ecn_high = 0.8 * queuesize;

    bool receiver_driven = true;

    RouteStrategy route_strategy = NOT_SET;
    
    int seed = 13;
    int path_burst = 1;
    int i = 1;
    double pcie_rate = 1.1;

    filename << "logout.dat";
    int end_time = 100000;//in microseconds
    bool force_disable_oversubscribed_cc = false;
    bool enable_accurate_base_rtt = true;

    //unsure how to set this. 
    queue_type snd_type = FAIR_PRIO;

    float ar_sticky_delta = 10;
    FatTreeSwitch::sticky_choices ar_sticky = FatTreeSwitch::PER_PACKET;

    char* tm_file = NULL;
    char* topo_file = NULL;
    //bool disable_fair_decrease = true;
    bool _collect_data = false;
    bool enable_qa_gate = true;
    double fast_increase_scaling_factor = UecSrc::get_fast_increase_scaling_factor();
    double prop_increase_scaling_factor = UecSrc::get_prop_increase_scaling_factor();
    double fair_increase_scaling_factor = UecSrc::get_fair_decrease_scaling_factor();
    double fair_decrease_scaling_factor = UecSrc::get_fair_decrease_scaling_factor();
    double mult_decrease_scaling_factor = UecSrc::get_mult_decrease_scaling_factor();
    bool use_exp_avg_ecn = UecSrc::get_use_exp_avg_ecn();
    bool input_fail_on = false;

    string data_collection_dir = "";
    htsim::DataCollector& data_collector = htsim::DataCollector::get_instance();

    while (i<argc) {
        if (!strcmp(argv[i], "-data_collection_config")) {
            data_collector.InitWithConfig(argv[i+1]);
            cout << "Data collector initialized with config file " << argv[i+1] << endl;
            i++;
        } else if (!strcmp(argv[i], "-data_collection_dir")) {
            data_collection_dir = argv[i+1];
            i++;
        } else if (!strcmp(argv[i],"-o")) {
            filename.str(std::string());
            filename << argv[i+1];
            i++;
        } else if (!strcmp(argv[i],"-conns")) {
            no_of_conns = atoi(argv[i+1]);
            cout << "no_of_conns "<<no_of_conns << endl;
            i++;
        } else if (!strcmp(argv[i],"-end")) {
            end_time = atoi(argv[i+1]);
            cout << "endtime(us) "<< end_time << endl;
            i++;            
        } else if (!strcmp(argv[i],"-nodes")) {
            no_of_nodes = atoi(argv[i+1]);
            cout << "no_of_nodes "<<no_of_nodes << endl;
            i++;
        } else if (!strcmp(argv[i],"-tiers")) {
            tiers = atoi(argv[i+1]);
            cout << "tiers " << tiers << endl;
            assert(tiers == 2 || tiers == 3);
            i++;
        } else if (!strcmp(argv[i],"-num_disabled")) {
            FatTreeSwitch::_num_disable_per_switch = atoi(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-planes")) {
            planes = atoi(argv[i+1]);
            ports = planes;
            cout << "planes " << planes << endl;
            cout << "ports per NIC " << ports << endl;
            assert(planes >= 1 && planes <= 8);
            i++;
        } else if (!strcmp(argv[i], "-fasti_scaling_factor")) {
            fast_increase_scaling_factor = std::stod(argv[i + 1]);
            UecSrc::set_fast_increase_scaling_factor(fast_increase_scaling_factor);
            printf("Fast increase: %f\n", fast_increase_scaling_factor);
            i++;
        } else if (!strcmp(argv[i], "-pi_scaling_factor")) {
            prop_increase_scaling_factor = std::stod(argv[i + 1]);
            UecSrc::set_prop_increase_scaling_factor(prop_increase_scaling_factor);
            printf("Prop increase: %f\n", prop_increase_scaling_factor);
            i++;
        } else if (!strcmp(argv[i], "-pi_scaling_factor")) {
            prop_increase_scaling_factor = std::stod(argv[i + 1]);
            UecSrc::set_prop_increase_scaling_factor(prop_increase_scaling_factor);
            printf("Prop increase: %f\n", prop_increase_scaling_factor);
            i++;
        } else if (!strcmp(argv[i], "-exit_freeze")) {
            CircularBufferREPS<int>::exit_freeze_after = std::stod(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-skip_asy")) {
            FatTreeTopology::skip_asy = true;
        } else if (!strcmp(argv[i],"-mixed_lb_traffic")) {
            UecSrc::_mixed_lb_traffic = true;
        } else if (!strcmp(argv[i],"-mp_flows")) {
            UecSrc::_num_mp_flows = atoi(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-collect_data")) {
            UecSrc::_collect_data = true;
            CompositeQueue::_collect_data = true;
            _collect_data = true;
        } else if (!strcmp(argv[i],"-save_rtt")) {
            UecSrc::_save_rtt = true;
        } else if (!strcmp(argv[i],"-other_location")) {
            FAILURE_GENERATOR->other_loc = true;
        } else if (!strcmp(argv[i],"-connections_mapping")) {
            UecSrc::_connections_mapping = true;
        } else if (!strcmp(argv[i],"-log_link")) {
            FatTreeSwitch::_log_link_utilization = true;
            Pipe::_log_link_utilization = true;
        } else if (!strcmp(argv[i],"-freezeoff")) {
            CircularBufferREPS<int>::repsUseFreezing = false;
        } else if (!strcmp(argv[i],"-switch_stats")) {
            FatTreeSwitch::_log_switch_stats = true;
        } else if (!strcmp(argv[i], "-fd_scaling_factor")) {
            fair_decrease_scaling_factor = std::stod(argv[i + 1]);
            UecSrc::set_fair_decrease_scaling_factor(fair_decrease_scaling_factor);
            printf("Fair decrease: %f\n", fair_decrease_scaling_factor);
            i++;
        } else if (!strcmp(argv[i], "-fd_scaling_factor")) {
            fair_decrease_scaling_factor = std::stod(argv[i + 1]);
            UecSrc::set_fair_decrease_scaling_factor(fair_decrease_scaling_factor);
            printf("Fair decrease: %f\n", fair_decrease_scaling_factor);
            i++;
        } else if (!strcmp(argv[i], "-down_ratio")) {
            FatTreeTopology::_failed_link_ratio = std::stod(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i],"-sender_cc_only")) {
            UecSrc::_sender_based_cc = true;
            UecSrc::_receiver_based_cc = false;
            UecSink::_oversubscribed_cc = false;
            receiver_driven = false;
            cout << "sender based CC enabled ONLY" << endl;
//        } else if (!strcmp(argv[i],"-disable_fd")) {
//            disable_fair_decrease = true;
//            cout << "fair_decrease disabled" << endl;
        } else if (!strcmp(argv[i],"-enable_qa_gate")) {
            enable_qa_gate = true;
        } else if (!strcmp(argv[i],"-enable_fi")) {
            UecSrc::mprdma_fast_recovery = true;
        } else if (!strcmp(argv[i],"-enable_avg_ecn_over_path")) {
            UecSrc::_enable_avg_ecn_over_path = true;
            cout << "enable avg_ecn_over_path algorithm." << endl;            
        } else if (!strcmp(argv[i], "-no_timeouts")) {
            CompositeQueue::_use_timeouts = false;
            UecSrc::_use_timeouts = false;
            Pipe::_use_timeouts = false;
            failuregenerator::_use_timeouts = false;
        } else if (!strcmp(argv[i], "-failures_input")) {
            input_fail_on = true;
            FAILURE_GENERATOR->setInputFile(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i],"-target_q_delay")) {
            UecSrc::_target_Qdelay = timeFromUs(atof(argv[i+1]));
            cout << "target_q_delay" << atof(argv[i+1]) << " us"<< endl;
            i++;
        } else if (!strcmp(argv[i],"-sender_cc_algo")) {
            UecSrc::_sender_based_cc = true;
            
            if (!strcmp(argv[i+1],"dctcp")) 
                UecSrc::_sender_cc_algo = UecSrc::DCTCP;
            else if (!strcmp(argv[i+1],"nscc")) 
                UecSrc::_sender_cc_algo = UecSrc::NSCC;
            else if (!strcmp(argv[i+1],"mprdma")) 
                UecSrc::_sender_cc_algo = UecSrc::MPRDMA_CC;
            else if (!strcmp(argv[i+1],"constant")) 
                UecSrc::_sender_cc_algo = UecSrc::CONSTANT;
            else if (!strcmp(argv[i+1],"smartt"))
                UecSrc::_sender_cc_algo = UecSrc::SMARTT;
            else if (!strcmp(argv[i+1],"smartt_ecn_aimd"))
                UecSrc::_sender_cc_algo = UecSrc::SMARTT_ECN_AIMD;
            else if (!strcmp(argv[i+1],"smartt_ecn_aifd"))
                UecSrc::_sender_cc_algo = UecSrc::SMARTT_ECN_AIFD;
            else if (!strcmp(argv[i+1],"smartt_ecn_fimd"))
                UecSrc::_sender_cc_algo = UecSrc::SMARTT_ECN_FIMD;
            else if (!strcmp(argv[i+1],"smartt_ecn_fifd"))
                UecSrc::_sender_cc_algo = UecSrc::SMARTT_ECN_FIFD;
            else if (!strcmp(argv[i+1],"smartt_rtt"))
                UecSrc::_sender_cc_algo = UecSrc::SMARTT_RTT;
            else {
                cout << "UNKNOWN CC ALGO " << argv[i+1] << endl;
                exit(1);
            }    
            cout << "sender based algo "<< argv[i+1] << endl;
            i++;
        } else if (!strcmp(argv[i],"-save_data_folder")) {
            SAVE_DATA_FOLDER = argv[i + 1];
            i++;
        } else if (!strcmp(argv[i], "-use_wait_to_decrease")) {
            use_exp_avg_ecn = atoi(argv[i + 1]);
            printf("Use wait to decrease: %d\n", use_exp_avg_ecn);
            UecSrc::set_use_exp_avg_ecn(use_exp_avg_ecn);
            i++;
        } else if (!strcmp(argv[i],"-sender_cc")) {
            UecSrc::_sender_based_cc = true;
            UecSink::_oversubscribed_cc = false;
            cout << "sender based CC enabled " << endl;
        }
        else if (!strcmp(argv[i],"-load_balancing_algo")){
            if (!strcmp(argv[i+1], "bitmap")) {
                UecSrc::_load_balancing_algo = UecSrc::BITMAP;
            } 
            else if (!strcmp(argv[i+1], "reps")) {
                UecSrc::_load_balancing_algo = UecSrc::REPS;
            }
            else if (!strcmp(argv[i+1], "oblivious")) {
                UecSrc::_load_balancing_algo = UecSrc::OBLIVIOUS;
            }
            else if (!strcmp(argv[i+1], "mixed")) {
                UecSrc::_load_balancing_algo = UecSrc::MIXED;
            } else if (!strcmp(argv[i+1],"nvidia")){
                UecSrc::_load_balancing_algo = UecSrc::OBLIVIOUS;
                FatTreeSwitch::use_adaptive_roce = true;
            } else if (!strcmp(argv[i+1], "incremental")) {
                UecSrc::_load_balancing_algo = UecSrc::INCREMENTAL;
            } else if (!strcmp(argv[i+1], "plb")) {
                UecSrc::_load_balancing_algo = UecSrc::PLB;
            } else if (!strcmp(argv[i+1], "flowlet")) {
                UecSrc::_load_balancing_algo = UecSrc::FLOWLET;
            } else if (!strcmp(argv[i+1], "mprdma")) {
                UecSrc::_load_balancing_algo = UecSrc::MPRDMA;
            } else if (!strcmp(argv[i+1], "ecmp")) {
                UecSrc::_load_balancing_algo = UecSrc::ECMP;
            } else if (!strcmp(argv[i+1], "mp")) {
                UecSrc::_load_balancing_algo = UecSrc::MP;
            }else if (!strcmp(argv[i+1], "freezing")) {
                UecSrc::_load_balancing_algo = UecSrc::FREEZING;
            } 
            else {
                cout << "Unknown load balancing algorithm of type " << argv[i+1] << ", expecting bitmap, reps or reps2" << endl;
                exit_error(argv[0]);
            }
            cout << "Load balancing algorithm set to  "<< argv[i+1] << endl;
            i++;
        }
        else if (!strcmp(argv[i],"-queue_type")) {
            if (!strcmp(argv[i+1], "composite")) {
                qt = COMPOSITE;
            } 
            else if (!strcmp(argv[i+1], "composite_ecn")) {
                qt = COMPOSITE_ECN;
            }
            else if (!strcmp(argv[i+1], "aeolus")){
                qt = AEOLUS;
            }
            else if (!strcmp(argv[i+1], "aeolus_ecn")){
                qt = AEOLUS_ECN;
            }
            else {
                cout << "Unknown queue type " << argv[i+1] << endl;
                exit_error(argv[0]);
            }
            cout << "queue_type "<< qt << endl;
            i++;
        } else if (!strcmp(argv[i],"-debug")) {
            UecSrc::_debug = true;
        } else if (!strcmp(argv[i],"-host_queue_type")) {
            if (!strcmp(argv[i+1], "swift")) {
                snd_type = SWIFT_SCHEDULER;
            } 
            else if (!strcmp(argv[i+1], "prio")) {
                snd_type = PRIORITY;
            }
            else if (!strcmp(argv[i+1], "fair_prio")) {
                snd_type = FAIR_PRIO;
            }
            else {
                cout << "Unknown host queue type " << argv[i+1] << " expecting one of swift|prio|fair_prio" << endl;
                exit_error(argv[0]);
            }
            cout << "host queue_type "<< snd_type << endl;
            i++;
        } else if (!strcmp(argv[i],"-log")){
            if (!strcmp(argv[i+1], "flow_events")) {
                log_flow_events = true;
            } else if (!strcmp(argv[i+1], "sink")) {
                cout << "logging sinks\n";
                log_sink = true;
            } else if (!strcmp(argv[i+1], "nic")) {
                cout << "logging nics\n";
                log_nic = true;
            } else if (!strcmp(argv[i+1], "tor_downqueue")) {
                cout << "logging tor downqueues\n";
                log_tor_downqueue = true;
            } else if (!strcmp(argv[i+1], "tor_upqueue")) {
                cout << "logging tor upqueues\n";
                log_tor_upqueue = true;
            } else if (!strcmp(argv[i+1], "switch")) {
                cout << "logging total switch queues\n";
                log_switches = true;
            } else if (!strcmp(argv[i+1], "traffic")) {
                cout << "logging traffic\n";
                log_traffic = true;
            } else if (!strcmp(argv[i+1], "queue_usage")) {
                cout << "logging queue usage\n";
                log_queue_usage = true;
            } else {
                exit_error(argv[0]);
            }
            i++;
        } else if (!strcmp(argv[i],"-cwnd")) {
            cwnd = atoi(argv[i+1]);
            cout << "cwnd "<< cwnd << endl;
            i++;
        } else if (!strcmp(argv[i],"-tm")){
            tm_file = argv[i+1];
            cout << "traffic matrix input file: "<< tm_file << endl;
            i++;
        } else if (!strcmp(argv[i],"-topo")){
            topo_file = argv[i+1];
            cout << "FatTree topology input file: "<< topo_file << endl;
            i++;
        } else if (!strcmp(argv[i],"-q")){
            param_queuesize_set = true;
            queuesize = atoi(argv[i+1]);
            cout << "Setting queuesize to " << queuesize << " packets " << endl;
            i++;
        }
        else if (!strcmp(argv[i],"-sack_threshold")){
            UecSink::_bytes_unacked_threshold = atoi(argv[i+1]);
            cout << "Setting receiver SACK bytes threshold to " << UecSink::_bytes_unacked_threshold  << " bytes " << endl;
            i++;            
        }
        else if (!strcmp(argv[i],"-oversubscribed_cc")){
            UecSink::_oversubscribed_cc = true;
            cout << "Using receiver oversubscribed CC " << endl;
        }
        else if (!strcmp(argv[i],"-force_disable_oversubscribed_cc")){
            UecSink::_oversubscribed_cc = false;
            force_disable_oversubscribed_cc = true;
            cout << "Disabling receiver oversubscribed CC even with OS topology" << endl;
        }
        else if (!strcmp(argv[i],"-disable_accurate_base_rtt")){
            enable_accurate_base_rtt = false;
            cout << "Disabling accurate base rtt configuration, each flow takes network wide rtt as the base rtt upper bound." << endl;
        }
        else if (!strcmp(argv[i],"-fastlossrecovery")){
            UecSrc::_enable_fast_loss_recovery = true;
            cout << "Using sender fast loss recovery heuristic " << endl;
        }
        else if (!strcmp(argv[i],"-ecn")){
            // fraction of queuesize, between 0 and 1
            param_ecn_set = true;
            ecn = true;
            ecn_low = atoi(argv[i+1]); 
            ecn_high = atoi(argv[i+2]);
            i+=2;
        } else if (!strcmp(argv[i],"-disable_trim")) {
            disable_trim = true;
            cout << "Trimming disabled, dropping instread." << endl;
            UecSrc::_trim_disbled = true;
        } else if (!strcmp(argv[i],"-trimsize")){
            // size of trimmed packet in bytes
            trimsize = atoi(argv[i+1]);
            cout << "trimmed packet size: " << trimsize << " bytes\n";
            i+=1;
        } else if (!strcmp(argv[i],"-logtime")){
            double log_ms = atof(argv[i+1]);            
            logtime = timeFromMs(log_ms);
            cout << "logtime "<< logtime << " ms" << endl;
            i++;
        } else if (!strcmp(argv[i],"-logtime_us")){
            double log_us = atof(argv[i+1]);            
            logtime = timeFromUs(log_us);
            cout << "logtime "<< log_us << " us" << endl;
            i++;
        } else if (!strcmp(argv[i],"-failed")){
            int num_failed = atoi(argv[i+1]);

            if (num_failed == 42) {
                num_failed = 0;
                printf("Activating Special mode for Micro failures scenario\n");
                CompositeQueue::scenario_micro_failures = true;
            } else {
                FatTreeTopology::set_failed_links(num_failed);
            }

            i++;
        } else if (!strcmp(argv[i],"-linkspeed")){
            // linkspeed specified is in Mbps
            linkspeed = speedFromMbps(atof(argv[i+1]));
            i++;
        } else if (!strcmp(argv[i],"-seed")){
            seed = atoi(argv[i+1]);
            cout << "random seed "<< seed << endl;
            i++;
        } else if (!strcmp(argv[i],"-mtu")){
            packet_size = atoi(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-paths")){
            path_entropy_size = atoi(argv[i+1]);
            cout << "no of paths " << path_entropy_size << endl;
            i++;
        } else if (!strcmp(argv[i],"-path_burst")){
            path_burst = atoi(argv[i+1]);
            cout << "path burst " << path_burst << endl;
            i++;
        } else if (!strcmp(argv[i],"-hop_latency")){
            hop_latency = timeFromUs(atof(argv[i+1]));
            cout << "Hop latency set to " << timeAsUs(hop_latency) << endl;
            i++;
        } else if (!strcmp(argv[i],"-pcie")){
            UecSink::_model_pcie = true;
            pcie_rate = atof(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-switch_latency")){
            switch_latency = timeFromUs(atof(argv[i+1]));
            cout << "Switch latency set to " << timeAsUs(switch_latency) << endl;
            i++;
        } else if (!strcmp(argv[i],"-ar_sticky_delta")){
            ar_sticky_delta = atof(argv[i+1]);
            cout << "Adaptive routing sticky delta " << ar_sticky_delta << "us" << endl;
            i++;
        } else if (!strcmp(argv[i],"-ar_granularity")){
            if (!strcmp(argv[i+1],"packet"))
                ar_sticky = FatTreeSwitch::PER_PACKET;
            else if (!strcmp(argv[i+1],"flow"))
                ar_sticky = FatTreeSwitch::PER_FLOWLET;
            else  {
                cout << "Expecting -ar_granularity packet|flow, found " << argv[i+1] << endl;
                exit(1);
            }   
            i++;
        } else if (!strcmp(argv[i],"-ar_method")){
            if (!strcmp(argv[i+1],"pause")){
                cout << "Adaptive routing based on pause state " << endl;
                FatTreeSwitch::fn = &FatTreeSwitch::compare_pause;
            }
            else if (!strcmp(argv[i+1],"queue")){
                cout << "Adaptive routing based on queue size " << endl;
                FatTreeSwitch::fn = &FatTreeSwitch::compare_queuesize;
            }
            else if (!strcmp(argv[i+1],"bandwidth")){
                cout << "Adaptive routing based on bandwidth utilization " << endl;
                FatTreeSwitch::fn = &FatTreeSwitch::compare_bandwidth;
            }
            else if (!strcmp(argv[i+1],"pqb")){
                cout << "Adaptive routing based on pause, queuesize and bandwidth utilization " << endl;
                FatTreeSwitch::fn = &FatTreeSwitch::compare_pqb;
            }
            else if (!strcmp(argv[i+1],"pq")){
                cout << "Adaptive routing based on pause, queuesize" << endl;
                FatTreeSwitch::fn = &FatTreeSwitch::compare_pq;
            }
            else if (!strcmp(argv[i+1],"pb")){
                cout << "Adaptive routing based on pause, bandwidth utilization" << endl;
                FatTreeSwitch::fn = &FatTreeSwitch::compare_pb;
            }
            else if (!strcmp(argv[i+1],"qb")){
                cout << "Adaptive routing based on queuesize, bandwidth utilization" << endl;
                FatTreeSwitch::fn = &FatTreeSwitch::compare_qb; 
            }
            else {
                cout << "Unknown AR method expecting one of pause, queue, bandwidth, pqb, pq, pb, qb" << endl;
                exit(1);
            }
            i++;
        } else if (!strcmp(argv[i],"-strat")){
            if (!strcmp(argv[i+1], "ecmp_host")) {
                route_strategy = ECMP_FIB;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
            } else if (!strcmp(argv[i+1], "rr_ecmp")) {
                //this is the host route strategy;
                route_strategy = ECMP_FIB_ECN;
                qt = COMPOSITE_ECN_LB;
                //this is the switch route strategy. 
                FatTreeSwitch::set_strategy(FatTreeSwitch::RR_ECMP);
            } else if (!strcmp(argv[i+1], "ecmp_host_ecn")) {
                route_strategy = ECMP_FIB_ECN;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
                qt = COMPOSITE_ECN_LB;
            } else if (!strcmp(argv[i+1], "reactive_ecn")) {
                // Jitu's suggestion for something really simple
                // One path at a time, but switch whenever we get a trim or ecn
                //this is the host route strategy;
                route_strategy = REACTIVE_ECN;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
                qt = COMPOSITE_ECN_LB;
            } else if (!strcmp(argv[i+1], "ecmp_ar")) {
                route_strategy = ECMP_FIB;
                path_entropy_size = 1;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ADAPTIVE_ROUTING);
            } else if (!strcmp(argv[i+1], "ecmp_host_ar")) {
                route_strategy = ECMP_FIB;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP_ADAPTIVE);
                //the stuff below obsolete
                //FatTreeSwitch::set_ar_fraction(atoi(argv[i+2]));
                //cout << "AR fraction: " << atoi(argv[i+2]) << endl;
                //i++;
            } else if (!strcmp(argv[i+1], "ecmp_rr")) {
                // switch round robin
                route_strategy = ECMP_FIB;
                path_entropy_size = 1;
                FatTreeSwitch::set_strategy(FatTreeSwitch::RR);
            }
            i++;
        } else {
            cout << "Unknown parameter " << argv[i] << endl;
            exit_error(argv[0]);
        }
        i++;
    }


    if (input_fail_on) {
        FAILURE_GENERATOR->read_failure_list();
        //FAILURE_GENERATOR->topology = topo[0];
    }

    if (!param_queuesize_set || !param_ecn_set){
        cout << "queuesizes and ecn threshold should be input from the parameters, otherwise, queuesize = BDP of 100Gbps and 12us RTT and ecn_low is 20\% of queuesize and 80\% of queuesize."<< endl;
        //abort(); We should restore to default values here, not abort
    }
    if (!data_collection_dir.empty()) {
        data_collector.setDataDir(data_collection_dir);
        cout << "Data collection dir set as " << data_collection_dir << endl;
    }

    assert(trimsize >= 64 && trimsize <= (uint32_t)packet_size);

    srand(seed);
    srandom(seed);
    cout << "Parsed args\n";
    Packet::set_packet_size(packet_size);

    if (route_strategy==NOT_SET){
        route_strategy = ECMP_FIB;
        FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
    }

    queuesize = memFromPkt(queuesize);

    if (ecn){
        ecn_low = memFromPkt(ecn_low);
        ecn_high = memFromPkt(ecn_high);
        cout << "Setting ECN for queues with size " << queuesize << ", with parameters low " << ecn_low << " high " << ecn_high <<  " enable on tor downlink " << !receiver_driven << endl;
        FatTreeTopology::set_ecn_parameters(true, !receiver_driven, ecn_low,ecn_high);
    }

    if (enable_qa_gate){
        UecSrc::_enable_qa_gate = true;
        cout << "enable quick adapt gate" << endl;            
    }

    // if(disable_fair_decrease){
    //     UecSrc::disableFairDecrease();
    // }

    /*
    UecSink::_oversubscribed_congestion_control = oversubscribed_congestion_control;
    */

    FatTreeSwitch::_ar_sticky = ar_sticky;
    FatTreeSwitch::_sticky_delta = timeFromUs(ar_sticky_delta);
    FatTreeSwitch::_ecn_threshold_fraction = ecn_thresh;
    FatTreeSwitch::_disable_trim = disable_trim;
    FatTreeSwitch::_trim_size = trimsize;

    eventlist.setEndtime(timeFromUs((uint32_t)end_time));

    
    
    switch (route_strategy) {
    case ECMP_FIB_ECN:
    case REACTIVE_ECN:
        if (qt != COMPOSITE_ECN_LB) {
            fprintf(stderr, "Route Strategy is ECMP ECN.  Must use an ECN queue\n");
            exit(1);
        }
        if (ecn_thresh <= 0 || ecn_thresh >= 1) {
            fprintf(stderr, "Route Strategy is ECMP ECN.  ecn_thresh must be between 0 and 1\n");
            exit(1);
        }
        // no break, fall through
    case ECMP_FIB:
        break;
    case NOT_SET:
        fprintf(stderr, "Route Strategy not set.  Use the -strat param.  \nValid values are perm, rand, pull, rg and single\n");
        exit(1);
    default:
        break;
    }

    // prepare the loggers

    cout << "Logging to " << filename.str() << endl;
    //Logfile 
    //Logfile logfile(filename.str(), eventlist);

    cout << "Linkspeed set to " << linkspeed/1000000000 << "Gbps" << endl;
    //logfile.setStartTime(timeFromSec(0));

    UecSinkLoggerSampling* sink_logger = NULL;
    if (log_sink) {
        //sink_logger = new UecSinkLoggerSampling(logtime, eventlist);
        //logfile.addLogger(*sink_logger);
    }
    NicLoggerSampling* nic_logger = NULL;
    if (log_nic) {
        nic_logger = new NicLoggerSampling(logtime, eventlist);
        //logfile.addLogger(*nic_logger);
    }
    TrafficLoggerSimple* traffic_logger = NULL;
    if (log_traffic) {
        traffic_logger = new TrafficLoggerSimple();
        //logfile.addLogger(*traffic_logger);
    }
    FlowEventLoggerSimple* event_logger = NULL;
    if (log_flow_events) {
        event_logger = new FlowEventLoggerSimple();
        //logfile.addLogger(*event_logger);
    }

    //UecSrc::setMinRTO(50000); //increase RTO to avoid spurious retransmits
    UecSrc::_path_entropy_size = path_entropy_size;
    
    UecSrc* uec_src;
    UecSink* uec_snk;

    //Route* routeout, *routein;

    // scanner interval must be less than min RTO
    //UecRtxTimerScanner UecRtxScanner(timeFromUs((uint32_t)9), eventlist);
   
    QueueLoggerFactory *qlf = 0;
    if (log_tor_downqueue || log_tor_upqueue) {
        //qlf = new QueueLoggerFactory(&logfile, QueueLoggerFactory::LOGGER_SAMPLING, eventlist);
        qlf->set_sample_period(timeFromUs(10.0));
    } else if (log_queue_usage) {
        //qlf = new QueueLoggerFactory(&logfile, QueueLoggerFactory::LOGGER_EMPTY, eventlist);
        qlf->set_sample_period(timeFromUs(10.0));
    }

    ConnectionMatrix* conns = new ConnectionMatrix(no_of_nodes);

    if (tm_file){
        cout << "Loading connection matrix from  " << tm_file << endl;

        if (!conns->load(tm_file)){
            cout << "Failed to load connection matrix " << tm_file << endl;
            exit(-1);
        }
    }
    else {
        cout << "Loading connection matrix from  standard input" << endl;        
        conns->load(cin);
    }

    if (conns->N != no_of_nodes && no_of_nodes != 0){
        cout << "Connection matrix number of nodes is " << conns->N << " while I am using " << no_of_nodes << endl;
        exit(-1);
    }

    no_of_nodes = conns->N;

    simtime_picosec network_max_unloaded_rtt = 0;
    // Register Metrics
    htsim::CsvMetric* _global_metric = data_collector.RegisterCsvMetric(
        "globalInfo", {"linkSpeedGbps", "linkDelayNs", "packetSizeBytes", "sackThresholdBytes",
                       "queueSizeBytes", "kMinBytes", "kMaxBytes", "loadBalancingAlgo"});

    simtime_picosec network_rtt = 0;
    vector <FatTreeTopology*> topo;
    topo.resize(planes);
    for (uint32_t p = 0; p < planes; p++) {
        if (topo_file) {
            topo[p] = FatTreeTopology::load(topo_file, qlf, eventlist, queuesize, qt, snd_type);
            tiers_latency = topo[p]->get_tiers_latency();
            no_of_nodes = topo[p]->no_of_nodes();
            /* if (topo[p]->no_of_nodes() != no_of_nodes) {
                cerr << "Mismatch between connection matrix (" << no_of_nodes << " nodes) and topology ("
                     << topo[p]->no_of_nodes() << " nodes)" << endl;
                exit(1);
            } */
        } else {
            FatTreeTopology::set_tiers(tiers);
            topo[p] = new FatTreeTopology(no_of_nodes, linkspeed, queuesize, qlf, 
                                          &eventlist, NULL, qt, hop_latency,
                                          switch_latency,
                                          snd_type);
            tiers_latency = topo[p]->get_tiers_latency();
        }

        if (topo[p]->get_oversubscription_ratio() > 1 && !UecSrc::_sender_based_cc && !force_disable_oversubscribed_cc) {
            UecSink::_oversubscribed_cc = true;
            OversubscribedCC::setOversubscriptionRatio(topo[p]->get_oversubscription_ratio());
            cout << "Using simple receiver oversubscribed CC. Oversubscription ratio is " << topo[p]->get_oversubscription_ratio() << endl;
        } 

        if (log_switches) {
            //topo[p]->add_switch_loggers(logfile, timeFromUs(20.0));
        }

        if (p==0) {
            network_max_unloaded_rtt = 2 * topo[p]->get_diameter_latency() + (Packet::data_packet_size() * 8 / speedAsGbps(linkspeed) * topo[p]->get_diameter() * 1000) + (UecBasePacket::get_ack_size() * 8 / speedAsGbps(linkspeed) * topo[p]->get_diameter() * 1000);
        } else {
            // We only allow identical network rtts for now
            assert(network_max_unloaded_rtt == topo[p]->get_diameter_latency());
        }
    }
    cout << "network_max_unloaded_rtt " << timeAsUs(network_max_unloaded_rtt) << endl;

   //2 priority queues; 3 hops for incast
    UecSrc::_min_rto = timeFromUs(timeAsUs(network_max_unloaded_rtt) + queuesize * 4.0 * 8 * 1000000 / linkspeed);

    cout << "Setting queuesize to " << queuesize << endl;
    cout << "Setting min RTO to " << timeAsUs(UecSrc::_min_rto) << endl;
    
    //handle link failures specified in the connection matrix.
    for (size_t c = 0; c < conns->failures.size(); c++){
        failure* crt = conns->failures.at(c);

        cout << "Adding link failure switch type" << crt->switch_type << " Switch ID " << crt->switch_id << " link ID "  << crt->link_id << endl;
        // xxx we only support failures in plane 0 for now.
        topo[0]->add_failed_link(crt->switch_type,crt->switch_id,crt->link_id);
    }

    // Initialize congestion control algorithms
    if (receiver_driven) {
        // TBD
    } else {
        // UecSrc::parameterScaleToTargetQ();
        UecSrc::initNsccParams(network_max_unloaded_rtt, linkspeed);
    }

    vector<UecPullPacer*> pacers;
    vector<PCIeModel*> pcie_models;
    vector<OversubscribedCC*> oversubscribed_ccs;

    vector<UecNIC*> nics;

    for (size_t ix = 0; ix < no_of_nodes; ix++){
        pacers.push_back(new UecPullPacer(linkspeed, 0.99, UecBasePacket::unquantize(UecSink::_credit_per_pull), eventlist, ports));

        if (UecSink::_model_pcie)
            pcie_models.push_back(new PCIeModel(linkspeed * pcie_rate,UecSrc::_mtu,eventlist,pacers[ix]));

        if (UecSink::_oversubscribed_cc)
            oversubscribed_ccs.push_back(new OversubscribedCC(eventlist,pacers[ix]));

        UecNIC* nic = new UecNIC(ix, eventlist, linkspeed, ports);
        nics.push_back(nic);
        if (log_nic) {
            nic_logger->monitorNic(nic);
        }
    }

    if (!SAVE_DATA_FOLDER.empty())  {
        system((string("mkdir -p ") + SAVE_DATA_FOLDER).c_str());
    }

    if (_collect_data) {
        // build a trailing‐slash‐terminated base directory
        string base_dir = SAVE_DATA_FOLDER;
        if (!base_dir.empty() && base_dir.back() != '/')
            base_dir += '/';

        // remove old data
        system((string("rm -rf ") + base_dir + "psn_over_time/*").c_str());
        system((string("rm -rf ") + base_dir + "port/*").c_str());
        system((string("rm -rf ") + base_dir + "link_util/*").c_str());
        system((string("rm -rf ") + base_dir + "queue/*").c_str());
        system((string("rm -rf ") + base_dir + "queueSize/*").c_str());
        system((string("rm -rf ") + base_dir + "cwd/*").c_str());
        system((string("rm -rf ") + base_dir + "valid_entropies/*").c_str());
        system((string("rm -rf ") + base_dir + "dropped/*").c_str());
        system((string("rm -rf ") + base_dir + "nack/*").c_str());
        system((string("rm -rf ") + base_dir + "ecn/*").c_str());
        system((string("rm -rf ") + base_dir + "rtt/*").c_str());
        system((string("rm -rf ") + base_dir + "new_entropy/*").c_str());
        system((string("rm -rf ") + base_dir + "valid_entropy/*").c_str());
        system((string("rm -rf ") + base_dir + "invalid_entropy/*").c_str());
        system((string("rm -rf ") + base_dir + "list_psn/*").c_str());
        system((string("rm -rf ") + base_dir + "pipe/").c_str());
        system((string("rm -rf ") + base_dir + "switch_drops/*").c_str());
        system((string("rm -rf ") + base_dir + "cable_drops/*").c_str());
        system((string("rm -rf ") + base_dir + "cable_failures/*").c_str());
        system((string("rm -rf ") + base_dir + "switch_failures/*").c_str());
        system((string("rm -rf ") + base_dir + "routing_failed_switch/*").c_str());
        system((string("rm -rf ") + base_dir + "cable_degradations/*").c_str());
        system((string("rm -rf ") + base_dir + "switch_degradations/*").c_str());
        system((string("rm -rf ") + base_dir + "random_packet_drops/*").c_str());
        system((string("rm -rf ") + base_dir + "ratio_over_rtt/*").c_str());
        system((string("rm -rf ") + base_dir + "ratio_over_time/*").c_str());
        system((string("rm -rf ") + base_dir + "psn_over_time/*").c_str());

        // recreate directories
        system((string("mkdir -p ") + base_dir + "switch_drops/").c_str());
        system((string("mkdir -p ") + base_dir + "cable_drops/").c_str());
        system((string("mkdir -p ") + base_dir + "cable_failures/").c_str());
        system((string("mkdir -p ") + base_dir + "switch_failures/").c_str());
        system((string("mkdir -p ") + base_dir + "routing_failed_switch/").c_str());
        system((string("mkdir -p ") + base_dir + "cable_degradations/").c_str());
        system((string("mkdir -p ") + base_dir + "switch_degradations/").c_str());
        system((string("mkdir -p ") + base_dir + "random_packet_drops/").c_str());
        system((string("mkdir -p ") + base_dir + "port/").c_str());
        system((string("mkdir -p ") + base_dir + "link_util/").c_str());
        system((string("mkdir -p ") + base_dir + "queue/").c_str());
        system((string("mkdir -p ") + base_dir + "queueSize/").c_str());
        system((string("mkdir -p ") + base_dir + "cwd/").c_str());
        system((string("mkdir -p ") + base_dir + "valid_entropies/").c_str());
        system((string("mkdir -p ") + base_dir + "dropped/").c_str());
        system((string("mkdir -p ") + base_dir + "nack/").c_str());
        system((string("mkdir -p ") + base_dir + "ecn/").c_str());
        system((string("mkdir -p ") + base_dir + "rtt/").c_str());
        system((string("mkdir -p ") + base_dir + "new_entropy/").c_str());
        system((string("mkdir -p ") + base_dir + "valid_entropy/").c_str());
        system((string("mkdir -p ") + base_dir + "invalid_entropy/").c_str());
        system((string("mkdir -p ") + base_dir + "list_psn/").c_str());
        system((string("mkdir -p ") + base_dir + "ratio_over_rtt/").c_str());
        system((string("mkdir -p ") + base_dir + "ratio_over_time/").c_str());
        system((string("mkdir -p ") + base_dir + "psn_over_time/").c_str());
    } 

    // used just to print out stats data at the end
    list <const Route*> routes;
    
    

    vector<connection*>* all_conns = conns->getAllConnections();
    vector <UecSrc*> uec_srcs;

    map <flowid_t, TriggerTarget*> flowmap;
    if(planes != 1){
        cout << "We are taking the plane 0 to calculate the network rtt; If all the planes have the same tiers, you can remove this check." << endl;
        assert(false);
    }


    mem_b cwnd_b = cwnd*Packet::data_packet_size();

    _global_metric->LogData({std::to_string(speedAsGbps(linkspeed)),
                             tiers_latency,
                             std::to_string(Packet::data_packet_size()),
                             std::to_string(UecSink::_bytes_unacked_threshold),
                             std::to_string(queuesize), std::to_string(ecn_low),
                             std::to_string(ecn_high),
                             std::to_string(UecSrc::_load_balancing_algo)});

    for (size_t c = 0; c < all_conns->size(); c++){
        connection* crt = all_conns->at(c);
        int src = crt->src;
        int dest = crt->dst;
        assert(planes > 0);
        simtime_picosec transmission_delay = (Packet::data_packet_size() * 8 / speedAsGbps(linkspeed) * topo[0]->get_diameter() * 1000) + (UecBasePacket::get_ack_size() * 8 / speedAsGbps(linkspeed) * topo[0]->get_diameter() * 1000);
        simtime_picosec base_rtt_bw_two_points = 2*topo[0]->get_two_point_diameter_latency(src, dest) + transmission_delay;

        //cout << "Connection " << crt->src << "->" <<crt->dst << " starting at " << crt->start << " size " << crt->size << endl;
        cout << "base_rtt_bw_two_points " << timeAsUs(base_rtt_bw_two_points)  << endl;

        uec_src = new UecSrc(traffic_logger, eventlist, *nics.at(src), ports);

        // If cwnd is 0 initXXcc will set a sensible default value 
        if (receiver_driven) {
            // uec_src->setCwnd(cwnd*Packet::data_packet_size());
            // uec_src->setMaxWnd(cwnd*Packet::data_packet_size());

            if (enable_accurate_base_rtt) {
            	uec_src->initRccc(cwnd_b, base_rtt_bw_two_points);
        	} else {
            	uec_src->initRccc(cwnd_b, network_max_unloaded_rtt);
        	}
        } else {
            if (enable_accurate_base_rtt) {
            	uec_src->initNscc(cwnd_b, base_rtt_bw_two_points);
        	} else {
            	uec_src->initNscc(cwnd_b, network_max_unloaded_rtt);
        	}
        }
        //uec_srcs.push_back(uec_src);
        uec_src->setSrc(src);
        uec_src->setDst(dest);

        uec_src->from = src;
        uec_src->to = dest;

        

        if (log_flow_events) {
            uec_src->logFlowEvents(*event_logger);
        }
        
        if (receiver_driven)
            uec_snk = new UecSink(NULL,pacers[dest],*nics.at(dest), ports);
        else //each connection has its own pacer, so receiver driven mode does not kick in! 
            uec_snk = new UecSink(NULL,linkspeed,1.1,UecBasePacket::unquantize(UecSink::_credit_per_pull),eventlist,*nics.at(dest), ports);

        uec_src->setName("Uec_" + ntoa(src) + "_" + ntoa(dest));
        //logfile.writeName(*uec_src);
        uec_snk->setSrc(src);
        uec_snk->from = src;
        uec_snk->to = dest;

        if (input_fail_on) {
            FAILURE_GENERATOR->addSrc(uec_src);
            FAILURE_GENERATOR->addDst(uec_snk);
        }
        

        if (UecSink::_model_pcie){
            uec_snk->setPCIeModel(pcie_models[dest]);
        }
                        
        if (UecSink::_oversubscribed_cc){
            uec_snk->setOversubscribedCC(oversubscribed_ccs[dest]);
        }

        ((DataReceiver*)uec_snk)->setName("Uec_sink_" + ntoa(src) + "_" + ntoa(dest));
        //logfile.writeName(*(DataReceiver*)uec_snk);

        if (crt->flowid) {
            uec_src->setFlowId(crt->flowid);
            uec_snk->setFlowId(crt->flowid);

            uec_src->setHashId(src, dest, crt->flowid);
            uec_snk->setHashId(src, dest, crt->flowid);
            assert(flowmap.find(crt->flowid) == flowmap.end()); // don't have dups
            flowmap[crt->flowid] = uec_src;
        }
                        
        if (crt->size>0){
            uec_src->setFlowsize(crt->size);
        }

        if (crt->trigger) {
            Trigger* trig = conns->getTrigger(crt->trigger, eventlist);
            trig->add_target(*uec_src);
        }
        if (crt->send_done_trigger) {
            Trigger* trig = conns->getTrigger(crt->send_done_trigger, eventlist);
            uec_src->setEndTrigger(*trig);
        }


        if (crt->recv_done_trigger) {
            Trigger* trig = conns->getTrigger(crt->recv_done_trigger, eventlist);
            uec_snk->setEndTrigger(*trig);
        }

        //uec_snk->set_priority(crt->priority);
                        
        //UecRtxScanner.registerUec(*UecSrc);
        for (uint32_t p = 0; p < planes; p++) {
            switch (route_strategy) {
            case ECMP_FIB:
            case ECMP_FIB_ECN:
            case REACTIVE_ECN:
                {
                    Route* srctotor = new Route();
                    srctotor->push_back(topo[p]->queues_ns_nlp[src][topo[p]->HOST_POD_SWITCH(src)][0]);
                    srctotor->push_back(topo[p]->pipes_ns_nlp[src][topo[p]->HOST_POD_SWITCH(src)][0]);
                    srctotor->push_back(topo[p]->queues_ns_nlp[src][topo[p]->HOST_POD_SWITCH(src)][0]->getRemoteEndpoint());

                    Route* dsttotor = new Route();
                    dsttotor->push_back(topo[p]->queues_ns_nlp[dest][topo[p]->HOST_POD_SWITCH(dest)][0]);
                    dsttotor->push_back(topo[p]->pipes_ns_nlp[dest][topo[p]->HOST_POD_SWITCH(dest)][0]);
                    dsttotor->push_back(topo[p]->queues_ns_nlp[dest][topo[p]->HOST_POD_SWITCH(dest)][0]->getRemoteEndpoint());

                    uec_src->connectPort(p, *srctotor, *dsttotor, *uec_snk, crt->start);
                    //uec_src->setPaths(path_entropy_size);
                    //uec_snk->setPaths(path_entropy_size);

                    //register src and snk to receive packets from their respective TORs. 
                    assert(topo[p]->switches_lp[topo[p]->HOST_POD_SWITCH(src)]);
                    assert(topo[p]->switches_lp[topo[p]->HOST_POD_SWITCH(src)]);
                    topo[p]->switches_lp[topo[p]->HOST_POD_SWITCH(src)]->addHostPort(src,uec_snk->flowId(),uec_src->getPort(p));
                    topo[p]->switches_lp[topo[p]->HOST_POD_SWITCH(dest)]->addHostPort(dest,uec_src->flowId(),uec_snk->getPort(p));
                    break;
                }
            default:
                abort();
            }
        }

        // set up the triggers
        // xxx

        if (log_sink) {
            sink_logger->monitorSink(uec_snk);
        }
    }

    //Logged::dump_idmap();
    // Record the setup
    int pktsize = Packet::data_packet_size();
    //logfile.write("# pktsize=" + ntoa(pktsize) + " bytes");
    //logfile.write("# hostnicrate = " + ntoa(linkspeed/1000000) + " Mbps");
    //logfile.write("# corelinkrate = " + ntoa(HOST_NIC*CORE_TO_HOST) + " pkt/sec");
    //logfile.write("# buffer = " + ntoa((double) (queues_na_ni[0][1]->_maxsize) / ((double) pktsize)) + " pkt");
    
    // GO!
    cout << "Starting simulation" << endl;
    while (eventlist.doNextEvent()) {
    }

    cout << "Done" << endl;
    int new_pkts = 0, rtx_pkts = 0, bounce_pkts = 0, rts_pkts = 0, ack_pkts = 0;
    for (size_t ix = 0; ix < uec_srcs.size(); ix++) {
        new_pkts += uec_srcs[ix]->_new_packets_sent;
        rtx_pkts += uec_srcs[ix]->_rtx_packets_sent;
        rts_pkts += uec_srcs[ix]->_rts_packets_sent;
        bounce_pkts += uec_srcs[ix]->_bounces_received;
        ack_pkts += uec_srcs[ix]->_acks_received;
    }
    cout << "New: " << new_pkts << " Rtx: " << rtx_pkts << " RTS: " << rts_pkts << " Bounced: " << bounce_pkts << " ACKs: " << ack_pkts << endl;
    if (input_fail_on) {
        FAILURE_GENERATOR->createLoggingData();
        FAILURE_GENERATOR->save_failure_list();

    }
    if (FatTreeSwitch::_log_switch_stats) {
        for (int i = 0; i < topo[0]->switches_lp.size(); i++) {
            if (topo[0]->switches_lp[i]->_up_ports_used.size() == 0 || topo[0]->switches_lp[i]->nodename() != "Switch_LowerPod_0") {
                continue;
            }
            printf("Switch %s\n", topo[0]->switches_lp[i]->nodename().c_str());
            for (int j = 0; j < topo[0]->switches_lp[i]->_up_ports_used.size(); j++) {
                printf("Port: %s\n", topo[0]->switches_lp[i]->_up_ports_used[j].c_str());
            }
        }
    }  
}

