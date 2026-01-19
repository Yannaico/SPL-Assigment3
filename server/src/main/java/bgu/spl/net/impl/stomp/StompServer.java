package bgu.spl.net.impl.stomp;

import bgu.spl.net.srv.Server;

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

        if (serverType.equals("tpc")) {
            System.err.println("DEBUG Starting Stomp Server in Thread Per Client mode");
            Server.threadPerClient(
                    port,
                    () -> new StompMessagingProtocolImpl(), // Protocol Factory 
                    () -> new StompMessageEncoderDecoder()         // Encoder Factory
            ).serve();

        } else if (serverType.equals("reactor")) {
            Server.reactor(
                    Runtime.getRuntime().availableProcessors(),
                    port,
                    () -> new StompMessagingProtocolImpl(), // Protocol Factory (Lambda)
                    () -> new StompMessageEncoderDecoder()         // Encoder Factory (Lambda)
            ).serve();

        } else {
            System.out.println("Unknown server type: " + serverType);
            System.out.println("Please use 'tpc' or 'reactor'.");
        }
    }
}
