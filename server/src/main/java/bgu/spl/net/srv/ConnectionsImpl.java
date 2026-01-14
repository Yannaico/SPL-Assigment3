package bgu.spl.net.srv;
import java.io.IOException;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

public class ConnectionsImpl<T> implements Connections<T> {
    private Map<Integer, ConnectionHandler<T>> connections = new ConcurrentHashMap<>();
    //Map <"Topic" <ConnectionId, SubscriptionId>>
    private Map<String,ConcurrentHashMap<Integer, String>> topics = new ConcurrentHashMap<>();

    @Override
    public boolean send(int connectionId, T msg){
        ConnectionHandler<T> handler = connections.get(connectionId);
        if(handler != null){
            handler.send(msg);
            return true;
        }
        return false;
    }

    @Override
    public void send(String channel, T msg){
        ConcurrentHashMap<Integer, String> subscribers = topics.get(channel);
        if(subscribers != null){
            Set<Integer> connectionIds = subscribers.keySet();
            for(Integer id : connectionIds){
                
                send(id, msg);
            }
        }
    }

    @Override
    public void disconnect(int connectionId){
        connections.remove(connectionId);//remove user from connections
        for(ConcurrentHashMap<Integer, String> subscribers : topics.values()){ //remove user from all topics he subscribed to
            subscribers.remove(connectionId);
        }

    }

    @Override
    public void subscribe(String channel, int connectionId, String subscriptionId) throws IOException{

    }

    @Override
    public void unsubscribe(String channel, int connectionId, String subscriptionId) throws IOException
    {

    }

}
