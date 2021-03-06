package uk.co.nimp.lofe;

import java.nio.channels.FileChannel;
import java.nio.channels.FileLock;
import java.nio.channels.OverlappingFileLockException;
import java.nio.file.*;
import static java.nio.file.StandardWatchEventKinds.*;
import static java.nio.file.LinkOption.*;
import java.nio.file.attribute.*;
import java.io.*;
import java.util.*;

public class WatchDir implements Runnable {

    private final WatchService watcher;
    private final Map<WatchKey,Path> keys;
    private boolean trace = false;
    final Path root;
    WatchEventProcessor processor;
    Thread processorThread;

    @SuppressWarnings("unchecked")
    static <T> WatchEvent<T> cast(WatchEvent<?> event) {
        return (WatchEvent<T>)event;
    }

    /**
     * Register the given directory with the WatchService
     */
    private void register(Path dir) throws IOException {
        WatchKey key = dir.register(watcher, ENTRY_CREATE, ENTRY_DELETE, ENTRY_MODIFY);
        if (trace) {
            Path prev = keys.get(key);
            if (prev == null) {
                System.out.format("register: %s\n", dir);
            } else {
                if (!dir.equals(prev)) {
                    System.out.format("update: %s -> %s\n", prev, dir);
                }
            }
        }
        keys.put(key, dir);
    }

    /**
     * Register the given directory, and all its sub-directories, with the
     * WatchService.
     */
    private void registerAll(final Path start) throws IOException {
        // register directory and sub-directories
        Files.walkFileTree(start, new SimpleFileVisitor<Path>() {
            @Override
            public FileVisitResult preVisitDirectory(Path dir, BasicFileAttributes attrs)
                throws IOException
            {
                register(dir);
                return FileVisitResult.CONTINUE;
            }
        });
    }

    /**
     * Creates a WatchService and registers the given directory
     */
    WatchDir(Path dir, WatchEventProcessor processor) throws IOException {
        this.watcher = FileSystems.getDefault().newWatchService();
        this.keys = new HashMap<WatchKey,Path>();
        this.processor = processor;

        processorThread = new Thread(this.processor);
        processorThread.start();

        System.out.format("Scanning %s ...\n", dir);
        registerAll(dir);
        System.out.println("Done.");
        root = dir;
        // enable trace after initial registration
        this.trace = true;
    }

    /**
     * Process all events for keys queued to the watcher
     */
    void processEvents() {
        while(true) {
            System.out.println("wait for events");
            // wait for key to be signalled
            WatchKey key;
            try {
                key = watcher.take();
            } catch (InterruptedException x) {
                return;
            }

            Path dir = keys.get(key);
            if (dir == null) {
                System.err.println("WatchKey not recognized!!");
                continue;
            }
            List<PathWatchEvent> eventList = new ArrayList<PathWatchEvent>();
            List<WatchEvent<?>> keyEvents = key.pollEvents();
            System.out.println("key has "+keyEvents.size()+" events");
            for (WatchEvent<?> event: keyEvents) {
                WatchEvent.Kind kind = event.kind();

                // TBD - provide example of how OVERFLOW event is handled
                if (kind == OVERFLOW) {
                    throw new RuntimeException("OVERFLOW");
                    //continue;
                }

                // Context for directory entry event is the file name of entry
                WatchEvent<Path> ev = cast(event);
                Path name = ev.context();
                Path child = dir.resolve(name);

                boolean partialModify = false;
                if (kind == ENTRY_MODIFY) {//find out if the file modification is completed or not
                    if(child.toFile().isFile()) {
                        partialModify = isFileLocked(child.toString());
                    }
                }

                if(partialModify){
                    System.out.format("ENTRY_PARTIAL_MODIFY: %s (file size = %d)\n", child,child.toFile().length());
                }else {
                    // print out event
                    //System.out.format("%s: %s\n", event.kind().name(), child);
                    //System.out.format("\t%s\n", root.relativize(child));
                    //processor.processEvent(new PathWatchEvent(root,ev,root.relativize(child)));
                    eventList.add(new PathWatchEvent(root, ev, root.relativize(child)));
                }

                // if directory is created, then
                // register it and its sub-directories
                if (kind == ENTRY_CREATE) {
                    try {
                        if (Files.isDirectory(child, NOFOLLOW_LINKS)) {
                            registerAll(child);
                        }
                    } catch (IOException x) {
                        throw new RuntimeException(x);
                    }
                }
            }
            processor.processEvents(eventList);
            // reset key and remove from set if directory no longer accessible
            boolean valid = key.reset();
            if (!valid) {
                keys.remove(key);

                // all directories are inaccessible
                if (keys.isEmpty()) {
                    System.out.printf("root directory deleted, exit.");
                    break;
                }
            }
        }
    }

    @Override
    public void run() {
        processEvents();
    }
    static void usage() {
        System.err.println("usage: java WatchDir dir");
        System.exit(-1);
    }

    public static void main(String[] args) throws IOException {
        args = new String[]{"c:\\tmp\\public"};
        // parse arguments
        if (args.length == 0 || args.length > 2)
            usage();

        // register directory and process its events
        Path dir = Paths.get(args[0]);
        Thread mon = new Thread(new WatchDir(dir,new DropBoxEncryptedMirror()));
        mon.start();
        System.out.println("monitor thread started");
    }

    public static boolean isFileLocked(String fileName) {
        boolean locked = false;
        File file = new File(fileName);
        RandomAccessFile ra=null;
        FileChannel channel=null;
        try {
            // Get a file channel for the file
            ra= new RandomAccessFile(file, "rw");
            channel = ra.getChannel();

            /*// Use the file channel to create a lock on the file.
            // This method blocks until it can retrieve the lock.
            FileLock lock = channel.lock();*/
            FileLock lock=null;
            // Try acquiring the lock without blocking. This method returns
            // null or throws an exception if the file is already locked.
            try {
                lock = channel.tryLock();
                if(null==lock) locked = true;
                else {
                    System.out.println("file size = "+file.length());
                    lock.release();
                }
            } catch (Throwable e) {
                locked = true;
            }
        } catch (Throwable e) {
            //must not do anything particular about FileNotFoundException !
            //we can get this: java.io.FileNotFoundException: <filename> (The process cannot access the file because it is being used by another process)
            locked = true;
        } finally{
            if(channel!=null) try {
                channel.close();// Close the file
                ra=null;
            }catch(Throwable e){ }
            if(ra!=null) try {
                ra.close();// Close the file
            }catch(Throwable e){ }
        }
        return locked;
    }


    public static boolean isFileLocked1(String filename) {
        boolean isLocked=false;
        RandomAccessFile fos=null;
        try {
            File file = new File(filename);
            if(file.exists()) {
                fos=new RandomAccessFile(file,"rw");
            }
        } catch (FileNotFoundException e) {
            isLocked=true;
        }catch (Exception e) {
            // handle exception
        }finally {
            try {
                if(fos!=null) {
                    fos.close();
                }
            }catch(Exception e) {
                //handle exception
            }
        }
        return isLocked;
    }

}
