#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <bits/stdc++.h>
#include <arpa/inet.h>
#include <unistd.h>
#define MAX_LENGTH 65535

using namespace std;
int main(int argc, char const *argv[])
{
	if(argc != 4) {
		cerr<<"Usage: "<<argv[0]<<" <serverIPAddr>:<port> <user-name> <passwd>\n";
		exit(1);

	}
	char* ip_port = (char*) argv[1];
	const char* serverIPAddr = strtok(ip_port, ":");
	const char* portNum = strtok(NULL, ":");
	const string userName = argv[2];
	const string passwd = argv[3];
	// const char* portNum = port_num;
	
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	struct addrinfo hints;
	struct addrinfo* server_info;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	int server_status = getaddrinfo(serverIPAddr, portNum, &hints, &server_info);
	char s[INET6_ADDRSTRLEN];
	inet_ntop(server_info->ai_family, &((struct sockaddr_in *)server_info->ai_addr)->sin_addr, s, sizeof s);

	// connect with client on given port and address
	int connect_status = connect(sockfd, server_info->ai_addr, server_info->ai_addrlen);
	if(connect_status == -1) {
		cerr<<"Connection to server "<<serverIPAddr<<" on port "<<portNum<<" failed\n";
		exit(2);
	}
	cout<<"ConnectDone: "<<serverIPAddr<<":"<<portNum<<"\n";
	

	// send username and password
	string temp = "User: " + userName + " Pass: " + passwd;
	const char* authDetails = temp.c_str();
	int msgLength = strlen(authDetails);
	int sentLength = send(sockfd, authDetails, msgLength, 0);
	if(sentLength == -1) {
		cerr<<"Sending failed"<<endl;
	}
	else if(sentLength != msgLength) {
		cerr<<"Complete message not sent"<<endl;
	}
	char welcomeMsg[MAX_LENGTH];
	recv(sockfd, welcomeMsg, MAX_LENGTH, 0);
	cout<<welcomeMsg;

	// send LIST
	const char* command = "LIST";
	send(sockfd, command, strlen(command), 0);
	char message[MAX_LENGTH];
	recv(sockfd, message, MAX_LENGTH, 0);
	cout<<message;

	// sending quit
	command = "quit";
	send(sockfd, command, strlen(command), 0);
	close(sockfd);
	return 0;
}