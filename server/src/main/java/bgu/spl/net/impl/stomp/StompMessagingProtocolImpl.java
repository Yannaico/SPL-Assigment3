package bgu.spl.net.impl.stomp;

import java.sql.Connection;

import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.srv.Connections;

public class StompMessagingProtocolImpl implements StompMessagingProtocol<StompFrame> {
    private int connectionId;
    private Connections<StompFrame> connections;
    private boolean shouldTerminate = false;
    private boolean isLoggedIn = false;
    private String currentUser = null;

    @Override
    public void start(int connectionId, Connections connections) {
        this.connectionId = connectionId;
        this.connections = connections;
    }
    @Override
    public void process(StompFrame frame) {
        String command = frame.getCommand();
        // Check if user is logged in for commands other than CONNECT
        if(!isLoggedIn && !command.equals("CONNECT")) {
            //Create ERROR frame
            StompFrame errorFrame = StompFrame.createErrorFrame("User not logged in", "You must log in before sending other commands.");
            //Send error frame to client
            connections.send(connectionId, errorFrame);
        }
        else{
            switch (command) {
                case "CONNECT":
                    handleConnect(frame);
                    break;
                case "SEND":
                    handleSend(frame);
                    break;
                case "SUBSCRIBE":
                    handleSubscribe(frame);
                    break;
                case "UNSUBSCRIBE":
                    handleUnsubscribe(frame);
                    break;
                case "DISCONNECT":
                    handleDisconnect(frame);
                    break;
                default:
                    //Create ERROR frame for unknown command
                    StompFrame errorFrame = StompFrame.createErrorFrame("Unknown command", "The command '" + command + "' is not recognized.");
                    //Send error frame to client
                    connections.send(connectionId, errorFrame);
                    shouldTerminate = true;
                    break;
        }
    }


    }

    private void handleDisconnect(StompFrame frame) {
        // TODO Auto-generated method stub
        throw new UnsupportedOperationException("Unimplemented method 'handleDisconnect'");
    }
    private void handleUnsubscribe(StompFrame frame) {
        // TODO Auto-generated method stub
        throw new UnsupportedOperationException("Unimplemented method 'handleUnsubscribe'");
    }
    private void handleSubscribe(StompFrame frame) {
        // TODO Auto-generated method stub
        throw new UnsupportedOperationException("Unimplemented method 'handleSubscribe'");
    }
    private void handleSend(StompFrame frame) {
        // TODO Auto-generated method stub
        throw new UnsupportedOperationException("Unimplemented method 'handleSend'");
    }
    private void handleConnect(StompFrame frame) {
        if(isLoggedIn) {
            //Create ERROR frame for already logged in user
            StompFrame errorFrame = StompFrame.createErrorFrame("Already logged in", "You are already logged in.");
            //Send error frame to client
            connections.send(connectionId, errorFrame);
            return;
        }
        String username = frame.getHeader("login");;
        String passcode = frame.getHeader("passcode");
        String acceptVersion = frame.getHeader("accept-version");

        if (username == null || passcode == null) {
            StompFrame errorFrame = StompFrame.createErrorFrame("Missing credentials", "Login or passcode header is missing.");
            connections.send(connectionId, errorFrame);
            this.shouldTerminate = true;
            return;
        }
        // #TODO: Here you would normally check the username and passcode against a database or in-memory store
        // #TODO: check database functions are enough for this assignment or need to be extended or connectionImpl.java
        this.isLoggedIn = true;
        StompFrame connectedFrame = new StompFrame("CONNECTED");
        connectedFrame.addHeader("version", "1.2");
        connections.send(connectionId, connectedFrame);
    }
    @Override
    public boolean shouldTerminate() {
        return shouldTerminate;
    }

    
}
