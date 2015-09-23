package old;

import java.io.File;
import java.io.IOException;
import java.net.URI;
import java.nio.file.*;
import java.util.Iterator;

/**
 * Created by seb on 9/13/2015.
 */
public class DropBoxPath implements Path {
    private final DropBoxFileSystem fs;
    private final Path path;

    DropBoxPath(DropBoxFileSystem fs, String path) {
        this(fs, path, false);
    }

    DropBoxPath(DropBoxFileSystem fs, Path path) {
        this(fs, path.toString(), false);
    }

    DropBoxPath(DropBoxFileSystem fs, String path, boolean normalized)
    {
        this.fs = fs;
        if (normalized)
            this.path = (new File(fs.basePath,path)).toPath();
        else
            this.path = (new File(fs.basePath,path)).toPath().normalize();
    }

/*    SeekableByteChannel newByteChannel(Set<? extends OpenOption> options,
                                       FileAttribute<?>... attrs)
            throws IOException
    {
        return fs.newByteChannel(getResolvedPath(), options, attrs);
    }*/

    // resolved path for locating zip entry inside the zip file,
    // the result path does not contain ./ and .. components
    String getResolvedPath() {
        return path.resolve(fs.basePath.toString()).toString();
    }

    @Override
    public FileSystem getFileSystem() {
        return fs;
    }

    @Override
    public boolean isAbsolute() {
        return path.isAbsolute();
    }

    @Override
    public Path getRoot() {
        return path.getRoot();
    }

    @Override
    public Path getFileName() {
        return new DropBoxPath(fs, path.getFileName());
    }

    @Override
    public Path getParent() {
        return path.getParent();
    }

    @Override
    public int getNameCount() {
        return path.getNameCount();
    }

    @Override
    public Path getName(int index) {
        return path.getName(index);
    }

    @Override
    public Path subpath(int beginIndex, int endIndex) {
        return path.subpath(beginIndex,endIndex);
    }

    @Override
    public boolean startsWith(Path other) {
        return path.startsWith(other);
    }

    @Override
    public boolean startsWith(String other) {
        return path.startsWith(other);
    }

    @Override
    public boolean endsWith(Path other) {
        return path.endsWith(other);
    }

    @Override
    public boolean endsWith(String other) {
        return path.endsWith(other);
    }

    @Override
    public Path normalize() {
        return path.normalize();
    }

    @Override
    public Path resolve(Path other) {
        return path.resolve(other);
    }

    @Override
    public Path resolve(String other) {
        return path.resolve(other);
    }

    @Override
    public Path resolveSibling(Path other) {
        return path.resolveSibling(other);
    }

    @Override
    public Path resolveSibling(String other) {
        return path.resolveSibling(other);
    }

    @Override
    public Path relativize(Path other) {
        return path.relativize(other);
    }

    @Override
    public URI toUri() {
        return path.toUri();
    }

    @Override
    public Path toAbsolutePath() {
        return path.toAbsolutePath();
    }

    @Override
    public Path toRealPath(LinkOption... options) throws IOException {
        return path.toRealPath(options);
    }

    @Override
    public File toFile() {
        return path.toFile();
    }

    @Override
    public WatchKey register(WatchService watcher, WatchEvent.Kind<?>[] events, WatchEvent.Modifier... modifiers) throws IOException {
        return path.register(watcher, events, modifiers);
    }

    @Override
    public WatchKey register(WatchService watcher, WatchEvent.Kind<?>... events) throws IOException {
        return path.register(watcher, events);
    }

    @Override
    public Iterator<Path> iterator() {
        return path.iterator();
    }

    @Override
    public int compareTo(Path other) {
        return path.compareTo(other);
    }

    @Override
    public boolean equals(Object other) {
        return path.equals(other);
    }

    @Override
    public int hashCode() {
        return path.hashCode();
    }

    @Override
    public String toString() {
        return path.toString();
    }
}
