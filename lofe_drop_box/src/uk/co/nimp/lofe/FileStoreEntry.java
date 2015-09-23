package uk.co.nimp.lofe;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.util.List;

/**
 * Created by seb on 9/13/2015.
 */
public class FileStoreEntry {
    final FileStore fs;
    final String path;

    public FileStoreEntry(FileStore fs, String path) {
        this.fs = fs;
        this.path = path;
    }

    String getName(){
        return fs.getName(path);
    }

    boolean exists(){
        return fs.exists(path);
    }

    boolean isDirectory(){
        return fs.isDirectory(path);
    }

    List<FileStoreEntry> listFiles(){
        return fs.listFiles(path);
    }

    List<FileStoreEntry> listDirectories(){
        return fs.listDirectories(path);
    }

    long length(){
        return fs.length(path);
    }
    long lastModified(){
        return fs.lastModified(path);
    }

    FileStoreEntry child(String name){
        return new FileStoreEntry(fs,fs.join(path,name));
    }

    boolean isDifferent(File f){
        return fs.isDifferent(path, f);
    }

    boolean isDifferent(FileStoreEntry f){
        return fs.isDifferent(path,f.path);
    }

    void copyTo(FileStoreEntry dst){
        if(dst.fs==this.fs) fs.copy(this.path,dst.path);
        else{
            if(fs.hasNativeFile()) dst.fs.checkin(fs.getNativeFile(path), dst.path);
            else{
                try {
                    File tmp;
                    if(isDirectory()){
                        tmp = Files.createTempDirectory("FileStoreEntry",null).toFile();
                    }else{
                        tmp = Files.createTempFile("FileStoreEntry", null).toFile();
                    }
                    fs.checkout(path, tmp);
                    dst.fs.checkin(tmp, dst.path);
                    tmp.delete();
                } catch (IOException e) {
                    throw new RuntimeException(e);
                }
            }
        }
    }

    @Override
    public String toString() {
        return fs.toString(path);
    }

    FileStoreEntry normalize(){
        return fs.normalize(path);
    }
}
