package bgu.spl.net.impl.stomp;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;

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
    private Map<String, String> subscriptions = new ConcurrentHashMap<>();
    private static AtomicInteger messageIdCounter = new AtomicInteger(0);

    @Override
    public void start(int connectionId, Connections<StompFrame> connections) {
        this.connectionId = connectionId;
        this.connections = connections;
    }
    @Override
    public StompFrame process(StompFrame frame) {
        String command = frame.getCommand();
        // Check if user is logged in for commands other than CONNECT
        if(!isLoggedIn && !command.equals("CONNECT")) {
            sendError(frame, "User not logged in", "You must log in before sending other commands.");
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
                    sendError(frame, "Unknown command", "The command '" + command + "' is not recognized.");
                    shouldTerminate = true;
                    break;
        }
    }
    return null;
}

    private void handleDisconnect(StompFrame frame) {
        if (!isLoggedIn) {
            sendError(frame, "Not logged in", "You must be logged in to disconnect.");
            return;
        }

        String receipt = frame.getHeader("receipt");
        if (receipt != null) {
            sendReceipt(receipt);
        }

        // Perform logout
        Database.getInstance().logout(connectionId);
        this.shouldTerminate = true;
    }
    private void handleUnsubscribe(StompFrame frame) {
        if (!isLoggedIn) {
            sendError(frame, "Not logged in", "You must be logged in to unsubscribe.");
            return;
        }

        String requestId = frame.getHeader("id");

        if (requestId == null || requestId.isEmpty()) {
            sendError(frame, "Malformed UNSUBSCRIBE", "Missing 'id' header.");
            return;
        }
        // Check if subscribed with the given id
        String destination = subscriptions.remove(requestId);
        if (destination == null) {
            sendError(frame, "Subscription not found", "No active subscription found with ID: " + requestId);
            return;
        }

        connections.unsubscribe(destination, connectionId, requestId);

        // Handle receipt if requested
        String receipt = frame.getHeader("receipt");
        if (receipt != null) {
            sendReceipt(receipt);
        }
    }
    private void handleSubscribe(StompFrame frame) {
        if (!isLoggedIn) {
            sendError(frame, "Not logged in", "You must be logged in to subscribe.");
            return;
        }

        String destination = frame.getHeader("destination");
        String requestId = frame.getHeader("id");

        if (destination == null || destination.isEmpty() || requestId == null || requestId.isEmpty()) {
            sendError(frame, "Malformed SUBSCRIBE", "Missing 'destination' or 'id' header.");            return;
        }

        // Check if already subscribed with the same id
        if (subscriptions.containsKey(requestId)) {
            sendError(frame, "Already subscribed", "You are already subscribed with id '" + requestId + "'.");
            return;
        }

        subscriptions.put(requestId, destination);
        
        connections.subscribe(destination, connectionId, requestId);

        // Handle receipt if requested
        String receipt = frame.getHeader("receipt");
        if (receipt != null) {
            sendReceipt(receipt);
        }
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

        if (!subscriptions.containsValue(destination)) {
            sendError(frame, "Not subscribed", "You cannot send to '" + destination + "' without subscribing first.");
            return;
        }

        // Track file upload if 'file-name' header is present
        String filename = frame.getHeader("file-name"); 
        if (filename != null) {
            Database.getInstance().trackFileUpload(currentUser, filename, destination);
        }

        StompFrame messageFrame = new StompFrame("MESSAGE");
        messageFrame.addHeader("destination", destination);
        messageFrame.addHeader("message-id", String.valueOf(messageIdCounter.incrementAndGet()));
        
        // Copy the body and content (Game updates, user, team names, etc.)
        // We don't validate them, we just pass them through.
        messageFrame.setBody(frame.getBody());

        // Send the MESSAGE frame (not the SEND frame)
        connections.send(destination, messageFrame);

        // Handle receipt if requested
        String receipt = frame.getHeader("receipt");
        if (receipt != null) {
            sendReceipt(receipt);
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
                sendConnectedFrame();
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
 * Sends a RECEIPT frame to acknowledge that a command with a receipt header was processed.
 * @param receiptId The receipt ID from the original frame's receipt header.
 */
private void sendReceipt(String receiptId) {
    StompFrame receiptFrame = new StompFrame("RECEIPT");
    receiptFrame.addHeader("receipt-id", receiptId);
    connections.send(connectionId, receiptFrame);
}

/**
 * Sends an ERROR frame in the specific format required by the assignment and closes the connection.
 * * @param faultyFrame The original frame that caused the error (used to extract receipt-id and print in body).
 * @param messageHeader A short description of the error (goes into the 'message' header).
 * @param detailedInfo A detailed explanation (goes into the body).
 */
private void sendError(StompFrame faultyFrame, String messageHeader, String detailedInfo) {
    StompFrame errorFrame = new StompFrame("ERROR");

    errorFrame.addHeader("message", messageHeader);

    if (faultyFrame != null) {
        String receipt = faultyFrame.getHeader("receipt");
        if (receipt != null) {
            errorFrame.addHeader("receipt-id", receipt);
        }
    }

    // Construct the body in the requested format
    StringBuilder body = new StringBuilder();
    body.append("The message:\n");
    body.append("-----\n");
    
    if (faultyFrame != null) {
        // Assuming your StompFrame.toString() returns the frame representation (Command + Headers + Body)
        body.append(faultyFrame.toString()); 
    } else {
        body.append("(No frame content available)");
    }
    
    body.append("\n-----\n");
    body.append(detailedInfo);
    errorFrame.setBody(body.toString());
    connections.send(connectionId, errorFrame);
    
    if (isLoggedIn) {
        Database.getInstance().logout(connectionId);
        isLoggedIn = false;
        currentUser = null;
    }
    this.shouldTerminate = true;
    connections.disconnect(connectionId);
}
}