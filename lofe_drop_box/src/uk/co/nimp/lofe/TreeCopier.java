package uk.co.nimp.lofe;

import java.io.File;
import java.io.IOException;
import java.nio.file.*;
import java.nio.file.attribute.BasicFileAttributes;
import java.nio.file.attribute.FileTime;
import static java.nio.file.StandardCopyOption.*;
import static java.nio.file.FileVisitResult.*;

/**
 * A {@code FileVisitor} that copies a file-tree ("cp -r")
 */
class TreeCopier implements FileVisitor<Path> {
    private final Path source;
    private final Path target;
    private final boolean prompt;
    private final boolean preserve;//preserve attributes like timestamps
    private final boolean dryRun;

    TreeCopier(Path source, Path target, boolean prompt, boolean preserve, boolean dryRun) {
        this.source = source;
        this.target = target;
        this.prompt = prompt;
        this.preserve = preserve;
        this.dryRun = dryRun;
    }

    /**
     * Returns {@code true} if okay to overwrite a  file ("cp -i")
     */
    static boolean okayToOverwrite(Path file) {
        String answer = System.console().readLine("overwrite %s (yes/no)? ", file);
        return (answer.equalsIgnoreCase("y") || answer.equalsIgnoreCase("yes"));
    }

    public static void fileCopy(Path source, Path target, CopyOption[] options, boolean dryRun) throws IOException {
        if(dryRun) System.out.println("DRYRUN: copy file "+source.toString()+" to "+target.toString());
        else Files.copy(source, target, options);
    }

    public static void fileCopy(Path source, Path target, boolean dryRun) throws IOException {
        CopyOption[] options = new CopyOption[] { COPY_ATTRIBUTES, REPLACE_EXISTING };
        fileCopy(source, target, options, dryRun);
    }

    /**
     * Copy source file to target location. If {@code prompt} is true then
     * prompt user to overwrite target if it exists. The {@code preserve}
     * parameter determines if file attributes should be copied/preserved.
     */
    static void copyFile(Path source, Path target, boolean prompt, boolean preserve, boolean dryRun) {
        CopyOption[] options = (preserve) ?
                new CopyOption[] { COPY_ATTRIBUTES, REPLACE_EXISTING } :
                new CopyOption[] { REPLACE_EXISTING };
        if (!prompt || Files.notExists(target) || okayToOverwrite(target)) {
            try {
                fileCopy(source,target,options,dryRun);
            } catch (IOException x) {
                System.err.format("Unable to copy: %s: %s%n", source, x);
            }
        }
    }

    @Override
    public FileVisitResult preVisitDirectory(Path dir, BasicFileAttributes attrs) {
        // before visiting entries in a directory we copy the directory
        // (okay if directory already exists).
        CopyOption[] options = (preserve) ?
                new CopyOption[] { COPY_ATTRIBUTES } : new CopyOption[0];

        Path newdir = target.resolve(source.relativize(dir));
        try {
            if(dryRun) System.out.println("DRYRUN: create directory "+newdir);
            else Files.copy(dir, newdir, options);
        } catch (FileAlreadyExistsException x) {
            // ignore
        } catch (IOException x) {
            System.err.format("Unable to create: %s: %s%n", newdir, x);
            return SKIP_SUBTREE;
        }
        return CONTINUE;
    }

    @Override
    public FileVisitResult visitFile(Path file, BasicFileAttributes attrs) {
        copyFile(file, target.resolve(source.relativize(file)),
                prompt, preserve, dryRun);
        return CONTINUE;
    }

    @Override
    public FileVisitResult postVisitDirectory(Path dir, IOException exc) {
        // fix up modification time of directory when done
        if (exc == null && preserve) {
            Path newdir = target.resolve(source.relativize(dir));
            try {
                if(!dryRun) {
                    FileTime time = Files.getLastModifiedTime(dir);
                    Files.setLastModifiedTime(newdir, time);
                }
            } catch (IOException x) {
                System.err.format("Unable to copy all attributes to: %s: %s%n", newdir, x);
            }
        }
        return CONTINUE;
    }

    @Override
    public FileVisitResult visitFileFailed(Path file, IOException exc) {
        if (exc instanceof FileSystemLoopException) {
            System.err.println("cycle detected: " + file);
        } else {
            System.err.format("Unable to copy: %s: %s%n", file, exc);
        }
        return CONTINUE;
    }
}
