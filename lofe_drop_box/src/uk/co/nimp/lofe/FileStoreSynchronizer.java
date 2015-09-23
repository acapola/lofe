package uk.co.nimp.lofe;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.file.FileVisitOption;
import java.nio.file.Files;
import java.util.EnumSet;
import java.util.HashSet;
import java.util.List;

/**
 * Created by seb on 9/12/2015.
 */
public class FileStoreSynchronizer {
    final FileStoreEntry aDir;
    final FileStoreEntry bDir;
    final boolean aReadOnly;
    final boolean bReadOnly;

    public FileStoreSynchronizer(FileStoreEntry aDir, FileStoreEntry bDir, boolean aReadOnly, boolean bReadOnly) {
        if(!aDir.isDirectory()) throw new RuntimeException();
        if(!bDir.isDirectory()) throw new RuntimeException();
        this.aDir = aDir;
        this.bDir = bDir;
        this.bReadOnly = bReadOnly;
        this.aReadOnly = aReadOnly;
    }

    /**
     * take the latest version of each file, based on file system timestamp
     */
    public void syncToLatest(boolean dryRun, boolean verbose) throws IOException {
        syncToLatest(aDir, bDir, aReadOnly, bReadOnly, dryRun, verbose);
    }

    public static void syncToLatest(FileStoreEntry aDir, FileStoreEntry bDir, boolean aReadOnly, boolean bReadOnly, boolean dryRun, boolean verbose) throws IOException {
        List<FileStoreEntry> aDirs = aDir.listDirectories();
        List<FileStoreEntry> aFiles = aDir.listFiles();
        List<FileStoreEntry> bDirs = bDir.listDirectories();
        List<FileStoreEntry> bFiles = bDir.listFiles();
        HashSet<FileStoreEntry> bDirectoriesRemaining = new HashSet<>();
        for(FileStoreEntry bf:bDirs) bDirectoriesRemaining.add(bf);
        HashSet<FileStoreEntry> bFilesRemaining = new HashSet<>();
        for(FileStoreEntry bf:bFiles) bFilesRemaining.add(bf);
        for(FileStoreEntry af:aDirs){
            FileStoreEntry bf = bDir.child(af.getName());
            bDirectoriesRemaining.remove(bf);
            syncDirectoryToLatest(af, bf, aReadOnly,bReadOnly,dryRun, verbose);
        }
        for(FileStoreEntry af:aFiles){
            FileStoreEntry bf = bDir.child(af.getName());
            bFilesRemaining.remove(bf);
            syncFileToLatest(af, bf, aReadOnly, bReadOnly, dryRun, verbose);
        }
        if(!aReadOnly) {
            for (FileStoreEntry bf : bDirectoriesRemaining) {
                FileStoreEntry af = aDir.child(bf.getName());
                syncDirectoryToLatest(bf, af, bReadOnly, aReadOnly, dryRun, verbose);
            }
            for (FileStoreEntry bf : bFilesRemaining) {
                FileStoreEntry af = aDir.child(bf.getName());
                syncFileToLatest(bf, af, bReadOnly, aReadOnly, dryRun, verbose);
            }
        }
    }

    static void syncDirectoryToLatest(FileStoreEntry a, FileStoreEntry b, boolean aReadOnly, boolean bReadOnly, boolean dryRun, boolean verbose) throws IOException {
        if(verbose) System.out.println("VERBOSE: Comparing directories " + a + " and " + b);
        if(b.exists()) {
            if (!b.isDirectory()) {
                throw new RuntimeException("Directory " + a + " is a file in second file system: " + b);
            } else {
                syncToLatest(a,b,aReadOnly,bReadOnly,dryRun, verbose);
            }
        } else if(!bReadOnly) {//copy a to b
            directoryCopy(a, b, dryRun, verbose);
        }
    }

