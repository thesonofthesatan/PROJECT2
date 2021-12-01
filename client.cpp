#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <cmath>
#include <random>
#include <algorithm>

#define PORT 8060

// Hamming
bool detectErrors(std::string message, int count, int sq, float ratio, std::vector<int> corruptedSqns) {
    if((std::find(corruptedSqns.begin(), corruptedSqns.end(), sq) != corruptedSqns.end())) return false;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::bernoulli_distribution d(ratio);
    bool e = d(gen);
    int n = 0, p_n = 0, i = 0;
	while (n > (int)(2*i) - (i + 1)) {
		p_n++;
		i++;
	}
	int k = 0;

	// traverse the message
    int ml = message.length();
	for (i = 0; i < ml; i++) {
		if (((int)(2* k) - 1) >= k) {
			message[i] = 0;
            p_n = (int)(ratio * 512);
		}
	}
    if(e) {
        std::cout << "Corrupted, sequence number: " << sq << ", Sending NACK" << std::endl;
    }
    return e;
}

bool calcLoss(std::string message, int count, int sq, float ratio, std::vector<int> corruptedSqns) {
    if((std::find(corruptedSqns.begin(), corruptedSqns.end(), sq) != corruptedSqns.end())) return false;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::bernoulli_distribution d(ratio);
    bool e = d(gen);
    int n = 0, p_n = 0, i = 0;
	while (n > (int)(2*i) - (i + 1)) {
		p_n++;
		i++;
	}
	int k = 0;

	// traverse the message
    int ml = message.length();
	for (i = 0; i < ml; i++) {
		if (((int)(2* k) - 1) >= k) {
			message[i] = 0;
            p_n = (int)(ratio * 512);
		}
	}
    if(e) {
        std::cout << "Lost packet, sequence number: " << sq << ", Sending nothing. Waiting for server to resend" << std::endl;
    }
    return e;
}

std::string gremlin(std::string packetstring) {
    // nothing, corrupt, loss, complete loss
    int possibilities[] = {0, 1, 2, 3};
    std::string packetCorrupted = packetstring;

    // choose random error to inject
    int choice = possibilities[std::rand() % 5];
    switch (choice)
    {
    case 0:
        // unchanged
        break;
    case 1:
        packetCorrupted = packetstring.substr(0, packetstring.length() / 2);
        break;
    case 2:
        packetCorrupted = packetstring.substr(0, packetstring.length() - 0.3*packetstring.length());
        break;
    case 3:
        packetCorrupted = "";
        break;    
    default:
        break;
    }
    return packetstring;
}

int main(int argc, char** argv) {
    if(argc < 4) {
        std::cout << "Error! Usage: client <file_to_get>  <error_rate(0-1)> <loss_rate(0-1)>\n";
        exit(1);
    }

    int clientSocket, readStatus;
    struct sockaddr_in serverAddress;
    socklen_t serverAddressLen;
    char buffer[512] = {0};
    int packetCount = 0;

    //create a socket
    if ((clientSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Error! Socket creation\n");
        exit(1);
    }

    //server socket address
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    serverAddress.sin_addr.s_addr = inet_addr("10.0.2.15");  // PLEASE CHANGE THIS IP TO YOUR COMPUTER'S IP

    if (sendto(clientSocket, argv[1], strlen(argv[1]), 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
    {
        perror("Error! Sending\n");
        close(clientSocket);
        exit(1);
    }

    serverAddressLen = sizeof(serverAddress);
    readStatus = recvfrom(clientSocket, buffer, 512, 0, (struct sockaddr*)&serverAddress, &serverAddressLen);

    if (readStatus < 0) {
        perror("Error! Reading\n");
        close(clientSocket);
        exit(1);
    }

    int fileFound = 0;
    float damageProbability = atof(argv[2]);
    float lossProbability = atof(argv[3]);
    std::string pkts(buffer, readStatus);
    std::stringstream ss(pkts);
    ss >> fileFound >> packetCount;

    if(!fileFound) {
        std::cout << "Error! File not found\n";
        close(clientSocket);
        exit(1);
    }

    int damaged = (int) std::ceil(damageProbability * packetCount);
    int lost = (int) std::ceil(lossProbability * packetCount);
    std::vector<std::string> packets;
    std::vector<int> corruptedSqns, lostSqns;
    int i = 0, ec = 0, lc = 0, sqN = 0;
    while(i < packetCount) {
        // receive packet
        readStatus = recvfrom(clientSocket, buffer, 512, 0, (struct sockaddr*)&serverAddress, &serverAddressLen);
        if (readStatus < 0) {
            perror("Error! Reading\n");
            close(clientSocket);
            exit(1);
        }
        std::string pkt(buffer, readStatus);

        // pass packet to gremlin func 
        pkt = gremlin(pkt);

        // check for errors
        // using relative sequence numbers : first packe has sqn = 0
        if((lc < lost)  && calcLoss(pkt, lc, i, lossProbability, lostSqns)) {
            lostSqns.push_back(i);
            lc++;
            sendto(clientSocket, "0 0", strlen("0 0"), 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
            continue;
        }

        if((ec < damaged)  && detectErrors(pkt, ec, i, damageProbability, corruptedSqns)) {
            if(sqN % 2 == 0) {
                sqN++;
            }
            ec++;
            corruptedSqns.push_back(i);
            // send nak
            sendto(clientSocket, "0 1", strlen("0 1"), 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
            continue;
        }

        packets.push_back(pkt);

        // send ack
        sendto(clientSocket, "1 1", strlen("1 1"), 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
        i++;
        sqN++;
    }

    std::ofstream outfile("received.txt");
    for (int i = 0; i < packetCount; i++)
    {
        outfile << packets[i];
    }
    
    outfile.close();   

    close(clientSocket);
    return 0;
}
