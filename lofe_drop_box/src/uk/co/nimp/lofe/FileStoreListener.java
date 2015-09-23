package uk.co.nimp.lofe;

import java.util.List;

/**
 * Created by seb on 9/13/2015.
 */
public interface FileStoreListener {
    void onFileStoreEvents(List<FileStoreEvent> events);
}
