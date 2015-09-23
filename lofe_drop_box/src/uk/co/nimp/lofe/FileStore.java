package uk.co.nimp.lofe;

import java.io.File;
import java.util.HashSet;
import java.util.List;

/**
 * Created by seb on 9/13/2015.
 */
public abstract class FileStore {

    String pathSeperator = "/";
    HashSet<FileStoreListener> listeners = new HashSet<>();

    List<FileStoreEntry> listAllEntries(String dir){
        List<FileStoreEntry> out = listDirectories(dir);
        out.addAll(listFiles(dir));
        return out;
    }

    boolean hasNativeFile() {
        return true;
    }

    File getNativeFile(String path){
        return (new File(path));
    }

    abstract FileStoreEntry normalize(String path);

    abstract String toString(String path);

    abstract long lastModified(String path);

    abstract List<FileStoreEntry> listFiles(String dir);

    abstract List<FileStoreEntry> listDirectories(String dir);

    /**
     * Tests whether <code>dir</code> is a directory
     * @param dir the path to test
     * @return true if <code>dir</code> is indeed an existing directory
     * @thows FileNotFoundException if <code>dir</code> does not exist (not a directory nor a file)
     */
    abstract boolean isDirectory(String dir);

    abstract boolean exists(String path);

    abstract boolean isDifferent(String path, File nativePath);

    abstract boolean isDifferent(String path, String otherPath);

    abstract long length(String path);

    String getName(String path){
        int pos = path.lastIndexOf(pathSeperator);
        if(-1==pos) return path;
        else return path.substring(pos);
    }

    String join(String parent,String child){
        if(parent.endsWith(pathSeperator)) return parent+child;
        else return parent+pathSeperator+child;
    }

    /**
     * Copy a native file to the file store
     * @param src
     * @param dst
     */
    abstract void checkinFile(File src, String dst);
    void checkinFile(File src, FileStoreEntry dst){
        //if(dst.fs!=this) throw new RuntimeException();
        checkinFile(src,dst.path);
    }

    /**
     * Copy a file from the file store to the native file system
     * @param src
     * @param dst
     */
    abstract void checkoutFile(String src, File dst);

    void checkin(File src, String dst){
        checkinFile(src,dst);
    }

    /**
     * Copy a directory or a file from the file store to the native file system
     * @param src
     * @param dst
     */
    void checkout(String src, File dst){
        try {
            if (isDirectory(src)) {
                if (!dst.isDirectory())
                    throw new RuntimeException("Cannot copy directory " + src + " into file " + dst.getCanonicalPath());
                List<FileStoreEntry> files = listFiles(src);
                for (FileStoreEntry f : files) {
                    File dstFile = new File(dst, f.getName());
                    checkoutFile(f.path, dstFile);
                }
                List<FileStoreEntry> directories = listDirectories(src);
                for (FileStoreEntry f : directories) {
                    File dstFile = new File(dst, f.getName());
                    checkout(f.path, dstFile);
                }
            } else {
                if (dst.isDirectory())
                    throw new RuntimeException("Cannot copy file " + src + " as " + dst.getCanonicalPath()+ ". This is an existing directory.");
                checkoutFile(src, dst);
            }
        }catch(Throwable e){
            throw new RuntimeException(e);
        }
    }

    /**
     * Move a file or directory within the file store
     * @param src
     * @param dst
     */
    abstract void move(String src, String dst);

    /**
     * Copy a file or directory within the file store
     * @param src
     * @param dst
     */
    abstract void copy(String src, String dst);

    /**
     * Delete a file or directory from the file store
     * @param target
     */
    abstract void delete(String target);

    void registerListener(FileStoreListener listener){
        listeners.add(listener);
    }

    abstract void startMonitoring();
    abstract void stopMonitoring();
}
