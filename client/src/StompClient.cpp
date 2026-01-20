#include <iostream>
#include <thread>
#include <string>
#include <sstream>
#include <vector>
#include "../include/ConnectionHandler.h"
#include "event.h"

void socketReaderThread(ConnectionHandler* handler) {
    while (handler->isConnected()) {
        std::string frame;
        
        if (!handler->getFrameAscii(frame, '\0')) {
            break;
        }

        if (frame.substr(0, 9) == "CONNECTED") {
            std::cout << "Login successful" << std::endl;
            handler->getProtocol().setLoggedIn(true);
            
        } else if (frame.substr(0, 5) == "ERROR") {
            std::cerr << "Error from server:\n" << frame << std::endl;
            handler->close();
            break;
            
        } else if (frame.substr(0, 7) == "RECEIPT") {
            size_t pos = frame.find("receipt-id:");
            if (pos != std::string::npos) {
                std::string receiptId = frame.substr(pos + 11);
                receiptId = receiptId.substr(0, receiptId.find('\n'));
            }
            
        } else if (frame.substr(0, 7) == "MESSAGE") {
            handler->getProtocol().handleMessageFrame(frame);
        }
    }
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}

int main(int argc, char* argv[]) {
    ConnectionHandler* handler = nullptr;
    std::thread* readerThread = nullptr;
    
    std::string line;
    
    while (std::getline(std::cin, line)) {
        std::vector<std::string> tokens = split(line, ' ');
        
        if (tokens.empty()) continue;
        
        std::string command = tokens[0];
        
        if (command == "login") {
            if (tokens.size() != 4) {
                std::cerr << "Usage: login host:port username password" << std::endl;
                continue;
            }
            
            if (handler != nullptr && handler->isConnected()) {
                std::cerr << "The client is already logged in, log out before trying again" << std::endl;
                continue;
            }
            
            std::string hostPort = tokens[1];
            size_t colonPos = hostPort.find(':');
            
            if (colonPos == std::string::npos) {
                std::cerr << "Invalid host:port format" << std::endl;
                continue;
            }
            
            std::string host = hostPort.substr(0, colonPos);
            short port = std::stoi(hostPort.substr(colonPos + 1));
            
            std::string username = tokens[2];
            std::string password = tokens[3];
            
            handler = new ConnectionHandler(host, port);
            
            if (!handler->connect()) {
                delete handler;
                handler = nullptr;
                continue;
            }
            readerThread = new std::thread(socketReaderThread, handler);

            std::string connectFrame = handler->getProtocol().buildConnectFrame(host, username, password);
            handler->sendFrameAscii(connectFrame, '\0');            
        }
        else if (command == "join") {

            if (tokens.size() != 2) {
                std::cerr << "Usage: join game_name" << std::endl;
                continue;
            }
            
            if (handler == nullptr || !handler->isConnected()) {
                std::cerr << "Not connected to server" << std::endl;
                continue;
            }
            if (!handler->getProtocol().isLoggedIn()) {
                std::cerr << "Not logged in. Please wait for login to complete." << std::endl;
                continue;
            }
            std::string gameName = tokens[1];
            std::string topic = "/" + gameName;
            
            std::string subscribeFrame = handler->getProtocol().buildSubscribeFrame(topic);

            handler->sendFrameAscii(subscribeFrame, '\0');
            
            std::cout << "Joined channel " << gameName << std::endl;
        }
        
        else if (command == "exit") {
            if (tokens.size() != 2) {
                std::cerr << "Usage: exit game_name" << std::endl;
                continue;
            }
            
            if (handler == nullptr || !handler->isConnected()) {
                std::cerr << "Not connected to server" << std::endl;
                continue;
            }
            
            std::string gameName = tokens[1];
            
            std::string topic = "/" + gameName;
            std::string subId = handler->getProtocol().getSubscriptionIdByTopic(topic);
            if (subId == "") {
                std::cerr << "Error: You are not subscribed to " << gameName << std::endl;
                continue;
            }
            std::string unsubscribeFrame = handler->getProtocol().buildUnsubscribeFrame(subId);
            handler->sendFrameAscii(unsubscribeFrame, '\0');
            
            std::cout << "Exited channel " << gameName << std::endl;
        }
        
        else if (command == "report") {
            if (tokens.size() != 2) {
                std::cerr << "Usage: report filename" << std::endl;
                continue;
            }
            
            if (handler == nullptr || !handler->isConnected()) {
                std::cerr << "Not connected to server" << std::endl;
                continue;
            }
            
            std::string filename = tokens[1];
            
              try {
                names_and_events nae = parseEventsFile(filename);
                
                std::string gameName = nae.team_a_name + "_" + nae.team_b_name;
                std::string topic = "/" + gameName;
                
                for (const Event& event : nae.events) {
                    std::string sendFrame = handler->getProtocol().buildSendFrame(
                        topic, event, handler->getProtocol().getCurrentUsername()
                    );
                    handler->sendFrameAscii(sendFrame, '\0');
                    
                    handler->getProtocol().saveGameEvent(handler->getProtocol().getCurrentUsername(), gameName, event);
                }

            } catch (const std::exception& e) {
                std::cerr << "Error processing report file: " << e.what() << std::endl;
            }
        }
        else if (command == "summary") {
            if (tokens.size() != 4) {
                std::cerr << "Usage: summary game_name user outputfile" << std::endl;
                continue;
            }
            
            std::string gameName = tokens[1];
            std::string user = tokens[2];
            std::string outputFile = tokens[3];
            
            handler->getProtocol().generateSummary(gameName, user, outputFile);
        }
        else if (command == "logout") {
            if (handler == nullptr || !handler->isConnected()) {
                std::cerr << "Not connected to server" << std::endl;
                continue;
            }
            
            std::string disconnectFrame = handler->getProtocol().buildDisconnectFrame();
            handler->sendFrameAscii(disconnectFrame, '\0');
            
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            handler->close();
            
            if (readerThread != nullptr) {
                readerThread->join();
                delete readerThread;
                readerThread = nullptr;
            }
            
            delete handler;
            handler = nullptr;
            
            std::cout << "Logged out" << std::endl;
        }
        else {
            std::cerr << "Unknown command: " << command << std::endl;
        }
    }
    
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