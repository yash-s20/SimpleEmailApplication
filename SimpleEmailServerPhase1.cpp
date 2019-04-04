#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <bits/stdc++.h>
#include <arpa/inet.h>
#include <unistd.h>
#define MAX_LENGTH 65535
// #define MYPORT 3490 // the port users will be connecting to
#define BACKLOG 10 
using namespace std;

// main
int main(int argc, char const *argv[])
{
	if(argc != 3) {
		cerr<<"Usage: "<<argv[0]<<" <portNum> <passwdfile>"<<'\n';
		exit(1);
	}
	const char* portNum = argv[1];
	const char* passwdfile = argv[2];



	// beej's reference

	struct sockaddr_storage their_addr;
    socklen_t addr_size;
    struct addrinfo hints, *res;
    int yes = 1;
    int sockfd, new_fd;
    memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;  // use IPv4 
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, portNum, &hints, &res); //copy address information to res

	//intializing socket descriptor
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

	//setsockopt
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

	//binding
	int bind_status = ::bind(sockfd, res->ai_addr, res->ai_addrlen);
	if(bind_status == -1) {
		cerr<<"Error: Binding failed"<<'\n';
		exit(2);
	}
	else {
		cout<<"BindDone: "<<portNum<<'\n';
	}
	freeaddrinfo(res); //we don't need this now
	
	ifstream passfile;
	passfile.open(string(passwdfile));
	// checking if the passfile is correct
	if(!passfile.is_open()) {
		cerr<<"Error: Password file either does not exist or is not readable"<<'\n';
		exit(3);
	}

	// listening on the fd
	int listen_status = listen(sockfd, BACKLOG);
	if(listen_status != -1) {

		cout<<"ListenDone: "<<portNum<<'\n';
	}

	addr_size = sizeof their_addr;

	// accepting a new client connection and initializing its file descriptor
	new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
	if(new_fd != -1) {
		// finding the host address and port
		char hoststr[NI_MAXHOST];
		char portstr[NI_MAXSERV];
		int rc = getnameinfo((struct sockaddr *)&their_addr, sizeof(struct sockaddr_storage), hoststr, sizeof(hoststr), portstr, sizeof(portstr), NI_NUMERICHOST | NI_NUMERICSERV);
		
		cout<<"Client: "<<hoststr<<":"<<portstr<<'\n';
	}
	close(sockfd); // not listening for any new clients

	char* auth = new char[MAX_LENGTH + 1];

	// receiving username and password
	int recv_status = recv(new_fd, auth, MAX_LENGTH, 0);


	// finding the username and constructing the welcome
	char* dummy = strtok(auth, " ");
	const string userName = string(strtok(NULL, " "));
	dummy = strtok(NULL, " ");
	const string passwd = string(strtok(NULL, " "));
	const char* welcomeMsg = (string("Welcome ") + userName + "\n").c_str();
	string users;
	string passes;
	bool user = false;
	bool pass = false;
	while(passfile>>users) {
		user = (users.compare(userName) == 0);
		passfile>>passes;
		if(user) {
			pass = (passes.compare(passwd) == 0);
			if(pass) break;
			else {
				// user with wrong password
				cout<<"Wrong Passwd\n";
				close(new_fd);
				passfile.close();
				exit(0);
			}
		}
	}
	if(!user) {
		// no good user
		cout<<"Invalid User\n";
		close(new_fd);
		passfile.close();
		exit(0);
	}
	passfile.close();
	cout<<welcomeMsg;
	
	// sending welcome message
	send(new_fd, welcomeMsg, strlen(welcomeMsg), 0);
	char* quit = new char[MAX_LENGTH + 1];
	// receiving quit
	recv_status = recv(new_fd, quit, MAX_LENGTH, 0);
	
	if(string(quit).compare("quit")==0) {
		cout<<"Bye "<<userName<<"\n";
		close(new_fd);
	}
	else {
		cout<<"Unknown command\n";
		close(new_fd);
	}
	return 0;
}
