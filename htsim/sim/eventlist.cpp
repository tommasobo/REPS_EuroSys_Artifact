// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        

#include "eventlist.h"

#include <memory>
#include "trigger.h"

std::unique_ptr<EventList> EventList::_theEventList = nullptr;


EventList& 
EventList::getTheEventList()
{
    if (!_theEventList) {
            _theEventList.reset(new EventList());
        }
        return *_theEventList;
}

void EventList::resetTheEventListForTesting()
{
    _theEventList.reset(nullptr);
}

void
EventList::setEndtime(simtime_picosec endtime)
{
    _endtime = endtime;
}

bool
EventList::doNextEvent() 
{
    // triggers happen immediately - no time passes; no guarantee that
    // they happen in any particular order (don't assume FIFO or LIFO).
    if (!_pending_triggers.empty()) {
        TriggerTarget *target = _pending_triggers.back();
        _pending_triggers.pop_back();
        target->activate();
        return true;
    }
    
    if (_pendingsources.empty())
        return false;
    
    simtime_picosec nexteventtime = _pendingsources.begin()->first;
    EventSource* nextsource = _pendingsources.begin()->second;
    _pendingsources.erase(_pendingsources.begin());
    assert(nexteventtime >= _lasteventtime);
    GLOBAL_TIME = nexteventtime;
    _lasteventtime = nexteventtime; // set this before calling doNextEvent, so that this::now() is accurate
    nextsource->doNextEvent();
    return true;
}


void 
EventList::sourceIsPending(EventSource &src, simtime_picosec when) 
{
    assert(when>=now());
    if (_endtime==0 || when<_endtime)
        _pendingsources.insert(make_pair(when,&src));
}

EventList::Handle
EventList::sourceIsPendingGetHandle(EventSource &src, simtime_picosec when) 
{
    assert(when>=now());
    if (_endtime==0 || when<_endtime) {
        EventList::Handle handle =_pendingsources.insert(make_pair(when,&src));
        return handle;
    }
    return _pendingsources.end();
}

simtime_picosec EventList::now() 
{
    return _lasteventtime;
}

EventList::Handle EventList::nullHandle() 
{
    return _pendingsources.end();
}

void EventList::sourceIsPendingRel(EventSource &src, simtime_picosec timefromnow) 
{
    sourceIsPending(src, now()+timefromnow);
}

void
EventList::triggerIsPending(TriggerTarget &target) {
    _pending_triggers.push_back(&target);
}

void 
EventList::cancelPendingSource(EventSource &src) {
    pendingsources_t::iterator i = _pendingsources.begin();
    while (i != _pendingsources.end()) {
        if (i->second == &src) {
            _pendingsources.erase(i);
            return;
        }
        i++;
    }
}

void 
EventList::cancelPendingSourceByTime(EventSource &src, simtime_picosec when) {
    // fast cancellation of a timer - the timer MUST exist
    // this should normally be fast, except if we have a lot of events with exactly the same time value

    auto range = _pendingsources.equal_range(when);

    for (auto i = range.first; i != range.second; ++i) {
        if (i->second == &src) {
            _pendingsources.erase(i);
            return;
        }
    }
    abort();
}


void EventList::cancelPendingSourceByHandle(EventSource &src, EventList::Handle handle) {
    // If we're cancelling timers often, cancel them by handle.  But
    // be careful - cancelling a handle that has already been
    // cancelled or has already expired is undefined behaviour
    assert(handle->second == &src);
    assert(handle != _pendingsources.end());
    assert(handle->first >= now());
    
    _pendingsources.erase(handle);
}

void 
EventList::reschedulePendingSource(EventSource &src, simtime_picosec when) {
    cancelPendingSource(src);
    sourceIsPending(src, when);
}

EventSource::EventSource(const string& name) : EventSource(EventList::getTheEventList(), name) 
{
}
