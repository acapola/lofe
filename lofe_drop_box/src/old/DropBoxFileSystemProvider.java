package old;

import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.nio.channels.SeekableByteChannel;
import java.nio.file.*;
import java.nio.file.attribute.BasicFileAttributes;
import java.nio.file.attribute.FileAttribute;
import java.nio.file.attribute.FileAttributeView;
import java.nio.file.spi.FileSystemProvider;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

/**
 * Created by seb on 9/13/2015.
 */
public class DropBoxFileSystemProvider extends FileSystemProvider {

    private final Map<Path, DropBoxFileSystem> filesystems = new HashMap<>();

    protected Path uriToPath(URI uri) {
        String scheme = uri.getScheme();
        if ((scheme == null) || !scheme.equalsIgnoreCase(getScheme())) {
            throw new IllegalArgumentException("URI scheme is not '" + getScheme() + "'");
        }
        try {
            // only support legacy JAR URL syntax  jar:{uri}!/{entry} for now
            String spec = uri.getSchemeSpecificPart();
            int sep = spec.indexOf("!/");
            if (sep != -1)
                spec = spec.substring(0, sep);
            return Paths.get(new URI(spec)).toAbsolutePath();
        } catch (URISyntaxException e) {
            throw new IllegalArgumentException(e.getMessage(), e);
        }
    }

    private boolean ensureFile(Path path) {
        try {
            BasicFileAttributes attrs =
                    Files.readAttributes(path, BasicFileAttributes.class);
            if (!attrs.isRegularFile())
                throw new UnsupportedOperationException();
            return true;
        } catch (IOException ioe) {
            return false;
        }
    }

    @Override
    public String getScheme() {
        return "dropbox";
    }

    @Override
    public FileSystem newFileSystem(URI uri, Map<String, ?> env) throws IOException {
        Path path = uriToPath(uri);
        synchronized(filesystems) {
            Path realPath = null;
            if (ensureFile(path)) {
                realPath = path.toRealPath();
                if (filesystems.containsKey(realPath))
                    throw new FileSystemAlreadyExistsException();
            }
            DropBoxFileSystem fs = null;
            try {
                fs = new DropBoxFileSystem(this, path, env);
            } catch (Throwable e) {
                throw new UnsupportedOperationException(e);
            }
            filesystems.put(realPath, fs);
            return fs;
        }
    }

    @Override
    public FileSystem getFileSystem(URI uri) {
        synchronized (filesystems) {
            DropBoxFileSystem fs = null;
            try {
                fs = filesystems.get(uriToPath(uri).toRealPath());
            } catch (IOException x) {
                // ignore the ioe from toRealPath(), return FSNFE
            }
            if (fs == null)
                throw new FileSystemNotFoundException();
            return fs;
        }
    }

    @Override
    public Path getPath(URI uri) {
        String spec = uri.getSchemeSpecificPart();
        int sep = spec.indexOf("!/");
        if (sep == -1)
            throw new IllegalArgumentException("URI: "
                    + uri
                    + " does not contain path info ex. jar:file:/c:/foo.zip!/BAR");
        return getFileSystem(uri).getPath(spec.substring(sep + 1));
    }

    // Checks that the given file is a UnixPath
    static final DropBoxPath toDropBoxPath(Path path) {
        if (path == null)
            throw new NullPointerException();
        if (!(path instanceof DropBoxPath))
            throw new ProviderMismatchException();
        return (DropBoxPath)path;
    }

    @Override
    public SeekableByteChannel newByteChannel(Path path, Set<? extends OpenOption> options, FileAttribute<?>... attrs) throws IOException {
        return null;//toDropBoxPath(path).newByteChannel(options, attrs);
    }

    @Override
    public DirectoryStream<Path> newDirectoryStream(Path dir, DirectoryStream.Filter<? super Path> filter) throws IOException {
        return null;
    }

    @Override
    public void createDirectory(Path dir, FileAttribute<?>... attrs) throws IOException {

    }

    @Override
    public void delete(Path path) throws IOException {

    }

    @Override
    public void copy(Path source, Path target, CopyOption... options) throws IOException {

    }

    @Override
    public void move(Path source, Path target, CopyOption... options) throws IOException {

    }

    @Override
    public boolean isSameFile(Path path, Path path2) throws IOException {
        return false;
    }

    @Override
    public boolean isHidden(Path path) throws IOException {
        return false;
    }

    @Override
    public FileStore getFileStore(Path path) throws IOException {
        return null;
    }

    @Override
    public void checkAccess(Path path, AccessMode... modes) throws IOException {

    }

    @Override
    public <V extends FileAttributeView> V getFileAttributeView(Path path, Class<V> type, LinkOption... options) {
        return null;
    }

    @Override
    public <A extends BasicFileAttributes> A readAttributes(Path path, Class<A> type, LinkOption... options) throws IOException {
        return null;
    }

    @Override
    public Map<String, Object> readAttributes(Path path, String attributes, LinkOption... options) throws IOException {
        return null;
    }

    @Override
    public void setAttribute(Path path, String attribute, Object value, LinkOption... options) throws IOException {

    }
}
