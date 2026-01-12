package bgu.spl.net.srv;
import java.io.IOException;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

public class ConnectionsImpl<T> implements Connections<T> {
    private Map<Integer, ConnectionHandler<T>> connections = new ConcurrentHashMap<>();
    private Map<String, Set<Integer>> topics = new ConcurrentHashMap<>();

    @Override
    public boolean send(int connectionId, T msg){
        return false;
    }

    @Override
    public void send(String channel, T msg){

    }

    @Override
    public void disconnect(int connectionId){

    }

    @Override
    public void subscribe(String channel, int connectionId, String subscriptionId) throws IOException{

    }

    @Override
    public void unsubscribe(String channel, int connectionId, String subscriptionId) throws IOException
    {

    }

}
