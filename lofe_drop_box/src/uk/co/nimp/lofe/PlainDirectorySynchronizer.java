package uk.co.nimp.lofe;

import java.io.*;
import java.nio.file.FileVisitOption;
import java.nio.file.Files;
import java.util.EnumSet;
import java.util.HashSet;

/**
 * Created by seb on 9/12/2015.
 */
public class PlainDirectorySynchronizer {
    final File aDir;
    final File bDir;
    final boolean aReadOnly;
    final boolean bReadOnly;

    public PlainDirectorySynchronizer(File aDir, File bDir, boolean aReadOnly, boolean bReadOnly) {
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
    public void syncToLatest(boolean dryRun) throws IOException {
        syncToLatest(aDir, bDir, aReadOnly, bReadOnly, dryRun);
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

    public static void syncToLatest(File aDir, File bDir, boolean aReadOnly, boolean bReadOnly, boolean dryRun) throws IOException {
        File[] aFiles = aDir.listFiles();
        File[] bFiles = bDir.listFiles();
        HashSet<File> bFilesRemaining = new HashSet<File>();
        for(File pf:bFiles) bFilesRemaining.add(pf);
        for(File af:aFiles){
            File bf = new File(bDir,af.getName());
            bFilesRemaining.remove(bf);
            syncLatestCore(af, bf, aReadOnly,bReadOnly,dryRun);
        }
        if(!aReadOnly) {
            for (File bf : bFilesRemaining) {
                File af = new File(aDir, bf.getName());
                syncLatestCore(bf, af, bReadOnly, aReadOnly, dryRun);
            }
        }
    }

    static void syncLatestCore(File a, File b, boolean aReadOnly, boolean bReadOnly, boolean dryRun) throws IOException {
        if(b.exists()) {
            if (a.isDirectory()) {
                if (b.isDirectory()) syncToLatest(a, b, aReadOnly, bReadOnly, dryRun);
                else {
                    throw new RuntimeException("Directory " + a.getCanonicalPath() + " is a file in plain directory: " + b.getCanonicalPath());
                }
            } else {
                if(filesDifferent(a, b)) {
                    long aTime = a.lastModified();
                    long bTime = b.lastModified();
                    if (bTime > aTime) if(!aReadOnly) TreeCopier.fileCopy(b.toPath(),a.toPath(),dryRun);
                    else if(!bReadOnly) TreeCopier.fileCopy(a.toPath(),b.toPath(),dryRun);
                }
            }
        } else if(!bReadOnly) {//copy a to b
            if(a.isDirectory()){
                // follow links when copying files
                EnumSet<FileVisitOption> opts = EnumSet.of(FileVisitOption.FOLLOW_LINKS);
                TreeCopier tc = new TreeCopier(a.toPath(), b.toPath(), false, true, dryRun);
                Files.walkFileTree(a.toPath(), opts, Integer.MAX_VALUE, tc);
            }else{
                TreeCopier.fileCopy(a.toPath(),b.toPath(),dryRun);
            }
        }
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
        System.out.println("--:        end of options, needed only if you have a directory named like an option");
        System.out.println("\n2015, Sebastien Riou");
    }
    public static void main(String[] args) throws IOException {
        boolean localReadOnly=false;
        boolean remoteReadOnly=true;
        boolean dryRun=true;
        File local = new File(".");
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
                case "--help":
                case "-help":
                case "-?":
                    showHelp();
                    return;
                default:
                    if(i==args.length-2){
                        local = new File(args[i]);
                        if(!local.exists()){
                            System.out.println("ERROR: directory1 does not exist: "+args[i]);
                            return;
                        }
                        if(!local.isDirectory()){
                            System.out.println("ERROR: directory1 is not a directory: "+local.getCanonicalPath());
                            return;
                        }
                    }else {
                        System.out.println("ERROR: unsupported argument: " + args[i] + "\n");
                        showHelp();
                        return;
                    }
            }
        }
        File remote = new File(args[i]);
        if(!remote.exists()){
            System.out.println("ERROR: directory2 does not exist: "+args[i]);
            return;
        }
        if(!remote.isDirectory()){
            System.out.println("ERROR: directory2 is not a directory: "+remote.getCanonicalPath());
            return;
        }
        PlainDirectorySynchronizer.syncToLatest(local, remote, localReadOnly, remoteReadOnly, dryRun);
    }
}
