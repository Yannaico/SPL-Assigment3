#include "StompProtocol.h"
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>

using namespace std;

StompProtocol::StompProtocol() 
    : username(""), password(""), receiptIdCounter(0), 
      subscriptionIdCounter(0), loggedIn(false) {}

string StompProtocol::generateReceiptId() {
    lock_guard<mutex> lock(mtx);
    return to_string(++receiptIdCounter);
}

string StompProtocol::generateSubscriptionId() {
    lock_guard<mutex> lock(mtx);
    return to_string(++subscriptionIdCounter);
}

string StompProtocol::buildConnectFrame(const string& host, 
                                        const string& user, 
                                        const string& pass) {
    username = user;
    password = pass;
    
    string frame = "";
    frame.append("CONNECT\n");
    frame.append("accept-version:1.2\n");
    frame.append("host:stomp.cs.bgu.ac.il\n");
    frame.append("login:").append(user).append("\n");
    frame.append("passcode:").append(pass).append("\n");
    frame.append("\n");
    
    return frame;
}

string StompProtocol::buildSubscribeFrame(const string& topic) {
    string subId = generateSubscriptionId();
    string receiptId = generateReceiptId();
    
    {
        lock_guard<mutex> lock(mtx);
        subscriptions[subId] = topic;
    }
    
    string frame = "";
    frame.append("SUBSCRIBE\n");
    frame.append("destination:").append(topic).append("\n");
    frame.append("id:").append(subId).append("\n");
    frame.append("receipt:").append(receiptId).append("\n");
    frame.append("\n");
    
    return frame;
}

string StompProtocol::buildUnsubscribeFrame(const string& subId) {
    string receiptId = generateReceiptId();
    
    string frame = "";
    frame.append("UNSUBSCRIBE\n");
    frame.append("id:").append(subId).append("\n");
    frame.append("receipt:").append(receiptId).append("\n");
    frame.append("\n");
    
    {
        lock_guard<mutex> lock(mtx);
        subscriptions.erase(subId);
    }
    
    return frame;
}

string StompProtocol::buildSendFrame(const string& topic, 
                                     const Event& event, 
                                     const string& user) {
    string frame = "";
    
    frame.append("SEND\n");
    frame.append("destination:").append(topic).append("\n");
    frame.append("user: ").append(user).append("\n");
    frame.append("team a: ").append(event.get_team_a_name()).append("\n");
    frame.append("team b: ").append(event.get_team_b_name()).append("\n");
    frame.append("event name: ").append(event.get_name()).append("\n");
    frame.append("time: ").append(to_string(event.get_time())).append("\n");
    
    frame.append("\n");
    
    frame.append("general game updates:\n");
    for (auto& kv : event.get_game_updates()) {
        frame.append(kv.first).append(": ").append(kv.second).append("\n");
    }
    
    frame.append("team a updates:\n");
    for (auto& kv : event.get_team_a_updates()) {
        frame.append(kv.first).append(": ").append(kv.second).append("\n");
    }
    
    frame.append("team b updates:\n");
    for (auto& kv : event.get_team_b_updates()) {
        frame.append(kv.first).append(": ").append(kv.second).append("\n");
    }
    
    // BODY - description
    frame.append("description:\n").append(event.get_discription()).append("\n");
    
    return frame;
}

string StompProtocol::buildDisconnectFrame() {
    string receiptId = generateReceiptId();
    
    string frame = "";
    frame.append("DISCONNECT\n");
    frame.append("receipt:").append(receiptId).append("\n");
    frame.append("\n");
    
    return frame;
}

void StompProtocol::handleMessageFrame(const string& frame) {
    istringstream iss(frame);
    string line;
    map<string, string> headers;
    
    getline(iss, line);
    
    while (getline(iss, line) && !line.empty() && line != "\r") {
        size_t colonPos = line.find(':');
        if (colonPos != string::npos) {
            string key = line.substr(0, colonPos);
            string value = line.substr(colonPos + 1);
            
            if (!value.empty() && value[0] == ' ') {
                value = value.substr(1);
            }
            
            if (!value.empty() && value.back() == '\r') {
                value.pop_back();
            }
            
            headers[key] = value;
        }
    }
    
    string bodyStream = "";
    while (getline(iss, line)) {
        bodyStream.append(line).append("\n");
    }
    
    std::string user = headers["user"];
    std::string teamA = headers["team a"];
    std::string teamB = headers["team b"];
    std::string gameName = teamA + "_" + teamB;
    std::string eventName = headers["event name"];
    std::string timeStr = headers["time"];
    int time = std::stoi(timeStr);
    
    // Parsing BODY to extract game updates
    std::map<std::string, std::string> gameUpdates;
    std::map<std::string, std::string> teamAUpdates;
    std::map<std::string, std::string> teamBUpdates;
    std::string description = "";
    
    std::istringstream bodyStream2(bodyStream);
    std::string section = "";
    
    while (std::getline(bodyStream2, line)) {
        if (line.find("general game updates:") == 0) {
            section = "general";
        } else if (line.find("team a updates:") == 0) {
            section = "team_a";
        } else if (line.find("team b updates:") == 0) {
            section = "team_b";
        } else if (line.find("description:") == 0) {
            section = "description";

            std::string descLine;
            while (std::getline(bodyStream2, descLine)) {
                description.append(descLine).append("\n");
            }
            if (!description.empty() && description.back() == '\n') {
                description.pop_back();
            }
            break;
        } else if (!line.empty() && line.find(':') != std::string::npos) {
            size_t colonPos = line.find(':');
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            
            if (!value.empty() && value[0] == ' ') {
                value = value.substr(1);
            }
            if (!value.empty() && value.back() == '\r') {
                value.pop_back();
            }
            
            if (section == "general") {
                gameUpdates[key] = value;
            } else if (section == "team_a") {
                teamAUpdates[key] = value;
            } else if (section == "team_b") {
                teamBUpdates[key] = value;
            }
        }
    }
    
    // Creat Event and save
    Event event(teamA, teamB, eventName, time, 
                gameUpdates, teamAUpdates, teamBUpdates, description);
    
    saveGameEvent(user, gameName, event);
    
    // Print to console
    std::cout << user << " - " << gameName << ":\n";
    std::cout << eventName << "\n\n";
    std::cout << description << "\n" << std::endl;
}

