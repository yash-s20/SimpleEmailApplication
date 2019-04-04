#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <bits/stdc++.h>
#define MAX_LENGTH 65535
#define PACKET_SIZE 32767
// #define MYPORT 3490 // the port users will be connecting to
#define BACKLOG 10 
using namespace std;

string file_name(int num, string ext) {
	return to_string(num) + "." + ext;
}

string add_file_name(string path, int num, string ext) {
	string ret = path + "/" + to_string(num) + "." + ext;
	return ret;
}

// https://stackoverflow.com/questions/5840148/how-can-i-get-a-files-size-in-c
ifstream::pos_type filesize(const char* filename) {
    ifstream in(filename, ifstream::ate | ifstream::binary);
    return in.tellg(); 
}

int main(int argc, char const *argv[])
{
	if(argc != 4) {
		cerr<<"Usage: "<<argv[0]<<" <portNum> <passwdfile> <user-database>"<<'\n';
		exit(1);
	}
	const char* portNum = argv[1];
	const char* passwdfile = argv[2];

	struct sockaddr_storage their_addr;
    socklen_t addr_size;
    struct addrinfo hints, *res;
    int yes = 1;
    int sockfd, new_fd;
    memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;  // use IPv4 or IPv6, whichever
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	getaddrinfo(NULL, portNum, &hints, &res);
	
	//intializing socket descriptor
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	//setsockopt
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	
	// some debugging check by output ip address
	char ipstr[INET_ADDRSTRLEN];
	void *addr;
	struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;           
	addr = &(ipv4->sin_addr);
	inet_ntop(res->ai_family, addr, ipstr, sizeof ipstr);
	
	int bind_status = ::bind(sockfd, res->ai_addr, res->ai_addrlen);
	if(bind_status == -1) {
		cerr<<"Error: Binding failed"<<'\n';
		exit(2);
	}
	else {
		cout<<"BindDone: "<<portNum<<'\n';
	}
	freeaddrinfo(res); // we don't need this now
	
	ifstream passfile;
	passfile.open(string(passwdfile));
	// checking if the passfile is correct
	if(!passfile.is_open()) {
		cerr<<"Error: Password file either does not exist or is not readable"<<'\n';
		exit(3);
	}
	
	// checking user database
	passfile.close();
	struct stat info;
	const char* pathName = argv[3];
	if( stat( pathName, &info ) != 0 || !(info.st_mode & S_IFDIR)) {
	    cerr<<"Cannot access: "<<pathName<<". Not a directory or does not exist\n";
	    exit(4);
	}
	
	// listening on the fd
	int listen_status = listen(sockfd, BACKLOG);
	if(listen_status != -1) {

		cout<<"ListenDone: "<<portNum<<'\n';
	}
	addr_size = sizeof their_addr;


	while(true) {
		// accepting a new client connection and initializing its file descriptor
		// we won't close sockfd now since we might have new clients after this closes
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
		if(new_fd != -1) {
			// finding the host address and port
			char hoststr[NI_MAXHOST];
			char portstr[NI_MAXSERV];
			int rc = getnameinfo((struct sockaddr *)&their_addr, sizeof(struct sockaddr_storage), hoststr, sizeof(hoststr), portstr, sizeof(portstr), NI_NUMERICHOST | NI_NUMERICSERV);
			cout<<"Client: "<<hoststr<<":"<<portstr<<'\n';
		}
		else {
			break;
		}
		char* auth = new char[MAX_LENGTH + 1];
		// receiving username and password
		int recv_status = recv(new_fd, auth, MAX_LENGTH, 0);
		
		// finding the username and constructing the welcome
		char* dummy = strtok(auth, " ");
		string userName = string(strtok(NULL, " "));
		dummy = strtok(NULL, " ");
		string passwd = string(strtok(NULL, " "));
		string welcomeMsg = "Welcome " + userName + "\n";
		bool user = false;
		bool pass = false;
		string users;
		string passes;
		passfile.open(string(passwdfile));
		while(passfile>>users) {
			if(pass) break;
			if(user && !pass) break;
			passfile>>passes;
			user = (users.compare(userName) == 0);
			if(user) {
				pass = (passes.compare(passwd) == 0);
				if(!pass) {
					// user with wrong password
					cout<<"Wrong Passwd\n";
					break;
				}
			}
		}
		if(user && !pass) {
			passfile.close();
			close(new_fd);
			continue;
		}
		if(!user) {
			cout<<"Invalid User\n";
			passfile.close();
			close(new_fd);
			continue;
		}
		if(!pass) {
			passfile.close();
			close(new_fd);
			continue;
		}
		passfile.close();
		
		// sending welcome message
		cout<<welcomeMsg;
		int recv_bytes;
		send(new_fd, welcomeMsg.c_str(), strlen(welcomeMsg.c_str()), 0);

		// reading his files
		map<int, string> extensionMap; // map of file number to extension
		map<int, int> fileSizeMap; // map of file number to size
		int file_count = 0;
		DIR * dirp;
		struct dirent * entry;
		string userpath = pathName;
		if(pathName[userpath.length() - 1]=='/') {
			userpath.append(userName);
		}
		else {
			userpath.append("/" + userName);
		}
		dirp = opendir(userpath.c_str());
		if(not dirp) {
			// error handling
			cout<<"Message Read Fail\n";
			close(new_fd);
			continue;
		}

		// counting and reading files, keeping extensions for later use
		while ((entry = readdir(dirp)) != NULL) {
		    if (entry->d_type == DT_REG) {
		    	file_count++;
		    	char* fileName = entry->d_name;
		    	ifstream f;
		    	string filePath = userpath + "/" + fileName;
		    	int fileSize = (int) filesize(filePath.c_str());
		    	int num = stoi(strtok(fileName, "."));
		    	string ext = strtok(NULL, ".");
		    	extensionMap[num] = ext;
		    	fileSizeMap[num] = fileSize;
		    }
		}

		while(true) {
			char command[MAX_LENGTH + 1];
			recv_bytes = recv(new_fd, command, MAX_LENGTH, 0);
			if(recv_bytes == 0 || recv_bytes == -1) {
				break;
			};
			command[recv_bytes] = '\0';
			if(string(command).compare("quit")==0) {
				// quit message
				cout<<"Bye "<<userName<<endl;
				close(new_fd);
			}
			else if((string(command).substr(0,min(5, (int)strlen(command)))).compare("RETRV")==0) {
				// RETRV message
				string c  = command;
				int fileNumber = stoi(c.substr(6));
				if(extensionMap.find(fileNumber)!=extensionMap.end()) {
					// here's where the file sending happens

					string fileToSend = add_file_name(userpath, fileNumber, extensionMap[fileNumber]);
					string filename = file_name(fileNumber, extensionMap[fileNumber]);
					string filesize_str = to_string(fileSizeMap[fileNumber]);
					char* details =(char *) (filename + ":" + filesize_str).c_str();
					send(new_fd, details, strlen(details), 0);
					char rbuffer[PACKET_SIZE];
					cout<<userName<<": Transferring Message "<<fileNumber<<"\n";
					usleep(10);
					ifstream rFile(fileToSend, ios::in | ios::binary);
					int x = fileSizeMap[fileNumber];
					while(x > PACKET_SIZE) {
						rFile.read(rbuffer, PACKET_SIZE);
						send(new_fd, rbuffer, PACKET_SIZE, 0);
						x -= PACKET_SIZE;
					}
					rFile.read(rbuffer, x);
					send(new_fd, rbuffer, x, 0);
				}
				else {
					cout<<"Message Read Fail\n";
					close(new_fd);
					break;
				}
			}
			else {
				cout<<"Unknown command\n";
				cout<<command<<endl;
				close(new_fd);
				break;
			}
		}
	}
	close(sockfd);
	return 0;
}