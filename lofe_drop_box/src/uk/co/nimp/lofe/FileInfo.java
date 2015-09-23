package uk.co.nimp.lofe;

/**
 * Created by seb on 9/13/2015.
 */
public class FileInfo {
    final String path;
    final byte[] digest;
    final long lastModifiedTimestamp;
    final long infoTimestamp;

    public FileInfo(String path, byte[] digest, long lastModifiedTimestamp, long infoTimestamp) {
        this.path = path;
        this.digest = digest;
        this.lastModifiedTimestamp = lastModifiedTimestamp;
        this.infoTimestamp = infoTimestamp;
    }

    public String getPath() {
        return path;
    }

    public byte[] getDigest() {
        return digest;
    }

    public long getLastModifiedTimestamp() {
        return lastModifiedTimestamp;
    }

    public long getInfoTimestamp() {
        return infoTimestamp;
    }
}
