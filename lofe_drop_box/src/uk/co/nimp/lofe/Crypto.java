package uk.co.nimp.lofe;

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.NoSuchPaddingException;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.spec.AlgorithmParameterSpec;

/**
 * Created by seb on 9/6/2015.
 */
public class Crypto {
    final String CipherAlgorithmName = "AES";
    final String CipherAlgorithmMode = "ECB";
    final String CipherAlgorithmPadding = "NoPadding";
    final String CryptoTransformation = CipherAlgorithmName + "/" + CipherAlgorithmMode + "/" + CipherAlgorithmPadding;

    final byte[] keyTweak;
    final Cipher aesEncCipher;
    final Cipher aesDecCipher;

    public Crypto(byte[] key, byte[] keyTweak) throws InvalidKeyException, NoSuchPaddingException, NoSuchAlgorithmException {
        if(keyTweak.length!=16) throw new InvalidKeyException("keyTweak length is "+keyTweak.length+", it must be exactly 16 bytes long.");
        this.keyTweak = keyTweak;
        SecretKeySpec ky = new SecretKeySpec(key, CipherAlgorithmName);
        aesEncCipher = Cipher.getInstance(CryptoTransformation);
        aesEncCipher.init(Cipher.ENCRYPT_MODE, ky);
        aesDecCipher = Cipher.getInstance(CryptoTransformation);
        aesDecCipher.init(Cipher.DECRYPT_MODE, ky);
    }

    public byte[] encryptBlock(byte[] src, byte[] iv, byte[] offset){
        assert(16==src.length);
        assert(16==iv.length);
        assert(8==offset.length);
        byte[] offset2 = new byte[16];
        System.arraycopy(offset,0,offset2,0,8);
        aesEnc128Round(offset2,keyTweak);
        aesEnc128Round(offset2, iv);
        for(int i=0;i<16;i++) offset2[i] = (byte)(src[i]^offset2[i]);
        byte[] dst = aesEnc(offset2);
        return dst;
    }
    public byte[] decryptBlock(byte[] src, byte[] iv, byte[] offset){
        assert(16==src.length);
        assert(16==iv.length);
        assert(8==offset.length);
        byte[] dst = aesDec(src);
        byte[] offset2 = new byte[16];
        System.arraycopy(offset,0,offset2,0,8);
        aesEnc128Round(offset2,keyTweak);
        aesEnc128Round(offset2,iv);
        for(int i=0;i<16;i++) dst[i] = (byte)(dst[i]^offset2[i]);
        return dst;
    }

    void aesEnc128Round(byte[] state, byte[] key){

    }

    byte[] aesEnc(byte[] state) {
        byte[] out;
        try {
            out = aesEncCipher.doFinal(state);
        } catch (IllegalBlockSizeException ex) {
            throw new RuntimeException(ex);
        } catch (BadPaddingException ex) {
            throw new RuntimeException(ex);
        }
        return out;
    }
    byte[] aesDec(byte[] state) {
        byte[] out;
        try {
            out = aesDecCipher.doFinal(state);
        } catch (IllegalBlockSizeException ex) {
            throw new RuntimeException(ex);
        } catch (BadPaddingException ex) {
            throw new RuntimeException(ex);
        }
        return out;
    }

    public static class Header{
        static final int INFO_LEN = 8;
        static final int LEN_LEN = 8;
        static final int IV_LEN = 16;
        static final int HASH_LEN = 32;
        byte[] info = new byte[INFO_LEN];
        byte[] len = new byte[LEN_LEN];
        byte[] iv = new byte[IV_LEN];
        byte[] hash = new byte[HASH_LEN];

        public Header() {
        }

        public Header(byte[] info, byte[] len, byte[] iv, byte[] hash) {
            this.info = info;
            this.len = len;
            this.iv = iv;
            this.hash = hash;
        }

        public byte[] getInfo() {
            return info;
        }

        public void setInfo(byte[] info) {
            this.info = info;
        }

        public byte[] getLen() {
            return len;
        }

        public long getLenValue() {
            long out = 0;
            for(int i=0;i<LEN_LEN;i++){
                out = (out<<8) + len[i];
            }
            return out;
        }

        private void setLen(long length) {
            int shift = 0;
            for(int i=0;i<LEN_LEN;i++){
                len[i] = (byte)(length>>shift);
                shift+=8;
            }
        }

        public void setLen(byte[] len) {
            this.len = len;
        }

        public byte[] getIv() {
            return iv;
        }

        public void setIv(byte[] iv) {
            this.iv = iv;
        }

        public byte[] getHash() {
            return hash;
        }

        public void setHash(byte[] hash) {
            this.hash = hash;
        }

        public static Header build(byte[] keyTweak, byte[] iv,File plainFile) throws NoSuchAlgorithmException, IOException {
            Header h = new Header();
            h.iv=iv.clone();
            h.setLen(plainFile.length());
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            BufferedInputStream plainContent = new BufferedInputStream(new FileInputStream(plainFile));
            md.update(keyTweak);
            md.update(h.getInfo());
            md.update(h.getLen());
            md.update(h.getIv());
            byte[] buf = new byte[64*1024];
            int nRead;
            while((nRead = plainContent.read(buf))!=-1){
                md.update(buf,0,nRead);
            }
            h.hash = md.digest();
            return h;
        }
    }


}
