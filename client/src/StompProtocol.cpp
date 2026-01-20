#include "StompProtocol.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <vector>

using namespace std;

//initializes the protocol state and counters
StompProtocol::StompProtocol() 
    : username(""), password(""), receiptIdCounter(0), 
      subscriptionIdCounter(0), loggedIn(false) {}

//ID Generation Helpers

string StompProtocol::generateReceiptId() {
    lock_guard<mutex> lock(mtx); //  thread safety
    return to_string(++receiptIdCounter);
}

string StompProtocol::generateSubscriptionId() {
    lock_guard<mutex> lock(mtx); //  thread safety
    return to_string(++subscriptionIdCounter);
}

// Frame Builders 


string StompProtocol::buildConnectFrame(const string& host, 
                                        const string& user, 
                                        const string& pass) {
    username = user;
    password = pass;
    
    // Building the frame 
    string frame = "CONNECT\n";
    frame += "accept-version:1.2\n";
    frame += "host:stomp.cs.bgu.ac.il\n";
    frame += "login:" + user + "\n";
    frame += "passcode:" + pass + "\n";
    frame += "\n"; // Empty line marks end of headers
    
    return frame;
}

string StompProtocol::buildSubscribeFrame(const string& topic) {
    string subId = generateSubscriptionId();
    string receiptId = generateReceiptId();
    
    // Map the subscription ID to the topic (Thread Safe)
    {
        lock_guard<mutex> lock(mtx);
        subscriptions[subId] = topic;
    }
    
    string frame = "SUBSCRIBE\n";
    frame += "destination:" + topic + "\n";
    frame += "id:" + subId + "\n";
    frame += "receipt:" + receiptId + "\n";
    frame += "\n";
    
    return frame;
}

string StompProtocol::buildUnsubscribeFrame(const string& subId) {
    string receiptId = generateReceiptId();
    
    string frame = "UNSUBSCRIBE\n";
    frame += "id:" + subId + "\n";
    frame += "receipt:" + receiptId + "\n";
    frame += "\n";
    
    // remove the subscription from our local map
    {
        lock_guard<mutex> lock(mtx);
        subscriptions.erase(subId);
    }
    
    return frame;
}

string StompProtocol::buildSendFrame(const string& topic, 
                                     const Event& event, 
                                     const string& user,  const string& filename) {
    string frame = "SEND\n";
    frame += "destination:" + topic + "\n";
   
    

    // Optional: Include filename if available (not in Event class,
    if (!filename.empty()) {
        frame += "file-name:" + filename + "\n";
    }
    
    frame += "\n"; // end of Headers

    // start of Body (Assignment format)
    frame += "user: " + user + "\n";
    frame += "team a: " + event.get_team_a_name() + "\n";
    frame += "team b: " + event.get_team_b_name() + "\n";
    frame += "event name: " + event.get_name() + "\n";
    frame += "time: " + to_string(event.get_time()) + "\n";
    
    // add General Updates
    frame += "general game updates:\n";
    for (auto& kv : event.get_game_updates()) {
        frame += kv.first + ": " + kv.second + "\n";
    }
    
    // add Team A Updates
    frame += "team a updates:\n";
    for (auto& kv : event.get_team_a_updates()) {
        frame += kv.first + ": " + kv.second + "\n";
    }
    
    // add Team B Updates
    frame += "team b updates:\n";
    for (auto& kv : event.get_team_b_updates()) {
        frame += kv.first + ": " + kv.second + "\n";
    }
    
    // Add Description
    frame += "description:\n" + event.get_discription() + "\n";
    
    return frame;
}

string StompProtocol::buildDisconnectFrame() {
    string receiptId = generateReceiptId();
    
    // --- LOGOUT TRACKING SETUP ---
    {
        lock_guard<mutex> lock(logoutMutex);
        expectedReceiptId = receiptId;
        isLogoutComplete = false;
    }
    // --------------------------------
    
    string frame = "DISCONNECT\n";
    frame += "receipt:" + receiptId + "\n";
    frame += "\n";
    
    return frame;
}
//frame procceing logic

