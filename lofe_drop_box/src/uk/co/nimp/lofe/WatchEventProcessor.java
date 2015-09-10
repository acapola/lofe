package uk.co.nimp.lofe;

import java.nio.file.Path;
import java.nio.file.WatchEvent;
import java.util.HashMap;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Created by seb on 9/8/2015.
 */
public abstract class WatchEventProcessor implements Runnable {

    public final Object syncObj = new Object();//to wake up the thread immediately when a new event comes in

    ConcurrentHashMap<Long,PathWatchEvent> events = new ConcurrentHashMap<Long, PathWatchEvent>();//thread safe


    long createdEvents=-1;//monotonic counter incremented by the event producer
    long firstEventNotConsumed=0;//monotonic counter incremented by this thread

    long getMaxEventIndex(){
        return createdEvents-firstEventNotConsumed;
    }

    PathWatchEvent get(long index){
        if(index<0) throw new RuntimeException();
        long absoluteIndex = firstEventNotConsumed + index;
        if(absoluteIndex>createdEvents) throw new RuntimeException();
        PathWatchEvent out = events.get(absoluteIndex);
        return out;
    }

    PathWatchEvent consume(long index) {
        if(index<0) throw new RuntimeException();
        long absoluteIndex = firstEventNotConsumed + index;
        if(absoluteIndex>createdEvents) throw new RuntimeException();
        PathWatchEvent out = events.get(absoluteIndex);
        events.remove(absoluteIndex);
        while (!events.containsKey(firstEventNotConsumed)) {
            firstEventNotConsumed++;
            if (firstEventNotConsumed > createdEvents) break;
        }
        return out;
    }

    PathWatchEvent consume(){
        return consume(0);
    }

    public void processEvent(PathWatchEvent event) {
        events.put(++createdEvents, event);
        synchronized(syncObj) {
            syncObj.notify();
        }
    }
}
