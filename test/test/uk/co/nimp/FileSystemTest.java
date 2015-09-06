package uk.co.nimp;

import org.junit.BeforeClass;
import org.junit.FixMethodOrder;
import org.junit.Test;
import org.junit.runners.MethodSorters;

import java.io.*;
import java.nio.channels.FileChannel;
import java.nio.file.Files;
import java.nio.file.StandardOpenOption;

@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class FileSystemTest {
    public static final class OsUtils
    {
        private static String OS = null;
        public static String getOsName()
        {
            if(OS == null) { OS = System.getProperty("os.name"); }
            return OS;
        }
        public static boolean isWindows(){
            return getOsName().startsWith("Windows");
        }
 // and so on
    }
    static File base;
    @BeforeClass
    public static void setUpBeforeClass() throws Exception {
        String baseDir;
        if(OsUtils.isWindows()) baseDir = "t:\\";
        else baseDir = "/home/seb/tmp/myfs";
        base=new File(baseDir,"FileSystemTest");
        /*boolean deleted = false;
        if(base.exists()) {
            try {
                deleted = deleteDirectory(base);
                assert(deleted);
            } catch (IOException e) {
                e.printStackTrace();
            }
        }

        assert(!base.exists());
        base.mkdir();*/
    }

    /*static {
        String baseDir;
        if(OsUtils.isWindows()) baseDir = "t:\\";
        else baseDir = "/home/seb/tmp/myfs";
        base=new File(baseDir,"FileSystemTest");
        if(base.exists()) {
            try {
                deleteDirectory(base);
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
        assert(!base.exists());
        base.mkdir();
    }*/

    public static boolean deleteDirectory(File directory) throws IOException {
        System.out.println("delete "+directory.getCanonicalPath());
        if(directory.exists()){
            File[] files = directory.listFiles();
            if(null!=files){
                for(int i=0; i<files.length; i++) {
                    if(files[i].isDirectory()) {
                        deleteDirectory(files[i]);
                    }
                    else {
                        files[i].delete();
                    }
                }
            }
        }
        return directory.delete();
    }

    static void writeToFile(byte[] data,File path) throws IOException {
        FileOutputStream stream = new FileOutputStream(path);
        try {
            stream.write(data);
        } finally {
            stream.close();
        }
    }

    static void writeToFile(byte[] data,int offset,int len,File path, long fileOffset) throws IOException {
        RandomAccessFile ra = new RandomAccessFile(path, "rw");
        try {
            ra.seek(fileOffset);
            ra.write(data,offset,len);
        } finally {
            ra.close();
        }
    }

    static byte[] readBytes(File path, long fileOffset, int len) throws IOException {
        RandomAccessFile ra = new RandomAccessFile(path, "rw");
        byte[] out = new byte[len];
        try {
            ra.seek(fileOffset);
            ra.read(out);
        } finally {
            ra.close();
        }
        return out;
    }

    static void assertEquals(byte[] a, byte[]b){
        assert(a.length==b.length);
        for(int i=0;i<a.length;i++) assert(a[i]==b[i]);
    }

    @org.junit.Test
    public void testBinaryAligned() throws Exception {
        byte[] data = new byte[16];
        for(int i=0;i<data.length;i++) data[i]=(byte)i;
        File f = new File(base,"testBinaryAligned");
        assert(!f.exists());
        writeToFile(data,f);
        byte[] readData = Files.readAllBytes(f.toPath());
        assertEquals(data,readData);
        long len = f.length();
        assert(len==data.length);
        assert(f.length()==data.length);
    }

    @org.junit.Test
    public void testBinaryAlignedSubBlock() throws Exception {
        byte[] data = new byte[13];
        for(int i=0;i<data.length;i++) data[i]=(byte)i;
        File f = new File(base,"testBinaryAlignedSubBlock");
        assert(!f.exists());
        writeToFile(data,f);
        byte[] readData = Files.readAllBytes(f.toPath());
        assertEquals(data,readData);
        long len = f.length();
        assert(len==data.length);
        assert(f.length()==data.length);
    }

    @org.junit.Test
    public void testBinaryUnalignedEnd() throws Exception {
        int sizes[] = new int[]{15,17,20,31,33};
        for(int size:sizes) {
            byte[] data = new byte[size];
            for (int i = 0; i < data.length; i++) data[i] = (byte) i;
            String name = "testBinaryUnalignedEnd"+size;
            File f = new File(base, name);
            assert (!f.exists());
            writeToFile(data, f);
            byte[] readData = Files.readAllBytes(f.toPath());
            assertEquals(data, readData);
            assert(f.length()==data.length);
        }
    }

    @org.junit.Test
    public void testBinaryUnaligned() throws Exception {
        byte[] data = new byte[100];
        String name = "testBinaryUnaligned";
        File f = new File(base, name);
        assert (!f.exists());
        writeToFile(data, f);
        int sizes[] = new int[]{15,17,20,31,33};
        int offset=0;
        for(int i=0;i<sizes.length;i++) {
            int next_offset=sizes[i];
            int len=next_offset-offset;
            for (int j = offset; j < next_offset; j++) data[j] = (byte) j;
            writeToFile(data,offset,len, f, offset);
            offset=next_offset;
        }
        byte[] readData = Files.readAllBytes(f.toPath());
        assertEquals(data, readData);
        assert(f.length()==data.length);
    }

    @org.junit.Test
    public void testBinaryUnalignedRead() throws Exception {
        byte[] data = new byte[100];
        for (int i = 0; i < data.length; i++) data[i] = (byte) i;
        String name = "testBinaryUnalignedRead";
        File f = new File(base, name);
        assert (!f.exists());
        writeToFile(data, f);
        int sizes[] = new int[]{15,17,20,31,33};
        int offset=0;
        for(int i=0;i<sizes.length;i++) {
            int next_offset=sizes[i];
            int len=next_offset-offset;
            byte[] expected = new byte[len];
            for (int j = 0; j < len; j++) expected[j] = (byte) (offset+j);
            byte[] readData = readBytes(f,offset,len);
            assertEquals(expected, readData);
            offset=next_offset;
        }
        byte[] readData = Files.readAllBytes(f.toPath());
        assertEquals(data, readData);
        assert(f.length()==data.length);
    }
}