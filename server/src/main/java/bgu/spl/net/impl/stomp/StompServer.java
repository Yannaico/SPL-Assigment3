package bgu.spl.net.impl.stomp;

import bgu.spl.net.srv.Server;
import bgu.spl.net.impl.data.Database;

public class StompServer {

    public static void main(String[] args) {
        if (args.length < 2) {
            System.out.println("Usage: StompServer <port> <server-type>");
            System.out.println("server-type: tpc (Thread Per Client) or reactor");
            return;
        }

        int port = 0;
        try {
            port = Integer.parseInt(args[0]);
        } catch (NumberFormatException e) {
            System.out.println("Error: Port must be a number.");
            return;
        }

        String serverType = args[1];
        Server<StompFrame> server = null;

        if (serverType.equals("tpc")) {
            server = Server.threadPerClient(
                    port,
                    () -> new StompMessagingProtocolImpl(), // Protocol Factory 
                    () -> new StompMessageEncoderDecoder()         // Encoder Factory
            );

        } else if (serverType.equals("reactor")) {
            server = Server.reactor(
                    Runtime.getRuntime().availableProcessors(),
                    port,
                    () -> new StompMessagingProtocolImpl(), // Protocol Factory (Lambda)
                    () -> new StompMessageEncoderDecoder()         // Encoder Factory (Lambda)
            );

        } else {
            System.out.println("Unknown server type: " + serverType);
            return;
        }
        // Start a separate thread to listen for "report" command
        new Thread(() -> {
            java.util.Scanner scanner = new java.util.Scanner(System.in);
            // Read lines from standard input
            while (scanner.hasNextLine()) {
                String line = scanner.nextLine();
                // If the line is "report", print the report
                if (line.trim().equalsIgnoreCase("report")) {
                    // Print the report from the Database
                    Database.getInstance().printReport();
                }
            }
        }).start();
        // Start the server
        server.serve();
    }
}
