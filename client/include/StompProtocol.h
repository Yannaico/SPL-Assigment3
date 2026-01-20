#ifndef STOMP_PROTOCOL_H
#define STOMP_PROTOCOL_H

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include "event.h"
#include <condition_variable>
using namespace std;

class StompProtocol {
private:
    string username;
    string password;
    int receiptIdCounter;
    int subscriptionIdCounter;
    bool loggedIn;
    mutex mtx;
    
    // Maps subscription ID to topic
    map<string, string> subscriptions;
    

    // Map: user -> game -> events
    map<string, map<string, names_and_events>> gameReports;
    
    string generateReceiptId();
    string generateSubscriptionId();
    
    std::mutex logoutMutex;
    std::condition_variable logoutCv;
    std::string expectedReceiptId = ""; 
    bool isLogoutComplete = false;


public:
    StompProtocol();
    
    // Frame builders
    string buildConnectFrame(const string& host, 
                                  const string& user, 
                                  const string& pass);
    
    string buildSubscribeFrame(const string& topic);
    
    string buildUnsubscribeFrame(const string& subscriptionId);
    
   string buildSendFrame(const string& topic, 
                        const Event& event, 
                        const string& user,  const string& filename);
    
    string buildDisconnectFrame();
    
    // Frame handlers
    void handleMessageFrame(const string& frame);
    
    // Game data management
    void saveGameEvent(const string& user, 
                      const string& gameName, 
                      const Event& event);
    
    void generateSummary(const string& gameName, 
                        const string& user, 
                        const string& outputFile);
    
    // State
    bool isLoggedIn() const { return loggedIn; }
    void setLoggedIn(bool status) { loggedIn = status; }

    string getSubscriptionIdByTopic(const string& topic);
    string getCurrentUsername() const { return username; }

};

#endif