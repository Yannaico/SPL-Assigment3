package bgu.spl.net.impl.stomp;

import bgu.spl.net.api.MessageEncoderDecoder;

public class StompMessageEncoderDecoder implements MessageEncoderDecoder<StompFrame> {
    private byte[] bytes = new byte[1024];
    private int len = 0;
    private static final int MAX_MESSAGE_SIZE = 1 << 20;
    @Override
    public StompFrame decodeNextByte(byte nextByte) {
        if(nextByte == '\0'){
            System.err.println("\n DEBUG: Decoder found NULL char. Buffer len: " + len);
            return popMessage();
        }
        pushByte(nextByte);
        return null;
    }

    private void pushByte(byte nextByte) {
        if (len >= MAX_MESSAGE_SIZE) {
            throw new RuntimeException("Message size limit exceeded");
        }
        // if needed, resize the array
        if (len >= bytes.length) {
            //double the size of the array
            bytes = java.util.Arrays.copyOf(bytes, len * 2);
        }
        bytes[len++] = nextByte;   
    }
    // when we get here we have a full message
    private StompFrame popMessage() {
        String message = new String(bytes,0,len, java.nio.charset.StandardCharsets.UTF_8);
        len = 0;
        return StompFrame.fromString(message);
    }

    @Override
    public byte[] encode(StompFrame message) {
        StringBuilder strMessage = new StringBuilder(message.toString());
        strMessage.append('\0');
        return strMessage.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8);
    }
}
