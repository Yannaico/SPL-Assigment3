package bgu.spl.net.impl.stomp;

import java.sql.Connection;
import bgu.spl.net.impl.data.Database;
import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.impl.data.LoginStatus;
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
        if (!isLoggedIn) {
            sendError(frame, "Not logged in", "You must be logged in to send messages.");
            return;
        }

        String destination = frame.getHeader("destination");
        if (destination == null || destination.isEmpty()) {
            sendError(frame, "Missing destination", "SEND frame must include a destination header.");
            return;
        }

        // Optionally, validate other headers if needed
        // String user = frame.getHeader("user");
        // String teamA = frame.getHeader("team a");
        // String teamB = frame.getHeader("team b");
        // String eventName = frame.getHeader("event name");
        // String time = frame.getHeader("time");
        // String description = frame.getHeader("description");
        // String body = frame.getBody();

        // Forward the frame to all subscribers of the destination
        connections.send(destination, frame);

        // Handle receipt if present
        String receipt = frame.getHeader("receipt");
        if (receipt != null) {
            StompFrame receiptFrame = new StompFrame("RECEIPT");
            receiptFrame.addHeader("receipt-id", receipt);
            connections.send(connectionId, receiptFrame);
        }
    }
    private void handleConnect(StompFrame frame) {
        if(isLoggedIn) {
            sendError(frame, "Already logged in", "You are already logged in.");
            return;
        }
        String username = frame.getHeader("login");
        String passcode = frame.getHeader("passcode");
        String acceptVersion = frame.getHeader("accept-version");
        String host = frame.getHeader("host");

        if (username == null || passcode == null || host == null) {
            sendError(frame, "Missing credentials", "Login or passcode header is missing.");
            return;
        }
        
        LoginStatus status = Database.getInstance().login(connectionId, username, passcode);
        switch (status) {
            case LOGGED_IN_SUCCESSFULLY:
            case ADDED_NEW_USER:
                this.currentUser = username;
                sendConnectedFrame();//
                break;
            case CLIENT_ALREADY_CONNECTED:
                sendError(frame, "Client already connected", "This client is already connected.");
                break;
            case ALREADY_LOGGED_IN:
                sendError(frame, "Already logged in", "This user is already logged in.");
                break;
            case WRONG_PASSWORD:
                sendError(frame, "Wrong password", "The passcode you entered is incorrect.");
                break;
            default:
                sendError(frame, "Login failed", "Login failed due to unknown reasons.");
                break;
        }
    }
    @Override
    public boolean shouldTerminate() {
        return shouldTerminate;
    }
    private void sendConnectedFrame() {
        this.isLoggedIn = true;
        StompFrame connectedFrame = new StompFrame("CONNECTED");
        connectedFrame.addHeader("version", "1.2");
        connections.send(connectionId, connectedFrame);
    }
/**
 * Sends an ERROR frame in the specific format required by the assignment and closes the connection.
 * * @param faultyFrame The original frame that caused the error (used to extract receipt-id and print in body).
 * @param messageHeader A short description of the error (goes into the 'message' header).
 * @param detailedInfo A detailed explanation (goes into the body).
 */
private void sendError(StompFrame faultyFrame, String messageHeader, String detailedInfo) {
    StompFrame errorFrame = new StompFrame("ERROR");

    // 1. Add the mandatory 'message' header with the short description
    // Source: "The ERROR frame SHOULD contain a message header with a short description" [cite: 358]
    errorFrame.addHeader("message", messageHeader);

    // 2. Handle 'receipt-id'
    // Source: "If the frame included a receipt header, the ERROR frame SHOULD set the receipt-id header..." [cite: 360]
    if (faultyFrame != null) {
        String receipt = faultyFrame.getHeader("receipt");
        if (receipt != null) {
            errorFrame.addHeader("receipt-id", receipt);
        }
    }

    // 3. Construct the body in the requested format
    // Format: "The message:" -> Separator -> Original Frame -> Separator -> Description
    StringBuilder body = new StringBuilder();
    body.append("The message:\n");
    body.append("-----\n");
    
    if (faultyFrame != null) {
        // Assuming your StompFrame.toString() returns the frame representation (Command + Headers + Body)
        // Note: Make sure it does not include the null terminator (\u0000) inside the body text.
        body.append(faultyFrame.toString()); 
    } else {
        body.append("(No frame content available)");
    }
    
    body.append("\n-----\n");
    body.append(detailedInfo);

    // 4. Send the frame
    // We append the body manually. Note: The toString() of the errorFrame usually handles command+headers.
    // We add the null terminator at the very end.
    String finalFrameString = errorFrame.toString() + body.toString() + "\u0000";
    connections.send(connectionId, finalFrameString);

    // 5. Close the connection
    // Source: "it MUST then close the connection just after sending the ERROR frame" [cite: 347]
    this.shouldTerminate = true;
}
}
