package uk.co.nimp.lofe;

import java.nio.file.Path;
import java.nio.file.WatchEvent;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.time.Instant;
import java.util.Date;

/**
 * Created by seb on 9/9/2015.
 */
public class PathWatchEvent {

    final Path root;
    final WatchEvent<Path> event;
    final Path relativePath;
    final Date date;

    public PathWatchEvent(Path root, WatchEvent<Path> event, Path relativePath) {
        this.root = root;
        this.event = event;
        this.relativePath = relativePath.normalize();
        date = Date.from(Instant.now());
    }

    public String getName() {
        return event.kind().name();
    }

    public WatchEvent.Kind<Path> getKind() {
        return event.kind();
    }

    public Path getContext() {
        return event.context();
    }

    public int getCount() {
        return event.count();
    }

    public Path getRoot() {
        return root;
    }

    public WatchEvent<Path> getEvent() {
        return event;
    }

    public Path getRelativePath() {
        return relativePath;
    }

    public Date getDate() {
        return date;
    }

    DateFormat dateFormat = new SimpleDateFormat("yyyy/MM/dd HH:mm:ss:SSS");

    @Override
    public String toString() {
        return String.format("%s %s %d %s %s", dateFormat.format(date), getName(), getCount(), getRoot().toAbsolutePath(), getRelativePath());
    }
}
