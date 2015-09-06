package uk.co.nimp.lofe;
import com.dropbox.core.*;

import java.io.*;
import java.util.Locale;

public class Main {

    public static void main(String[] args) throws IOException, DbxException {
        String accessToken = "5RgENv3jMiAAAAAAAAAAHp3MuBHSYHdZo_SMgcj2doJj5NtC035mb_Te3KiY7jbP";
        DbxRequestConfig config = new DbxRequestConfig(
                "JavaTutorial/1.0", Locale.getDefault().toString());
        if(accessToken.isEmpty()) {
            // Get your app key and secret from the Dropbox developers website.
            final String APP_KEY = "b6hxknt2m9b1ttu";
            final String APP_SECRET = "t3yy3y5rkvgrv59";

            DbxAppInfo appInfo = new DbxAppInfo(APP_KEY, APP_SECRET);
            DbxWebAuthNoRedirect webAuth = new DbxWebAuthNoRedirect(config, appInfo);

            // Have the user sign in and authorize your app.
            String authorizeUrl = webAuth.start();
            System.out.println("1. Go to: " + authorizeUrl);
            System.out.println("2. Click \"Allow\" (you might have to log in first)");
            System.out.println("3. Copy the authorization code.");
            String code = new BufferedReader(new InputStreamReader(System.in)).readLine().trim();

            DbxAuthFinish authFinish = webAuth.finish(code);
            accessToken = authFinish.accessToken;
            System.out.println(accessToken);
        }

        DbxClient client = new DbxClient(config, accessToken);
        System.out.println("Linked account: " + client.getAccountInfo().displayName);


        File inputFile = new File("working-draft.txt");
        FileInputStream inputStream = new FileInputStream(inputFile);
        try {
            DbxEntry.File uploadedFile = client.uploadFile("/magnum-opus2.txt",
                    DbxWriteMode.add(), inputFile.length(), inputStream);
            System.out.println("Uploaded: " + uploadedFile.toString());
        } finally {
            inputStream.close();
        }

        DbxEntry.WithChildren listing = client.getMetadataWithChildren("/");
        System.out.println("Files in the root path:");
        for (DbxEntry child : listing.children) {
            System.out.println("	" + child.name + ": " + child.toString());
        }

        FileOutputStream outputStream = new FileOutputStream("magnum-opus.txt");
        try {
            DbxEntry.File downloadedFile = client.getFile("/magnum-opus.txt", null,
                    outputStream);
            System.out.println("Metadata: " + downloadedFile.toString());
        } finally {
            outputStream.close();
        }

    }
}
