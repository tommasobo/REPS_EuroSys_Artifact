// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef EVENTLIST_H
#define EVENTLIST_H

#include <map>
#include <memory>
#include <sys/time.h>
#include "config.h"
#include "loggertypes.h"

class EventList;
class TriggerTarget;
namespace htsim {
    class MetricTest;
}

class EventSource : public Logged {
public:
    EventSource(EventList& eventlist, const string& name) : Logged(name), _eventlist(eventlist) {};
    EventSource(const string& name);
    virtual ~EventSource() {};
    virtual void doNextEvent() = 0;
    inline EventList& eventlist() const {return _eventlist;}
protected:
    EventList& _eventlist;
};

class EventList {
public:
    typedef multimap <simtime_picosec, EventSource*>::iterator Handle;
    void setEndtime(simtime_picosec endtime); // end simulation at endtime (rather than forever)
    bool doNextEvent(); // returns true if it did anything, false if there's nothing to do
    void sourceIsPending(EventSource &src, simtime_picosec when);
    Handle sourceIsPendingGetHandle(EventSource &src, simtime_picosec when);
    void sourceIsPendingRel(EventSource &src, simtime_picosec timefromnow);
    void cancelPendingSource(EventSource &src);
    // optimized cancel, if we know the expiry time
    void cancelPendingSourceByTime(EventSource &src, simtime_picosec when);   
    // optimized cancel by handle - be careful to ensure handle is still valid
    void cancelPendingSourceByHandle(EventSource &src, Handle handle);       
    void reschedulePendingSource(EventSource &src, simtime_picosec when);
    void triggerIsPending(TriggerTarget &target);
    simtime_picosec now();
    Handle nullHandle();

    static EventList& getTheEventList();
    EventList(const EventList&)      = delete;  // disable Copy Constructor
    void operator=(const EventList&) = delete;  // disable Assign Constructor

private:
    typedef multimap <simtime_picosec, EventSource*> pendingsources_t;

    // Private constructor to enforce singleton pattern. The only way to get an instance of 
    // EventList is through getTheEventList().
    EventList() {
        _endtime = 0;
        _lasteventtime = 0;
    } 
    // Reset the singleton instance of EventList. Declared private to only be used for testing.
    static void resetTheEventListForTesting();

    simtime_picosec _endtime;
    simtime_picosec _lasteventtime;
    pendingsources_t _pendingsources;
    vector <TriggerTarget*> _pending_triggers;
    // Singleton instance of EventList.
    static std::unique_ptr<EventList> _theEventList;

    friend class htsim::MetricTest;
};

#endif
