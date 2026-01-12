package bgu.spl.net.srv;

import java.io.IOException;

public interface Connections<T> {

    boolean send(int connectionId, T msg);

    void send(String channel, T msg);

    void disconnect(int connectionId);

    //new methods for subscribe and unsubscribe
    void subscribe(String channel, int connectionId, String subscriptionId) throws IOException;
    void unsubscribe(String channel, int connectionId, String subscriptionId) throws IOException;
}