    static void syncFileToLatest(FileStoreEntry a, FileStoreEntry b, boolean aReadOnly, boolean bReadOnly, boolean dryRun,boolean verbose) throws IOException {
        if(verbose) System.out.println("VERBOSE: Comparing files "+a+" and "+b);
        if(b.exists()) {
            if (b.isDirectory()) {
                throw new RuntimeException("File " + a + " is a directory in second file system: " + b);
            } else {
                long aTime = a.lastModified();
                long bTime = b.lastModified();
                if(aTime==bTime){
                    if(verbose) System.out.println("VERBOSE: files with identical modification time, skip, "+a.toString()+", "+b.toString());
                }else {
                    if (bTime > aTime) if (!aReadOnly) fileCopy(b, a, dryRun, verbose);
                    else if (!bReadOnly) fileCopy(a, b, dryRun, verbose);
                }
            }
        } else if(!bReadOnly) {//copy a to b
            fileCopy(a,b,dryRun,verbose);
        }
    }

    static void directoryCopy(FileStoreEntry a, FileStoreEntry b, boolean dryRun, boolean verbose){
        List<FileStoreEntry> aDirs = a.listDirectories();
        for(FileStoreEntry af:aDirs){
            FileStoreEntry bf = b.child(af.getName());
            directoryCopy(af, bf, dryRun, verbose);
        }
        List<FileStoreEntry> aFiles = a.listFiles();
        for(FileStoreEntry af:aFiles){
            FileStoreEntry bf = b.child(af.getName());
            fileCopy(af, bf, dryRun, verbose);
        }
    }

    static void fileCopy(FileStoreEntry a, FileStoreEntry b, boolean dryRun, boolean verbose){
        if(dryRun) System.out.println("DRYRUN: copy file "+a.toString()+" to "+b.toString());
        else if(verbose)  System.out.println("VERBOSE: copy file "+a.toString()+" to "+b.toString());
        if(!dryRun)a.copyTo(b);
    }

    public static void showHelp(){
        System.out.println("Directory synchronization tool");
        System.out.println("Synchronize this directory with the latest version of each file");
        System.out.println("between two directories");
        System.out.println("\nusage:");
        System.out.println("sync_to_latest [options] [directory1] <directory2>");
        System.out.println("if omitted, directory1 is the current directory");
        System.out.println("options:");
        System.out.println("-both:     synchronize both this directory and the remote directory");
        System.out.println("-sync2:    synchronize only directory2, do not modify directory1");
        System.out.println("-dry:      dry run, just print what would be done");
        System.out.println("-verbose:  print what is decided for each entry");
        System.out.println("--:        end of options, needed only if you have a directory named like an option");
        System.out.println("\n2015, Sebastien Riou");
    }
    public static void main(String[] args) throws IOException {
        boolean localReadOnly=false;
        boolean remoteReadOnly=true;
        boolean dryRun=false;
        boolean verbose=false;
        FileStoreEntry local = NativeFileStore.newEntry(".");
        int i;
        if(args.length<1){
            showHelp();
            return;
        }
        for(i=0;i<args.length-1;i++){
            if(args[i]=="--") {
                i++;
                break;
            }
            switch (args[i]){
                case "-both": remoteReadOnly=false;break;
                case "-sync2": {
                    remoteReadOnly=false;
                    localReadOnly=true;
                    break;
                }
                case "-dry": dryRun=true;break;
                case "-verbose": verbose=true;break;
                case "--help":
                case "-help":
                case "-?":
                    showHelp();
                    return;
                default:
                    if(i==args.length-2){
                        local = NativeFileStore.newEntry(args[i]);
                        if(!local.exists()){
                            System.out.println("ERROR: directory1 does not exist: "+args[i]);
                            return;
                        }
                        if(!local.isDirectory()){
                            System.out.println("ERROR: directory1 is not a directory: "+local.normalize());
                            return;
                        }
                    }else {
                        System.out.println("ERROR: unsupported argument: " + args[i] + "\n");
                        showHelp();
                        return;
                    }
            }
        }
        FileStoreEntry remote = NativeFileStore.newEntry(args[i]);
        if(!remote.exists()){
            System.out.println("ERROR: directory2 does not exist: "+args[i]);
            return;
        }
        if(!remote.isDirectory()){
            System.out.println("ERROR: directory2 is not a directory: "+remote.normalize());
            return;
        }
        FileStoreSynchronizer.syncToLatest(local, remote, localReadOnly, remoteReadOnly, dryRun, verbose);
    }
}
