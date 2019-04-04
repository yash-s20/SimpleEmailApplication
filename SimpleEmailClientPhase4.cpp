#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <bits/stdc++.h>
#include <arpa/inet.h>
#include <fts.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#define MAX_LENGTH 65535
#define PACKET_SIZE 32767
using namespace std;

//removing directory - https://stackoverflow.com/questions/2256945/removing-a-non-empty-directory-programmatically-in-c-or-c/42978529
int recursive_delete(const char *dir) {
    int ret = 0;
    FTS *ftsp = NULL;
    FTSENT *curr;

    // Cast needed (in C) because fts_open() takes a "char * const *", instead
    // of a "const char * const *", which is only allowed in C++. fts_open()
    // does not modify the argument.
    char *files[] = { (char *) dir, NULL };

    try {
    	ftsp = fts_open(files, FTS_NOCHDIR | FTS_PHYSICAL | FTS_XDEV, NULL);
    }
    catch(...) {
    	return 0;
    }
    if (!ftsp) {
        goto finish;
    }

    while ((curr = fts_read(ftsp))) {
        switch (curr->fts_info) {
        case FTS_NS:
        case FTS_DNR:
        case FTS_ERR:
            fprintf(stderr, "%s: fts_read error: %s\n",
                    curr->fts_accpath, strerror(curr->fts_errno));
            break;

        case FTS_DC:
        case FTS_DOT:
        case FTS_NSOK:
            // Not reached unless FTS_LOGICAL, FTS_SEEDOT, or FTS_NOSTAT were
            // passed to fts_open()
            break;

        case FTS_D:
            // Do nothing. Need depth-first search, so directories are deleted
            // in FTS_DP
            break;

        case FTS_DP:
        case FTS_F:
        case FTS_SL:
        case FTS_SLNONE:
        case FTS_DEFAULT:
            if (remove(curr->fts_accpath) < 0) {
                fprintf(stderr, "%s: Failed to remove: %s\n",
                        curr->fts_path, strerror(errno));
                ret = -1;
            }
            break;
        }
    }

	finish:
    if (ftsp) {
        fts_close(ftsp);
    }

    return ret;
}


// main
int main(int argc, char const *argv[])
{
	if(argc != 7) {
		cerr<<"Usage: "<<argv[0]<<" <serverIPAddr>:<port> <user-name> <passwd> <file1,file2...> <local folder> <interval>\n";
		exit(1);

	}
	char* ip_port = (char*) argv[1];
	const char* serverIPAddr = strtok(ip_port, ":");
	const char* portNum = strtok(NULL, ":");
	const string userName = argv[2];
	const string passwd = argv[3];
	const string localFolder = argv[5];

	int noOfFiles = 0;
	char* token = strtok((char*)argv[4], ",");
	vector<int> files;
	while(token!=NULL) {
		noOfFiles++;
		try {
			files.push_back(stoi(token));
		}
		catch(...) {
			cout<<token<<endl;
			cerr<<"Usage: <executable> <serverIPAddr>:<port> <user-name> <passwd> <file1, file2...> <local folder>\n";
			exit(3);
		}
		token = strtok(NULL, ",");
	}
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIR* dir = opendir(localFolder.c_str());
	if (dir)
	{
	    closedir(dir);
	    recursive_delete(localFolder.c_str());
	}
	else if(ENOENT != errno) {
		cerr<<"Error in accessing folder\n";
		exit(4);
	}
	mkdir(localFolder.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);



	struct addrinfo hints;
	struct addrinfo* server_info;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	int server_status = getaddrinfo(serverIPAddr, portNum, &hints, &server_info);
	char s[INET6_ADDRSTRLEN];
	inet_ntop(server_info->ai_family, &((struct sockaddr_in *)server_info->ai_addr)->sin_addr, s, sizeof s);

	int connect_status = connect(sockfd, server_info->ai_addr, server_info->ai_addrlen);
	if(connect_status == -1) {
		cerr<<"Connection to server "<<serverIPAddr<<" on port "<<portNum<<" failed\n";
		exit(2);
	}
	cout<<"ConnectDone: "<<serverIPAddr<<":"<<portNum<<"\n";
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
	usleep(stoi(argv[6])*1000000);
	//send LIST
	const char* command ;
	// send(sockfd, command, strlen(command), 0);
	// char message[MAX_LENGTH];
	// recv(sockfd, message, MAX_LENGTH, 0);
	// cout<<message;

	for (int i = 0; i < noOfFiles; ++i)
	{
		string retrieve = "RETRV ";
		retrieve.append(to_string(files[i]));
		int status = send(sockfd, retrieve.c_str(), strlen(retrieve.c_str()), 0);
		
		char details[MAX_LENGTH];
		int bytes_received = recv(sockfd, details, MAX_LENGTH, 0);
		if (bytes_received==0 or bytes_received==-1) {
			close(sockfd);
			exit(0);
		}
		details[bytes_received] = '\0';
		string filename = strtok(details, ":");
		int filesize = stoi(strtok(NULL, ":"));

		// cout<<"Receiving "<<filename<<" of size "<<filesize<<endl;
		int received = 0;
		string filepath = localFolder + "/" + filename;
		ofstream of(filepath, ios::out | ios::binary);
		char buffer[PACKET_SIZE];
		while(received < filesize) {
			int r = recv(sockfd, buffer, PACKET_SIZE, 0);
			of.write(buffer, r);
			received += r;
		}
		assert(received == filesize);
		cout<<"Downloaded Message "<<files[i]<<endl;
		of.close();
	}

	command = "quit";
	send(sockfd, command, strlen(command), 0);
	close(sockfd);
	return 0;
}