void StompProtocol::handleMessageFrame(const string& frame) {
    // Variables to store parsed data
    string user, teamA, teamB, eventName, description;
    int time = 0;
    map<string, string> gameUpdates, teamAUpdates, teamBUpdates;
    
    string section = ""; // tracks if we are in "general", "team a", or "description"
    bool inBody = false; // tracks if we passed the headers
    
    // manual line splitting using string::find
    size_t startPos = 0;
    size_t endPos = frame.find('\n');
    
    //until we reach the end of the frame - >endPos != no position
    while (endPos != string::npos) {
        // Extract the current line
        string line = frame.substr(startPos, endPos - startPos);
        
        // --- FIX FOR \r (Carriage Return) ---
        // Windows/Network lines often end in \r\n. We must remove \r 
        // otherwise comparisons like (line == "description:") will fail.
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        // --- HEADER SKIP LOGIC ---
        // The body starts after the first empty line.
        if (!inBody) {
            if (line.empty()) {
                inBody = true; // Found the empty line, Body starts next
            }
            // Move to next line
            startPos = endPos + 1;
            endPos = frame.find('\n', startPos);
            continue; 
        }
        
        // --- BODY PARSING LOGIC ---
        
        //  data Fields
        if (line.find("user: ") == 0) user = line.substr(6);
        else if (line.find("team a: ") == 0) teamA = line.substr(8);
        else if (line.find("team b: ") == 0) teamB = line.substr(8);
        else if (line.find("event name: ") == 0) eventName = line.substr(12);
        else if (line.find("time: ") == 0) {
            try { time = stoi(line.substr(6)); } catch (...) { time = 0; }
        }
        
        // Section Detection
        else if (line == "general game updates:") section = "general";
        else if (line == "team a updates:") section = "team_a";
        else if (line == "team b updates:") section = "team_b";
        else if (line == "description:") {
            section = "description";
            // Once we hit description, everything else is the description text.
            // We read until the end of the frame string manually.
            if (endPos + 1 < frame.length()) {
                description = frame.substr(endPos + 1);
                // Clean \r from description if needed (optional but good practice)
                // Note: This takes the rest of the frame as description.
            }
            break; // Stop loop, we handled the rest as description
        }
        
        // 3. Key-Value Parsing (inside a section)
        else if (!line.empty()) {
            size_t colonPos = line.find(':');
            if (colonPos != string::npos) {
                string key = line.substr(0, colonPos);
                string value = line.substr(colonPos + 1);
                
                // Trim leading space from value
                if (!value.empty() && value[0] == ' ') value = value.substr(1);
                
                if (section == "general") gameUpdates[key] = value;
                else if (section == "team_a") teamAUpdates[key] = value;
                else if (section == "team_b") teamBUpdates[key] = value;
            }
        }
        
        // Move to next line
        startPos = endPos + 1;
        endPos = frame.find('\n', startPos);
    }
    
    // save and display
    string gameName = teamA + "_" + teamB;
    Event event(teamA, teamB, eventName, time, 
                gameUpdates, teamAUpdates, teamBUpdates, description);
    
    saveGameEvent(user, gameName, event);
    
    // Output to console
    cout << "Displaying update from user: " << user << "\n";
    cout << "Game: " << gameName << "\n";
    cout << "Event: " << eventName << "\n";
    cout << description << "\n" << endl;
}

// data Management: Saves the event to the map
void StompProtocol::saveGameEvent(const string& user, 
                                  const string& gameName, 
                                  const Event& event) {
    lock_guard<mutex> lock(mtx); // Critical section: protecting the map
    
    // If the game entry doesn't exist for this user, CREATE it
    if (gameReports[user].find(gameName) == gameReports[user].end()) {
        gameReports[user][gameName] = names_and_events();
        gameReports[user][gameName].team_a_name = event.get_team_a_name();
        gameReports[user][gameName].team_b_name = event.get_team_b_name();
    }
    
    // Add the event to the list
    gameReports[user][gameName].events.push_back(event);
}

// generates the final summary file
void StompProtocol::generateSummary(const string& gameName, 
                                    const string& user, 
                                    const string& outputFile) {
    // Check if data exists
    if (gameReports.find(user) == gameReports.end() || 
        gameReports[user].find(gameName) == gameReports[user].end()) {
        cerr << "No reports found for game " << gameName << endl;
        return;
    }
    
    names_and_events& reportData = gameReports[user][gameName];
    
    // Open File
    ofstream out(outputFile);
    if (!out.is_open()) {
        cerr << "Cannot open file: " << outputFile << endl;
        return;
    }
    
    //Statistics 
    map<string, string> generalStats;
    map<string, string> teamAStats;
    map<string, string> teamBStats;
    
    for (const Event& event : reportData.events) {
        for (auto& kv : event.get_game_updates()) generalStats[kv.first] = kv.second;
        for (auto& kv : event.get_team_a_updates()) teamAStats[kv.first] = kv.second;
        for (auto& kv : event.get_team_b_updates()) teamBStats[kv.first] = kv.second;
    }
    
    // Write to File
    out << reportData.team_a_name << " vs " << reportData.team_b_name << "\n";
    out << "Game stats:\n";
    
    out << "General stats:\n";
    for (auto& kv : generalStats) out << kv.first << ": " << kv.second << "\n";
    
    out << reportData.team_a_name << " stats:\n";
    for (auto& kv : teamAStats) out << kv.first << ": " << kv.second << "\n";
    
    out << reportData.team_b_name << " stats:\n";
    for (auto& kv : teamBStats) out << kv.first << ": " << kv.second << "\n";
    
    out << "Game event reports:\n";
    // (Ideally, sort events by time here before printing, 
    // keeping simplistic as per request for "simplification")
    for (const Event& event : reportData.events) {
        out << event.get_time() << " - " << event.get_name() << ":\n\n";
        out << event.get_discription() << "\n\n\n";
    }
    
    out.close();
    cout << "Summary written to " << outputFile << endl;
}

// Utility: Find Subscription ID by Topic
string StompProtocol::getSubscriptionIdByTopic(const string& topic) {
    lock_guard<mutex> lock(mtx);

    for(auto const& [id,subTopic]: subscriptions) {
        if(subTopic == topic) {
            return id;
        }
    }
    return "";
}


// Waits for logout to complete
void StompProtocol::waitForLogout() {
    unique_lock<mutex> lock(logoutMutex);
   //wait until isLogoutComplete is true
    logoutCv.wait(lock, [this] { return isLogoutComplete; });
}

//RECEIPT processing for logout
bool StompProtocol::processLogoutReceipt(const string& receiptId) {
    lock_guard<mutex> lock(logoutMutex);
    
    // Check if this receipt ID matches the expected logout receipt ID
    if (!expectedReceiptId.empty() && receiptId == expectedReceiptId) {
        isLogoutComplete = true; // set the flag we are done
        logoutCv.notify_all();   // notify waiting threads
        return true;
    }
    return false;
}
