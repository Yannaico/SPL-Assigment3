package bgu.spl.net.srv;

import bgu.spl.net.api.MessageEncoderDecoder;
import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.api.MessagingProtocol;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.IOException;
import java.net.Socket;

public class BlockingConnectionHandler<T> implements Runnable, ConnectionHandler<T> {

    private final StompMessagingProtocol<T> protocol;
    private final MessageEncoderDecoder<T> encdec;
    private final Socket sock;
    private BufferedInputStream in;
    private BufferedOutputStream out;
    private volatile boolean connected = true;

    private final int connectionId;
    private final ConnectionsImpl<T> connections;

    public BlockingConnectionHandler(Socket sock, MessageEncoderDecoder<T> reader, MessagingProtocol<T> protocol, int connectionId, ConnectionsImpl<T> connections) {
        this.sock = sock;
        this.encdec = reader;

        // Cast to StompMessagingProtocol
        if (protocol instanceof StompMessagingProtocol) {
            this.protocol = (StompMessagingProtocol<T>) protocol;
        } else {
            throw new IllegalArgumentException("Protocol must implement StompMessagingProtocol");
        }
        
        this.connectionId = connectionId;
        this.connections = connections;
    }

    @Override
    public void run() {
        try (Socket sock = this.sock) { //just for automatic closing
            int read;

            in = new BufferedInputStream(sock.getInputStream());
            out = new BufferedOutputStream(sock.getOutputStream());

            // Initialize protocol with connectionId and connections
            protocol.start(connectionId, connections);

            while (!protocol.shouldTerminate() && connected && (read = in.read()) >= 0) {
                T nextMessage = encdec.decodeNextByte((byte) read);
                System.err.println("DEBUG Received message: " + nextMessage);
                if (nextMessage != null) {
                    // Process message (no response needed - protocol uses connections.send())
                    protocol.process(nextMessage);
                    /*if (response != null) {
                        out.write(encdec.encode(response));
                        out.flush();
                    }*/
                }
            }

        } catch (IOException ex) {
            ex.printStackTrace();
        }

    }

    @Override
    public void close() throws IOException {
        connected = false;
        sock.close();
    }

    @Override
    public void send(T msg) {
         try {
            if (out != null && connected) {
                synchronized (out) {
                    out.write(encdec.encode(msg));
                    out.flush();
                }
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
