package old;

import java.io.File;
import java.io.IOException;
import java.nio.file.*;
import java.nio.file.attribute.UserPrincipalLookupService;
import java.nio.file.spi.FileSystemProvider;
import java.util.ArrayList;
import java.util.Map;
import java.util.Set;
import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;

/**
 * Created by seb on 9/13/2015.
 */
public class DropBoxFileSystem extends FileSystem {
    private final DropBoxFileSystemProvider provider;
    String accessToken;
    String APP_KEY;
    String APP_SECRET;
    final File basePath = new File("c:\\tmp\\dpfs");
    private volatile boolean isOpen = true;

    DropBoxFileSystem(DropBoxFileSystemProvider provider,
                  Path path,
                  Map<String, ?> env)
            throws IOException
    {
        /*
        // configurable env setup
        this.accessToken = env.containsKey("accessToken") ?
                (String)env.get("accessToken") : "";
        if(!env.containsKey("APP_KEY")) throw new IllegalArgumentException("APP_KEY is missing from env");
        this.APP_KEY   = (String)env.get("APP_KEY");
        if(!env.containsKey("APP_SECRET")) throw new IllegalArgumentException("APP_SECRET is missing from env");
        this.APP_SECRET   = (String)env.get("APP_SECRET");
*/
        this.provider = provider;
    }


    final String getString(byte[] name) {
        return String.valueOf(name);
    }

    @Override
    public FileSystemProvider provider() {
        return provider;
    }

    @Override
    public void close() throws IOException {
        if (!isOpen)
            return;
        isOpen = false;             // set closed
    }

    @Override
    public boolean isOpen() {
        return isOpen;
    }

    @Override
    public boolean isReadOnly() {
        return false;
    }

    @Override
    public String getSeparator() {
        return File.pathSeparator;
    }

    @Override
    public Iterable<Path> getRootDirectories() {
        ArrayList<Path> pathArr = new ArrayList<>();
        pathArr.add(new DropBoxPath(this, "/"));
        return pathArr;
    }

    @Override
    public Iterable<FileStore> getFileStores() {
        return null;
    }

    @Override
    public Set<String> supportedFileAttributeViews() {
        return null;
    }

    @Override
    public Path getPath(String first, String... more) {
        String path;
        if (more.length == 0) {
            path = first;
        } else {
            StringBuilder sb = new StringBuilder();
            sb.append(first);
            for (String segment: more) {
                if (segment.length() > 0) {
                    if (sb.length() > 0)
                        sb.append('/');
                    sb.append(segment);
                }
            }
            path = sb.toString();
        }
        return new DropBoxPath(this, path);
    }

    private static final String GLOB_SYNTAX = "glob";
    private static final String REGEX_SYNTAX = "regex";

    @Override
    public PathMatcher getPathMatcher(String syntaxAndInput) {
        int pos = syntaxAndInput.indexOf(':');
        if (pos <= 0 || pos == syntaxAndInput.length()) {
            throw new IllegalArgumentException();
        }
        String syntax = syntaxAndInput.substring(0, pos);
        String input = syntaxAndInput.substring(pos + 1);
        String expr;
        if (syntax.equals(GLOB_SYNTAX)) {
            expr = toRegexPattern(input);
        } else {
            if (syntax.equals(REGEX_SYNTAX)) {
                expr = input;
            } else {
                throw new UnsupportedOperationException("Syntax '" + syntax +
                        "' not recognized");
            }
        }
        // return matcher
        final Pattern pattern = Pattern.compile(expr);
        return new PathMatcher() {
            @Override
            public boolean matches(Path path) {
                return pattern.matcher(path.toString()).matches();
            }
        };
    }

    @Override
    public UserPrincipalLookupService getUserPrincipalLookupService() {
        return null;
    }

