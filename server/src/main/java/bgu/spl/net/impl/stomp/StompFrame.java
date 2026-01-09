package bgu.spl.net.impl.stomp;

import java.util.HashMap;
import java.util.Map;

public class StompFrame {
    private String command;
    private Map<String,String> headers = new HashMap<>();
    private String body;

    public StompFrame(String commands)
    {
        this.command= commands;
    }

    public String getCommand()
    {
        return command;

    }
    public void addHeader(String key,String value)
    {
        headers.put(key, value);
    }
    public String getHeader(String key)
    {
        return headers.get(key);
    }
    public void setBody(String body)
    {
        this.body=body;
    }
    public String getBody()
    {
        return this.body;
    }

    public static StompFrame fromString(String rawMessage)
    {

        StompFrame ret=null;
        int i=0;

        if(rawMessage==null || rawMessage.length()==0)
        {
            return null;
        }
        String[] lines = rawMessage.split("\n");   

        //COMMAND
        ret= new StompFrame(lines[i].trim());//create new StompFrame with Command string 
        i++;
       
       
        //HEADERS -until end of lines or an empty line 
        for(i=1; i<lines.length && !lines[i].isEmpty(); i++)
        {
            String[] parts = lines[i].split(":",2);
            //Add new Header to frame 
            if(parts.length ==2)
            {
                ret.addHeader(parts[0].trim(), parts[1].trim());
            }     
        }

       
       //BODY - build body until end of message
       

       
        StringBuilder bodyBuilder = new StringBuilder();
     
        //if we got to an empty line
        if(i!= lines.length)
        {   
            i++;//contuing after empty line
            for(;i<lines.length;i++)
            {
                bodyBuilder.append(lines[i]);
                
                //add new line
                if (i < lines.length - 1) {
                    bodyBuilder.append("\n");
                }
            }
        }

        ret.setBody(bodyBuilder.toString());


        return ret;
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        
        // 1. Command
        sb.append(command).append("\n");
        
        // 2. Headers
        // (Map<String, String>)
        for (Map.Entry<String, String> header : headers.entrySet()) {
            sb.append(header.getKey())
            .append(":")
            .append(header.getValue())
            .append("\n");
        }
        
        // 3. Empty Line _ SEPERATION
        sb.append("\n");
        
        // 4. Body
        if (body != null && !body.isEmpty()) {
            sb.append(body);
        }
        
        return sb.toString();
    }
}
