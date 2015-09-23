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
                /*try {
                    Thread.sleep(10);                 //10 milliseconds delay to get complete operations
                } catch(InterruptedException ex) {
                    ex.printStackTrace();
                }*/
                System.out.println(getMaxEventIndex());
                PathWatchEvent ev=consume();
                while(null!=ev) {
                    System.out.println(ev);
                    ev=consume();
                }
            } catch (InterruptedException e) {
                e.printStackTrace();
            }

        }
    }
}