    @Override
    public WatchService newWatchService() throws IOException {
        return null;
    }
    private static final String regexMetaChars = ".^$+{[]|()";
    private static final String globMetaChars = "\\*?[{";
    private static boolean isRegexMeta(char c) {
        return regexMetaChars.indexOf(c) != -1;
    }
    private static boolean isGlobMeta(char c) {
        return globMetaChars.indexOf(c) != -1;
    }
    private static char EOL = 0;  //TBD
    private static char next(String glob, int i) {
        if (i < glob.length()) {
            return glob.charAt(i);
        }
        return EOL;
    }
    /*
     * Creates a regex pattern from the given glob expression.
     *
     * @throws  PatternSyntaxException
     */
    public static String toRegexPattern(String globPattern) {
        boolean inGroup = false;
        StringBuilder regex = new StringBuilder("^");

        int i = 0;
        while (i < globPattern.length()) {
            char c = globPattern.charAt(i++);
            switch (c) {
                case '\\':
                    // escape special characters
                    if (i == globPattern.length()) {
                        throw new PatternSyntaxException("No character to escape",
                                globPattern, i - 1);
                    }
                    char next = globPattern.charAt(i++);
                    if (isGlobMeta(next) || isRegexMeta(next)) {
                        regex.append('\\');
                    }
                    regex.append(next);
                    break;
                case '/':
                    regex.append(c);
                    break;
                case '[':
                    // don't match name separator in class
                    regex.append("[[^/]&&[");
                    if (next(globPattern, i) == '^') {
                        // escape the regex negation char if it appears
                        regex.append("\\^");
                        i++;
                    } else {
                        // negation
                        if (next(globPattern, i) == '!') {
                            regex.append('^');
                            i++;
                        }
                        // hyphen allowed at start
                        if (next(globPattern, i) == '-') {
                            regex.append('-');
                            i++;
                        }
                    }
                    boolean hasRangeStart = false;
                    char last = 0;
                    while (i < globPattern.length()) {
                        c = globPattern.charAt(i++);
                        if (c == ']') {
                            break;
                        }
                        if (c == '/') {
                            throw new PatternSyntaxException("Explicit 'name separator' in class",
                                    globPattern, i - 1);
                        }
                        // TBD: how to specify ']' in a class?
                        if (c == '\\' || c == '[' ||
                                c == '&' && next(globPattern, i) == '&') {
                            // escape '\', '[' or "&&" for regex class
                            regex.append('\\');
                        }
                        regex.append(c);

                        if (c == '-') {
                            if (!hasRangeStart) {
                                throw new PatternSyntaxException("Invalid range",
                                        globPattern, i - 1);
                            }
                            if ((c = next(globPattern, i++)) == EOL || c == ']') {
                                break;
                            }
                            if (c < last) {
                                throw new PatternSyntaxException("Invalid range",
                                        globPattern, i - 3);
                            }
                            regex.append(c);
                            hasRangeStart = false;
                        } else {
                            hasRangeStart = true;
                            last = c;
                        }
                    }
                    if (c != ']') {
                        throw new PatternSyntaxException("Missing ']", globPattern, i - 1);
                    }
                    regex.append("]]");
                    break;
                case '{':
                    if (inGroup) {
                        throw new PatternSyntaxException("Cannot nest groups",
                                globPattern, i - 1);
                    }
                    regex.append("(?:(?:");
                    inGroup = true;
                    break;
                case '}':
                    if (inGroup) {
                        regex.append("))");
                        inGroup = false;
                    } else {
                        regex.append('}');
                    }
                    break;
                case ',':
                    if (inGroup) {
                        regex.append(")|(?:");
                    } else {
                        regex.append(',');
                    }
                    break;
                case '*':
                    if (next(globPattern, i) == '*') {
                        // crosses directory boundaries
                        regex.append(".*");
                        i++;
                    } else {
                        // within directory boundary
                        regex.append("[^/]*");
                    }
                    break;
                case '?':
                    regex.append("[^/]");
                    break;
                default:
                    if (isRegexMeta(c)) {
                        regex.append('\\');
                    }
                    regex.append(c);
            }
        }
        if (inGroup) {
            throw new PatternSyntaxException("Missing '}", globPattern, i - 1);
        }
        return regex.append('$').toString();
    }
}
