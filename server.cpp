#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include <string>
#include <cstring>
#include <bitset>
#include <vector>
#include <chrono>
#include <stdint.h>

#define PORT 8060
#define WAIT_TIME 20

typedef struct
{
    struct header
    {
        int sequenceNumber;
    };
    char checksum[7];
    char content[500];
} Packet;

void error(const char* message) {
    perror(message);
    exit(1);
}

// Hamming
bool detectErrors(std::string message, int count, int sq, float ratio) {
    bool e = false;
    int n = 0, p_n = 0, i = 0;
	while (n > (int)(2*i) - (i + 1)) {
		p_n++;
		i++;
	}
	int k = 0;

	// traverse the message
    int ml = message.length();
	for (i = 0; i < ml; i++) {
		if (i == ((int)(2* k) - 1)) {
			message[i] = 0;
            if(!e) {
                if(count < (int)(ratio * 512)) {
                    std::cout << "Corrupted, sequence number: " << sq << std::endl;
                    return true;
                }
            }
		}
	}
    return e;
}

std::vector<std::string> segment(std::string fileContent, int &packetCount) {
	std::vector<std::string> segments;
    unsigned int i, fileLen = fileContent.length();
    for(i = 0; i < fileLen; i+=512) {
        std::string segment = fileContent.length() - i < 512 ? fileContent.substr(i, fileContent.length()) : fileContent.substr(i, 512);
		segments.push_back(segment);
        packetCount++;
    }
	return segments;
}

int main() {

    std::cout << "-----------------\n";
    std::cout << "Server Started\n";
    std::cout << "-----------------\n";
    
    int serverSocket, readStatus, packetCount = 0;
    struct sockaddr_in serverAddress, clientAddress;
    socklen_t clientAddressLen;
    char buffer[512] = {0};

    // new server socket
    if ((serverSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        error("Error! Socket creation\n");
    }

    // binding the port
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    if ((bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress))) < 0)
    {
        close(serverSocket);
        error("Error! Binding\n");
    }

    
    clientAddressLen = sizeof(clientAddress);
    readStatus = recvfrom(serverSocket, buffer, 512, 0, (struct sockaddr*)&clientAddress, &clientAddressLen);
    if (readStatus < 0) { 
        close(serverSocket);
        error("Error! Reading\n");
    }

    std::string fileName(buffer, readStatus);
    std::ifstream infile(fileName);

    if (!infile)
    {
        sendto(serverSocket, "0 0", strlen("0 0"), 0, (struct sockaddr*)&clientAddress, clientAddressLen);
        close(serverSocket);
        error("File not found!\n");
    }

    std::stringstream sBuff;
    sBuff << infile.rdbuf();
    std::string fileContent = sBuff.str();
    infile.close();

    std::vector<std::string> segments = segment(fileContent, packetCount);
    std::string pkt = std::to_string(1) + " " + std::to_string(packetCount);
    const char* pkts = pkt.c_str();
    
    sendto(serverSocket, pkts, strlen(pkts), 0, (struct sockaddr*)&clientAddress, clientAddressLen);
    
    int i = 0;
    while(i < packetCount) {        
        // send packet
        const char* pktTs = segments[i].c_str();
        if (sendto(serverSocket, pktTs, strlen(pktTs), 0, (struct sockaddr*)&clientAddress, clientAddressLen) < 0) { 
            close(serverSocket);
            error("Error! Sending\n");
        }

        // acknowledgement
        readStatus = recvfrom(serverSocket, buffer, 512, 0, (struct sockaddr*)&clientAddress, &clientAddressLen);
        // wait 20ms    
        auto a = std::chrono::steady_clock::now();
        while ((std::chrono::steady_clock::now() - a) < std::chrono::milliseconds(WAIT_TIME)) continue;
        if (readStatus < 0) { 
            std::cout << "Nothing Received! Resending packet!" << std::endl;
            continue;
        } else {
            int ok, received;
            std::string ack(buffer, readStatus);
            std::stringstream ss(ack);
            ss >> ok >> received; 
            if(!received) {
                std::cout << "Nothing Received! Resending packet!" << std::endl;
                continue;
            } else {
                if(!ok) {
                    std::cout << "NACK Received!" << std::endl;
                    continue;
                } else {            
                    std::cout << "ACK Received!" << std::endl;
                }
            }
        }
        i++;
    }

    close(serverSocket);
    return 0;
}
