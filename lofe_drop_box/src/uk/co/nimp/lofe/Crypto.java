package uk.co.nimp.lofe;

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.NoSuchPaddingException;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
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
        aesEnc128Round(offset2,iv);
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
}
