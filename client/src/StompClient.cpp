#include <iostream>
#include <thread>
#include <string>
#include <sstream>
#include <vector>
#include "../include/ConnectionHandler.h"
#include "event.h"
using namespace std;


// helper function split String 
// splits a string by a delimiter (\0) into a vector of parts (tokens)
vector< string> split(const string& str, char delimiter) {
    vector< string> tokens;
    string token;
    istringstream tokenStream(str);
    while (getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// socket reader thread
//  function runs in a separate thread.
// its ONLY job is to listen to the server and process incoming messages.
void socketReaderThread(ConnectionHandler* handler) {
    while (handler->isConnected()) {
        string frame;
        
        // read from socket until '\0' (Blocking call - waits for data)
        // if false, it means connection is closed or error occurred.
        if (!handler->getFrameAscii(frame, '\0')) {
             cout << "Disconnected from server." <<  endl;
            break;
        }
        
        //process the frame based on the command
        if (frame.substr(0, 9) == "CONNECTED") {
             cout << "Login successful" <<  endl;
            handler->getProtocol().setLoggedIn(true);
            
        } else if (frame.substr(0, 5) == "ERROR") {
            // if error, print it and close connection
             cerr << "Error from server:\n" << frame <<  endl;
            handler->close(); 
            handler->getProtocol().setLoggedIn(false); // Update status
            break;
            
        } else if (frame.substr(0, 7) == "MESSAGE") {
            // delegate business logic to the protocol class
            handler->getProtocol().handleMessageFrame(frame);
            
        } else  if (frame.substr(0, 7) == "RECEIPT") {
            // Check if it's a logout receipt
           size_t pos = frame.find("receipt-id:");
            if (pos != std::string::npos) {
                std::string receiptId = frame.substr(pos + 11);
                receiptId = receiptId.substr(0, receiptId.find('\n'));

                // Notify the protocol about the logout receipt
                //main thread will wake up and close the socket
                handler->getProtocol().processLogoutReceipt(receiptId);
            }
        }
    }
}

// --- main thread: user input handler ---
int main(int argc, char* argv[]) {
    // pointers to manage the connection and the listener thread
    ConnectionHandler* handler = nullptr;
    thread* readerThread = nullptr;
    
    string line;
    
    // loop forever, reading lines from keyboard (Standard Input)
    while (getline(cin, line)) {
        vector<string> tokens = split(line, ' ');
        
        // Skip empty lines
        if (tokens.empty()) continue;
        
        string command = tokens[0];
        
        // --- command: LOGIN ---
        if (command == "login") {
            // validation checks if there are enough arguments
            if (tokens.size() != 4) {
                cerr << "Usage: login host:port username password" << endl;
                continue;
            }
            // check if already connected
            if (handler != nullptr && handler->isConnected()) {
                cerr << "The client is already logged in, log out before trying again" << endl;
                continue;
            }
            
            // parse Host and Port
            string hostPort = tokens[1];

            //find separator ':'
            size_t colonPos = hostPort.find(':');
            if (colonPos == string::npos) {
                cerr << "Invalid host:port format" << endl;
                continue;
            }
            // extract host and port
             string host = hostPort.substr(0, colonPos);
            short port =  stoi(hostPort.substr(colonPos + 1));
            
            // connect physically (TCP Handshake)
            handler = new ConnectionHandler(host, port);
            if (!handler->connect()) {
                 cerr << "Could not connect to server" <<  endl;
                delete handler;
                handler = nullptr;
                continue;
            }
            
            // start the listener thread immediately!
            // We need it running BEFORE we send the CONNECT frame, 
            // so we can catch the CONNECTED response.
            readerThread = new thread(socketReaderThread, handler);

            // send STOMP CONNECT frame
             string connectFrame = handler->getProtocol().buildConnectFrame(host, tokens[2], tokens[3]);
            handler->sendFrameAscii(connectFrame, '\0');            
        }
        
        // --- Checks for other commands ---
        // Verify we are connected before trying to send anything
        else if (handler == nullptr || !handler->isConnected()) {
             cerr << "Not connected to server" <<  endl;
            continue;
        }
        
        // --- Command: JOIN ---
        else if (command == "join") {
            if (tokens.size() != 2) {
                 cerr << "Usage: join game_name" <<  endl;
                continue;
            }
             string gameName = tokens[1];
            // protocol builds the SUBSCRIBE frame
             string frame = handler->getProtocol().buildSubscribeFrame("/" + gameName);
            handler->sendFrameAscii(frame, '\0');
             cout << "Joined channel " << gameName <<  endl;
        }
        
        // --- Command: EXIT ---
        else if (command == "exit") {
            if (tokens.size() != 2) {
                 cerr << "Usage: exit game_name" <<  endl;
                continue;
            }
            //get game name
             string gameName = tokens[1];
            // we need to find the Subscription ID to unsubscribe
             string subId = handler->getProtocol().getSubscriptionIdByTopic("/" + gameName);
            
            if (subId == "") {
                 cerr << "Error: You are not subscribed to " << gameName <<  endl;
                continue;
            }
            // build and send the UNSUBSCRIBE frame
             string frame = handler->getProtocol().buildUnsubscribeFrame(subId);
             ///seinding....
            handler->sendFrameAscii(frame, '\0');
             cout << "Exited channel " << gameName <<  endl;
        }
        
        // --- Command: REPORT ---
        else if (command == "report") {
            if (tokens.size() != 2) {
                 cerr << "Usage: report filename" <<  endl;
                continue;
            }
             string filename = tokens[1];
            
            // Parse the JSON file (provided logic)
            try {
                names_and_events nae = parseEventsFile(filename);
                
                std::string gameName = nae.team_a_name + "_" + nae.team_b_name;
                std::string topic = "/" + gameName;
                
                for (const Event& event : nae.events) {
                    std::string sendFrame = handler->getProtocol().buildSendFrame(
                        topic, event, handler->getProtocol().getCurrentUsername(), filename);
                    
                    handler->sendFrameAscii(sendFrame, '\0');
                    
                    handler->getProtocol().saveGameEvent(handler->getProtocol().getCurrentUsername(), gameName, event);
                }
              

            } catch (const std::exception& e) {
                // Cattura sia file non trovato che errori JSON
                std::cerr << "Error processing report file: " << e.what() << std::endl;
            }
        }
        
        // --- Command: SUMMARY ---
        else if (command == "summary") {
            if (tokens.size() != 4) {
                 cerr << "Usage: summary game_name user outputfile" <<  endl;
                continue;
            }
            // Delegate logic to protocol (writes to file)
            handler->getProtocol().generateSummary(tokens[1], tokens[2], tokens[3]);
        }
        
        // --- command: LOGOUT ---
        else if (command == "logout") {
            //  send DISCONNECT frame
             string frame = handler->getProtocol().buildDisconnectFrame();
            handler->sendFrameAscii(frame, '\0');
            
            // mark as logged out internally
            handler->getProtocol().setLoggedIn(false);

            //  wait for logout to complete (wait for receipt)    
             handler->getProtocol().waitForLogout(); 
            
            //  close resources
            handler->close(); // This will cause socketReaderThread to exit its loop
            
            //closing the reader thread will also exit the loop
            // wait for the reader thread to finish
            if (readerThread != nullptr) {
                readerThread->join(); // wait for the thread to finish and die
                delete readerThread;
                readerThread = nullptr;
            }
            // finally delete the connection handler
            delete handler;
            handler = nullptr;
            
             cout << "Logged out" <<  endl;
        }
        else {
             cerr << "Unknown command: " << command <<  endl;
        }
    }
    
    // --- Cleanup (if user pressed Ctrl+C or input ended without logout)  
    // make sure we close the connection and join the thread
    if (handler != nullptr) {
        handler->close();
        if (readerThread != nullptr) {
            readerThread->join();
            delete readerThread;
        }
        delete handler;
    }
    
    return 0;
}