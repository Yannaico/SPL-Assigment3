package bgu.spl.net.srv;
import java.io.IOException;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

import bgu.spl.net.impl.stomp.StompFrame;

public class ConnectionsImpl implements Connections<StompFrame> {
    private Map<Integer, ConnectionHandler<StompFrame>> connections = new ConcurrentHashMap<>();
    //Map <"Topic" <ConnectionId, SubscriptionId>>
    private Map<String,ConcurrentHashMap<Integer, String>> topics = new ConcurrentHashMap<>();

    @Override
    public boolean send(int connectionId, StompFrame msg){
        ConnectionHandler<StompFrame> handler = connections.get(connectionId);
        if(handler != null){
            handler.send(msg);
            return true;
        }
        return false;
    }

    @Override
    public void send(String channel, StompFrame msg){
        ConcurrentHashMap<Integer, String> subscribers = topics.get(channel);
        if(subscribers != null){
            Set<Integer> connectionIds = subscribers.keySet();
            for(Integer id : connectionIds){
                StompFrame temp = msg;//create a new frame to add subscription header (JAVA)
                temp.addHeader("subscription:", subscribers.get(id));
                send(id, temp);
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
        topics.putIfAbsent(channel, new ConcurrentHashMap<>());
        topics.get(channel).put(connectionId, subscriptionId);
    }

    @Override
    public void unsubscribe(String channel, int connectionId, String subscriptionId) throws IOException
    {
        ConcurrentHashMap<Integer, String> subscribers = topics.get(channel);
        if(subscribers != null){
            subscribers.remove(connectionId);
            if (subscribers.isEmpty()) {
                topics.remove(channel);
            }
        }
    }
}
