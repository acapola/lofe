package uk.co.nimp.lofe;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.List;

/**
 * Created by seb on 9/13/2015.
 */
public class NativeFileStore extends FileStore {

    static NativeFileStore single = new NativeFileStore();

    private NativeFileStore() {
        pathSeperator = File.separator;
    }

    static NativeFileStore getFileStore(){
        return single;
    }

    static FileStoreEntry newEntry(String path){
        return new FileStoreEntry(single,path);
    }

    FileStoreEntry normalize(String path){
        String normalizedPath = (new File(path)).toPath().normalize().toString();
        return newEntry(normalizedPath);
    }

    @Override
    String toString(String path) {
        return path;
    }

    @Override
    List<FileStoreEntry> listFiles(String dir) {
        File[] files = (new File(dir)).listFiles();
        ArrayList<FileStoreEntry> out = new ArrayList<>();
        try {
            for (File f : files) {
                if(f.isFile())
                    out.add(new FileStoreEntry(this, f.getCanonicalPath()));
            }
        }catch (Throwable e){
            throw new RuntimeException();
        }
        return out;
    }

    @Override
    List<FileStoreEntry> listDirectories(String dir) {
        File[] files = (new File(dir)).listFiles();
        ArrayList<FileStoreEntry> out = new ArrayList<>();
        try {
            for (File f : files) {
                if(f.isDirectory())
                    out.add(new FileStoreEntry(this, f.getCanonicalPath()));
            }
        }catch (Throwable e){
            throw new RuntimeException();
        }
        return out;
    }

    @Override
    boolean isDirectory(String dir) {
        return (new File(dir)).isDirectory();
    }

    @Override
    boolean exists(String path) {
        return (new File(path)).exists();
    }

    @Override
    String getName(String path) {
        return (new File(path)).getName();
    }

    @Override
    boolean isDifferent(String path, File nativePath) {
        boolean out=false;
        try {
            out = filesDifferent((new File(path)),nativePath);
        } catch (IOException e) {
            e.printStackTrace();
        }
        return out;
    }

    @Override
    boolean isDifferent(String path, String otherPath) {
        boolean out=false;
        try {
            out = filesDifferent((new File(path)),(new File(otherPath)));
        } catch (IOException e) {
            e.printStackTrace();
        }
        return out;
    }

    @Override
    long length(String path) {
        return (new File(path)).length();
    }

    @Override
    long lastModified(String path) {
        return (new File(path)).lastModified();
    }

    @Override
    void checkinFile(File src, String dst) {
        try {
            Files.copy(src.toPath(), (new File(dst)).toPath(), null);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    void checkoutFile(String src, File dst) {
        try {
            Files.copy((new File(src)).toPath(),dst.toPath(), null);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    void move(String src, String dst) {
        try {
            Files.move((new File(src)).toPath(), (new File(dst)).toPath(), null);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    void copy(String src, String dst) {
        try {
            Files.copy((new File(src)).toPath(),(new File(dst)).toPath(), null);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    void delete(String target) {
        (new File(target)).delete();
    }

    @Override
    void startMonitoring() {

    }

    @Override
    void stopMonitoring() {

    }

    static boolean filesDifferent(File a, File b) throws IOException {
        long aLen = a.length();
        if(aLen!=b.length()) return true;
        boolean different=false;
        BufferedInputStream as = new BufferedInputStream(new FileInputStream(a));
        BufferedInputStream bs = new BufferedInputStream(new FileInputStream(b));
        for(long i=0;i<aLen;i++){
            if(as.read()!=bs.read()){
                different=true;
                break;
            };
        }
        as.close();
        bs.close();
        return different;
    }
}
