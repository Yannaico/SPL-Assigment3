#include "../include/ConnectionHandler.h"

using boost::asio::ip::tcp;

using std::cin;
using std::cout;
using std::cerr;
using std::endl;
using std::string;

// constructor: initializes the 'io_service' (the engine) and the 'socket' (the connection object).
ConnectionHandler::ConnectionHandler(string host, short port) 
    : host_(host), port_(port), io_service_(), socket_(io_service_), connected_(false) {}

// destructor: ensures the connection is closed when the object is destroyed.
ConnectionHandler::~ConnectionHandler() {
    close();
}

// attempts to connect to the server.
bool ConnectionHandler::connect() {
    std::cout << "Starting connect to " << host_ << ":" << port_ << std::endl;
    try {
        // Create an endpoint (IP + port) - The "address" of the server application.
        tcp::endpoint endpoint(boost::asio::ip::address::from_string(host_), port_); 
        
        boost::system::error_code error;
        
        // try to connect the socket to the endpoint.
        socket_.connect(endpoint, error);
        
        // if the socket reports an error (e.g., server unreachable), throw an exception.
        if (error)
            throw boost::system::system_error(error);
            
        connected_ = true;
    }
    catch (std::exception &e) {
        std::cerr << "Connection failed (Error: " << e.what() << ')' << std::endl;
        return false;
    }
    return true;
}

// reads'bytesToRead' bytes from the network.
// TCP is a stream, we might not get all bytes in one shot, so we loop.

bool ConnectionHandler::getBytes(char bytes[], unsigned int bytesToRead) {
    size_t tmp = 0; // counts how many bytes we have read so far OUR PROGRESS*****
    boost::system::error_code error;
    try {
        // We lock specifically for reading if multiple threads share this handler (safety)
        // lock_guard<std::mutex> lock(socketMutex_); 
        
        // loop until we have read exactly the amount requested
        while (!error && bytesToRead > tmp) {
            // read_some: Read whatever is available currently. 
            // it puts it in the buffer at position 'bytes + tmp'.
            // it asks to read 'bytesToRead - tmp' (REMAINING- THings to reads).
            tmp += socket_.read_some(boost::asio::buffer(bytes + tmp, bytesToRead - tmp), error);
        }
        if (error)
            throw boost::system::system_error(error);
    } catch (std::exception &e) {
        std::cerr << "recv failed (Error: " << e.what() << ')' << std::endl;
        return false;
    }
    return true;
}

// Sends EXACTLY 'bytesToWrite' to the network.
bool ConnectionHandler::sendBytes(const char bytes[], int bytesToWrite) {
    int tmp = 0; // OUR PROGRESS****counts how many bytes sent so far*******
    boost::system::error_code error;
    try {
        // lock_guard<std::mutex> lock(socketMutex_);
        
        // Loop until all data is sent
        //meaning temp is <= bytesToWrite meaning we still have more to send
        while (!error && bytesToWrite > tmp) {
            // write_some: Sends a chunk of data returns how much was actually sent.
            //add to temp what we manged to send in this iteration
            tmp += socket_.write_some(boost::asio::buffer(bytes + tmp, bytesToWrite - tmp), error);
        }
        if (error)
            throw boost::system::system_error(error);
    } catch (std::exception &e) {
        std::cerr << "recv failed (Error: " << e.what() << ')' << std::endl;
        return false;
    }
    return true;
}

//read until '\n' (New Line).

bool ConnectionHandler::getLine(std::string &line) {
    return getFrameAscii(line, '\n');
}

// send a string followed by '\n'.
bool ConnectionHandler::sendLine(std::string &line) {
    return sendFrameAscii(line, '\n');
}

// CRITICAL FOR STOMP: Reads byte by byte until it finds the 'delimiter'.
//  frames end with a null character ('\0').
bool ConnectionHandler::getFrameAscii(std::string &frame, char delimiter) {
    char ch;
    // Try to read 1 byte at a time until we hit the delimiter.
    try {
        do {
            // read 1 byte into 'ch'
            if (!getBytes(&ch, 1)) {
                return false; // connection closed or error
            }
            // if it's not the delimiter, append it to our result string
            if (ch != delimiter) 
                frame.append(1, ch);
        } while (delimiter != ch);
    } catch (std::exception &e) {
        std::cerr << "recv failed2 (Error: " << e.what() << ')' << std::endl;
        return false;
    }
    return true;
}

//  stomp sends the frame string and appends the delimiter.
//  send the frame content, then send '\0' .
bool ConnectionHandler::sendFrameAscii(const std::string &frame, char delimiter) {
    // Send the actual content
    bool result = sendBytes(frame.c_str(), frame.length());
    if (!result) return false;
    
    // send the delimiter \0
    return sendBytes(&delimiter, 1);
}

//  closes the socket connection.
void ConnectionHandler::close() {
    try {
        connected_ = false;
        socket_.close();
    } catch (...) {
        std::cout << "closing failed: connection already closed" << std::endl;
    }
}