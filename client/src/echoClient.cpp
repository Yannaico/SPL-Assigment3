#include <stdlib.h>
#include "../include/ConnectionHandler.h"


/**
* this code assumes that the server replies the exact text the client sent it
*/
int main (int argc, char *argv[]) {
   
    //check if user sended enough arguments
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " host port" << endl << endl;
        return -1;
    }
    //saving host and port from arguments
    string host = argv[1];
    short port = atoi(argv[2]); //use atoi to convert from string to short
    

    //createing connection handler to talk with the server
    ConnectionHandler connectionHandler(host, port);

    //trying to connect to server
    if (!connectionHandler.connect()) {
        cerr << "Cannot connect to " << host << ":" << port << endl;
        return 1;
    }
	
	//from here we will see the rest of the ehco client implementation:
    while (1) {
        const short bufsize = 1024;
        char buf[bufsize];


        //read a line from the keyboard and puts \0 at the end of buf (without \n becuse we use getline and not cin)
        cin.getline(buf, bufsize);
		string line(buf); //create line from buf
		int len=line.length();



        //send the line to the server with the \n at the end (Automaticlly added in sendLine() function)
        if (!connectionHandler.sendLine(line)) {
            cout << "Disconnected. Exiting...\n" << endl;
            break;
        }
		//again connectionHandler.sendLine(line) appends '\n' to the message. Therefor we send len+1 bytes.
        cout << "Sent " << len+1 << " bytes to server" << endl;

        
        // we can use one of three options to read data from the server:
        //  read a fixed number of characters
        //  read a line (up to the newline character using the getline() buffered reader
        //  read up to the null character
        string answer;
        // get back an answer: by using the expected number of bytes (len bytes + newline delimiter)
        // we could also use: connectionHandler.getline(answer) and then get the answer without the newline char at the end
        
        //Waiting for the answer from the server blocking the client until we get the answer
        if (!connectionHandler.getLine(answer)) {
            cout << "Disconnected. Exiting...\n" << std::endl;
            break;
        }
        
		len=answer.length();
		// a C string must end with a 0 char delimiter.  When we filled the answer buffer from the socket
		// we filled up to the \n char - we must make sure now that a 0 char is also present. So we truncate last character.
        answer.resize(len-1);
        cout << "Reply: " << answer << " " << len << " bytes " << std::endl << std::endl;
        if (answer == "bye") {
            cout << "Exiting...\n" << std::endl;
            break;
        }
    }
    return 0;
}
