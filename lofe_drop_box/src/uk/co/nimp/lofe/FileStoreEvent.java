package uk.co.nimp.lofe;

/**
 * Created by seb on 9/13/2015.
 */
public class FileStoreEvent {
    public enum EventType {
        DELETE, //file or directory deleted
        MOVE,   //file or directory moved
        CHANGE; //file created or updated
    }
    final EventType type;
    final String target;
    final String moveSrc;

    protected FileStoreEvent(EventType type, String target, String moveSrc) {
        this.type = type;
        this.target = target;
        this.moveSrc = moveSrc;
    }

    static FileStoreEvent newDeleteEvent(String target){
        return new FileStoreEvent(EventType.DELETE, target,null);
    }

    static FileStoreEvent newChangeEvent(String target){
        return new FileStoreEvent(EventType.CHANGE, target,null);
    }

    static FileStoreEvent newMoveEvent(String src, String target){
        return new FileStoreEvent(EventType.MOVE, target,src);
    }

    @Override
    public String toString() {
        String out;
        if(EventType.MOVE==type){
            out = "MOVE "+moveSrc+" to "+target;
        }else{
            out = type+" "+target;
        }
        return out;
    }
}
