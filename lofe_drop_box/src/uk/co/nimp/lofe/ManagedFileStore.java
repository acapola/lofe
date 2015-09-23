package uk.co.nimp.lofe;

import javafx.util.Pair;

import java.util.List;

/**
 * Created by seb on 9/13/2015.
 */
public interface ManagedFileStore {

    boolean isLockable();

    /**
     * if isLockable return false, lock must always return false.
     * @return
     */
    boolean lock();
    void unlock();

    /**
     * Recover file info from file info record
     * @param path
     * @return
     */
    FileInfo getLastStoredFileInfo(String path);

    /**
     * Conpute file info from actual file
     * @param path
     * @return
     */
    FileInfo getFileInfo(String path);

    List<FileInfo> getNew();
    List<FileInfo> getRemoved();
    List<Pair<FileInfo, FileInfo>> getMoved();
    List<FileInfo> getChanged();

    void writeFileInfoRecord();

}