void StompProtocol::saveGameEvent(const string& user, 
                                  const string& gameName, 
                                  const Event& event) {
    lock_guard<mutex> lock(mtx);
    
    if (gameReports[user].find(gameName) == gameReports[user].end()) {
        gameReports[user][gameName] = names_and_events();
        
        // Extract team names from gameName (format: "USA_Canada")
        size_t underscorePos = gameName.find('_');
        if (underscorePos != string::npos) {
            gameReports[user][gameName].team_a_name = gameName.substr(0, underscorePos);
            gameReports[user][gameName].team_b_name = gameName.substr(underscorePos + 1);
        }
    }
    
    // Add event
    gameReports[user][gameName].events.push_back(event);
}

void StompProtocol::generateSummary(const string& gameName, 
                                    const string& user, 
                                    const string& outputFile) {
    ofstream out(outputFile);
    
    if (!out.is_open()) {
        cerr << "Cannot open file: " << outputFile << endl;
        return;
    }
    
    // Find reports for this game and user
    auto userIt = gameReports.find(user);
    if (userIt == gameReports.end()) {
        out << "No reports found for user " << user << endl;
        out.close();
        cout << "Summary written to " << outputFile << endl;
        return;
    }
    
    auto gameIt = userIt->second.find(gameName);
    if (gameIt == userIt->second.end()) {
        out << "No reports found for game " << gameName << endl;
        out.close();
        cout << "Summary written to " << outputFile << endl;
        return;
    }
    
    names_and_events& nae = gameIt->second;
    
    // Header (assignment format)
    out << nae.team_a_name << " vs " << nae.team_b_name << "\n";
    out << "Game stats:\n";
    out << "General stats:\n";
    
    // Aggregate general stats from all events
    map<string, string> generalStats;
    for (const Event& event : nae.events) {
        for (auto& kv : event.get_game_updates()) {
            generalStats[kv.first] = kv.second;  // Last value wins
        }
    }
    
    // Print general stats (lexicographic order)
    for (auto& kv : generalStats) {
        out << kv.first << ": " << kv.second << "\n";
    }
    
    // Team A stats
    out << nae.team_a_name << " stats:\n";
    map<string, string> teamAStats;
    for (const Event& event : nae.events) {
        for (auto& kv : event.get_team_a_updates()) {
            teamAStats[kv.first] = kv.second;
        }
    }
    for (auto& kv : teamAStats) {
        out << kv.first << ": " << kv.second << "\n";
    }
    
    // Team B stats
    out << nae.team_b_name << " stats:\n";
    map<string, string> teamBStats;
    for (const Event& event : nae.events) {
        for (auto& kv : event.get_team_b_updates()) {
            teamBStats[kv.first] = kv.second;
        }
    }
    for (auto& kv : teamBStats) {
        out << kv.first << ": " << kv.second << "\n";
    }
    
    // Game event reports
    out << "\nGame event reports:\n";
    
    // Sort events by time (handling halftime)
    vector<Event> sortedEvents = nae.events;
    sort(sortedEvents.begin(), sortedEvents.end(), 
         [](const Event& a, const Event& b) {
             // If one is "halftime", handle specially
             bool aBeforeHalftime = true;
             bool bBeforeHalftime = true;
             
             // Check if events have "before halftime" in game_updates
             auto aUpdates = a.get_game_updates();
             auto bUpdates = b.get_game_updates();
             
             if (aUpdates.find("before halftime") != aUpdates.end()) {
                 aBeforeHalftime = (aUpdates.at("before halftime") == "true");
             }
             
             if (bUpdates.find("before halftime") != bUpdates.end()) {
                 bBeforeHalftime = (bUpdates.at("before halftime") == "true");
             }
             
             // If one is before halftime and the other is after
             if (aBeforeHalftime && !bBeforeHalftime) return true;
             if (!aBeforeHalftime && bBeforeHalftime) return false;
             
             // Otherwise sort by time
             return a.get_time() < b.get_time();
         });
    
    // Print events
    for (const Event& event : sortedEvents) {
        out << event.get_time() << " - " << event.get_name() << ":\n";
        out << event.get_discription() << "\n\n";
    }
    
    out.close();
    cout << "Summary written to " << outputFile << endl;
}
string StompProtocol::getSubscriptionIdByTopic(const string& topic) {
    lock_guard<mutex> lock(mtx);
    
    for (auto const& [id, subTopic] : subscriptions) {
        if (subTopic == topic) {
            return id; 
        }
    }
    return ""; 
}