package bgu.spl.net.impl.stomp;

import java.sql.Connection;

import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.srv.Connections;

public class StompMessagingProtocolImpl implements StompMessagingProtocol{
    private int connectionId;
    private Connections<String> connections;
    private boolean shouldTerminate = false;
    private boolean isLoggedIn = false;
    private String currentUser = null;

    @Override
    public void start(int connectionId, Connections connections) {
        // TODO Auto-generated method stub
        throw new UnsupportedOperationException("Unimplemented method 'start'");
    }
    @Override
    public void process(Object message) {
        // TODO Auto-generated method stub
        throw new UnsupportedOperationException("Unimplemented method 'process'");
    }

    @Override
    public boolean shouldTerminate() {
        // TODO Auto-generated method stub
        throw new UnsupportedOperationException("Unimplemented method 'shouldTerminate'");
    }

    
}
