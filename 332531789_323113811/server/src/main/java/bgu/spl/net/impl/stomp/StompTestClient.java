import java.io.OutputStream;
import java.net.Socket;
import java.nio.charset.StandardCharsets;

public class StompTestClient {
    public static void main(String[] args) throws Exception {
        Socket socket = new Socket("localhost", 7777);
        OutputStream out = socket.getOutputStream();

        String frame =
                "CONNECT\n" +
                "accept-version :1.2\n" +
                "host:stomp.cs.bgu.ac.il\n" +
                "login:alice\n" +
                "passcode:123\n" +
                "\n";

        out.write(frame.getBytes(StandardCharsets.UTF_8));
        out.write(0); // STOMP null character
        out.flush();

        Thread.sleep(5000);
        socket.close();
    }
}
