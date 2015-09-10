package uk.co.nimp.lofe;

import java.nio.file.Path;
import java.nio.file.WatchEvent;
import java.util.HashMap;

/**
 * Created by seb on 9/8/2015.
 */
public class DropBoxEncryptedMirror extends WatchEventProcessor{


    @Override
    public void run() {

        //TODO: pump events and query/listen DropBox server
        while (true) {
            try {
                synchronized (syncObj) {
                    syncObj.wait();
                }
                System.out.println(consume());
            } catch (InterruptedException e) {
                e.printStackTrace();
            }

        }
    }
}
