#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <bits/stdc++.h> 
#include <netinet/in.h>
#include <sys/time.h>
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

	fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
	int fdmax;
	// maximum file descriptor number
	// listening socket descriptor
	// newly accept()ed socket descriptor
	int listener;
	int newfd;
	struct sockaddr_storage remoteaddr; // client address
	socklen_t addrlen;
	char buf[256];    // buffer for client data
	int nbytes;
	char remoteIP[INET_ADDRSTRLEN];
	int yes=1;        // for setsockopt() SO_REUSEADDR, below
	int i, j, rv;
	// struct addrinfo *ai, *p;
	FD_ZERO(&master);    // clear the master and temp sets
	FD_ZERO(&read_fds);

	struct sockaddr_storage their_addr;
    socklen_t addr_size;
    struct addrinfo hints, *res;
    int sockfd, new_fd;
    memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;  // use IPv4 or IPv6, whichever
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	getaddrinfo(NULL, portNum, &hints, &res);
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

	char ipstr[INET_ADDRSTRLEN];
	void *addr;
	struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;           
	addr = &(ipv4->sin_addr);
	inet_ntop(res->ai_family, addr, ipstr, sizeof ipstr);
	// cout<<ipstr<<endl;

	int bind_status = ::bind(sockfd, res->ai_addr, res->ai_addrlen);


	if(bind_status == -1) {
		cerr<<"Error: Binding failed"<<'\n';
		exit(2);
	}
	else {
		cout<<"BindDone: "<<portNum<<'\n';
	}
	freeaddrinfo(res);
	ifstream passfile;
	passfile.open(string(passwdfile));
	if(!passfile.is_open()) {
		cerr<<"Error: Password file either does not exist or is not readable"<<'\n';
		exit(3);
	}
	
	passfile.close();
	struct stat info;
	const char* pathName = argv[3];
	if( stat( pathName, &info ) != 0 || !(info.st_mode & S_IFDIR)) {
	    cerr<<"Cannot access: "<<pathName<<". Not a directory or does not exist\n";
	    exit(4);
	}
	
	int listen_status = listen(sockfd, BACKLOG);
	if(listen_status != -1) {

		cout<<"ListenDone: "<<portNum<<'\n';
	}
	addr_size = sizeof their_addr;

	FD_SET(sockfd, &master);
	fdmax = sockfd;
	//some bookkeeping
	map<int, bool> authMap;
	map<int, string> authUser;
	map<int, map<int, string> > authFileExt;
	map<int, map<int, int> > authFileSize;
	map<int, string> authPath;

	for(;;) { // basically a while true loop
	    read_fds = master; // copy it
	    if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) { 
	        cerr<<"select\n";
			exit(5);
		}
	    // run through the existing connections looking for data to read
	    for(i = 0; i <= fdmax; i++) {

	        if (FD_ISSET(i, &read_fds)) { // either a new connection or a connected user
	            if (i == sockfd) {

	            	new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
					if(new_fd != -1) {
						char hoststr[NI_MAXHOST];
						char portstr[NI_MAXSERV];
						int rc = getnameinfo((struct sockaddr *)&their_addr, sizeof(struct sockaddr_storage), hoststr, sizeof(hoststr), portstr, sizeof(portstr), NI_NUMERICHOST | NI_NUMERICSERV);
						cout<<"Client: "<<hoststr<<":"<<portstr<<'\n';
						FD_SET(new_fd, &master);
						if(new_fd > fdmax) fdmax = new_fd;
					}
					else {
						cerr<<"accept fail\n";
						break;
					}
	            }
	            else {
	            	if(authMap.find(i)==authMap.end()) {
	            		//authenticate
	            		char* auth = new char[MAX_LENGTH + 1];
						int recv_status = recv(i, auth, MAX_LENGTH, 0);
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
									cout<<"Wrong Passwd\n";
									break;
								}
							}
						}
						if(user && !pass) {
							passfile.close();
							close(i);
							FD_CLR(i, &master);
							continue;
						}
						if(!user) {
							cout<<"Invalid User\n";
							passfile.close();
							close(i);
							FD_CLR(i, &master);
							continue;
						}
						if(!pass) {
							passfile.close();
							close(i);
							FD_CLR(i, &master);
							continue;
						}
						passfile.close();
						
						cout<<welcomeMsg;
						send(i, welcomeMsg.c_str(), strlen(welcomeMsg.c_str()), 0);
						//adding his authentication
						authMap[i] = true;
						authUser[i] = userName;
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
						dirp = opendir(userpath.c_str()); /* There should be error handling after this */
						if(not dirp) {
							cout<<"Message Read Fail\n";
							close(i);
							FD_CLR(i, &master);
							continue;
						}
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
						//more bookkeeping
						authFileExt[i] = extensionMap;
						authFileSize[i] = fileSizeMap;
						authPath[i] = userpath;
	            	}
	            	else {
	            		char command[MAX_LENGTH + 1];
						// cout<<authUser[i]<<" haha"<<endl;
						int recv_bytes = recv(i, command, MAX_LENGTH, 0);
						if(recv_bytes == 0 || recv_bytes == -1) {
							continue;
						};
						command[recv_bytes] = '\0';
						if(string(command).compare("quit")==0) {
							cout<<"Bye "<<authUser[i]<<endl;
							close(i);
							FD_CLR(i, &master);
						}
						else if((string(command).substr(0,min(5, (int)strlen(command)))).compare("RETRV")==0) {
							string c  = command;
							int fileNumber = stoi(c.substr(6));
							if(authFileExt[i].find(fileNumber)!=authFileExt[i].end()) {
								// here's where the file sending happens
								string fileToSend = add_file_name(authPath[i], fileNumber, authFileExt[i][fileNumber]);
								string filename = file_name(fileNumber, authFileExt[i][fileNumber]);
								string filesize_str = to_string(authFileSize[i][fileNumber]);
								char* details =(char *) (filename + ":" + filesize_str).c_str();
								send(i, details, strlen(details), 0);
								char rbuffer[PACKET_SIZE];
								cout<<authUser[i]<<": Transferring Message "<<fileNumber<<"\n";
								usleep(10);
								ifstream rFile(fileToSend, ios::in | ios::binary);
								int x = authFileSize[i][fileNumber];
								while(x > PACKET_SIZE) {
									rFile.read(rbuffer, PACKET_SIZE);
									send(i, rbuffer, PACKET_SIZE, 0);
									x -= PACKET_SIZE;
								}
								rFile.read(rbuffer, x);
								send(i, rbuffer, x, 0);
							}
							else {
								cout<<"Message Read Fail\n";
								close(i);
								FD_CLR(i, &master);
							}
						}
						else {
							cout<<"Unknown command\n"<<endl;
							cout<<command<<endl;
							close(i);
							FD_CLR(i, &master);
						}
	            	}
	            }
	        }
	    }
	}
	close(sockfd);
	return 0;
}