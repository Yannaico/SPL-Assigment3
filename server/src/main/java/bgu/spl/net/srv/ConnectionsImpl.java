package bgu.spl.net.srv;
import java.io.IOException;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

import bgu.spl.net.impl.stomp.StompFrame;

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
                T msgToSend = addsubscriptionToMsg(msg, id, channel);
                send(id, msgToSend);
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
    public void subscribe(String channel, int connectionId, String subscriptionId){
        topics.putIfAbsent(channel, new ConcurrentHashMap<>());
        topics.get(channel).put(connectionId, subscriptionId);
    }

    @Override
    public void unsubscribe(String channel, int connectionId, String subscriptionId)
    {
        ConcurrentHashMap<Integer, String> subscribers = topics.get(channel);
        if(subscribers != null){
            subscribers.remove(connectionId);
            if (subscribers.isEmpty()) {
                topics.remove(channel);
            }
        }
    }
    public void addConnection(int connectionId, ConnectionHandler<T> handler) {
        connections.put(connectionId, handler);
    }
    private T addsubscriptionToMsg(T msg, int connectionId, String channel) {
        if (msg instanceof StompFrame) {
           
            StompFrame cloned = new StompFrame((StompFrame) msg);
            
            ConcurrentHashMap<Integer, String> subscribers = topics.get(channel);
            String subscriptionId = subscribers.get(connectionId);
            if (subscriptionId != null) {
                cloned.addHeader("subscription", subscriptionId);
            }
            
            // Add message-id (server-unique)
            cloned.addHeader("message-id", String.valueOf(System.nanoTime()));
            
            return (T) cloned;
        }
        
        return msg;
    }
}
