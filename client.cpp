#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <signal.h>
#include <mutex>

#define MAX_LEN 200
#define NUM_COLORS 6
#define PORT 8080
#define BUFFER_SIZE 4096

using namespace std;

bool exit_flag = false;
thread t_send, t_recv;
int client_socket;
string def_col = "\033[0m";
string colors[] = {"\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m", "\033[36m"};

void quit_connection();
void catch_ctrl_c(int signal);
string color(int code);
void eraseText(int cnt);
void send_message(int client_socket);
void recv_message(int client_socket);
void print_help();

int main()
{
	// Create connection

    //cout << "Client is running on port " << PORT << endl;
    cout << "Creating socket..." << endl;
	
	if((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket: ");
		exit(-1);
	}
    
	string ip;
    string command;

    cout << "Socket created successfully" << endl;

    while(1){
        cout << "Type /connect <ip> to connect to a server or /quit to finish the connection or /help to see the commands" << endl;
        getline(cin,command);
        
        if(command.substr(0,8) == "/connect")
        {
            ip = command.substr(9);
            break;
        }
        if(command == "/quit")
        {
            cout << "Quitting..." << endl;
            return 0;
        }
		if(command == "/help")
		{
			print_help();
			continue;
		}
    }

	// Bind socket fd to socket address

	struct sockaddr_in client;
	client.sin_family = AF_INET;
	client.sin_port = htons(PORT); // Port no. of server
	client.sin_addr.s_addr = INADDR_ANY;
	client.sin_addr.s_addr = inet_addr(ip.c_str()); // Provide IP address of server
	bzero(&client.sin_zero, 0);

	if((connect(client_socket, (struct sockaddr *)&client, sizeof(struct sockaddr_in))) == -1)
	{
		perror("connect: ");
		exit(-1);
	}
	signal(SIGINT, catch_ctrl_c);

    cout << "Connected to server" << endl;
    
	// Name selection

	while(1){
	    char name[MAX_LEN];
	    cout << "Enter your name : ";
	    cin.getline(name, MAX_LEN);
		if(strlen(name) == 0 || strlen(name) > 50){
			cout << "Name must be between 1 and 50 characters." << endl;
			continue;
		}
	    send(client_socket, name, sizeof(name), 0);

        //Check if name is already taken
        char response[BUFFER_SIZE];
        recv(client_socket, response, sizeof(response), 0);
    	fflush(stdout);

        if(strcmp(response, "#NAME_TAKEN") == 0){
            cout << "Name already taken. Please choose another name." << endl;
            continue;
        }
        else{
            cout << "Name accepted." << endl;
            break;
        }
    }

	// Channel selection 

  /*  cout << "Enter the channel name: ";
    char channel[MAX_LEN];
    cin.getline(channel, MAX_LEN);
    send(client_socket, channel, sizeof(channel), 0);
*/
	// Start channel logic

	cout << colors[NUM_COLORS-1] << "\n\t  ====== Welcome to the chat-room ======   " << endl << def_col;

	thread t1(send_message, client_socket);
	thread t2(recv_message, client_socket);

	t_send = move(t1);
	t_recv = move(t2);

	if(t_send.joinable())
		t_send.join();

	if(t_recv.joinable())
		t_recv.join();

    close(client_socket);

    cout << "Quitting..." << endl;
			
	return 0;
}

void quit_connection()
{
    char str[BUFFER_SIZE] = "/quit";
    send(client_socket, str, sizeof(str), 0);
    exit_flag = true;

    // Shutdown socket so that recv() in recv_message() returns 0
    // making it able to quit
    shutdown(client_socket, SHUT_RD);
}

// Handler for "Ctrl + C"
void catch_ctrl_c(int signal) 
{
    string str = "/quit";
    send(client_socket, str.c_str(), sizeof(str), 0);
    quit_connection();
}

string color(int code)
{
	return colors[code%NUM_COLORS];
}

// Erase text from terminal
void eraseText(int cnt)
{
	char back_space=8;
	for(int i = 0; i < cnt; i++)
	{
		cout << back_space;
	}	
}

// Send message to everyone
void send_message(int client_socket)
{
	while(!exit_flag)
	{
		cout << colors[1] << "You : " << def_col;
		
		char str[BUFFER_SIZE];
		cin.getline(str, BUFFER_SIZE);

        if(cin.eof()) {
            quit_connection();
        }
        
		if(strlen(str) == 0)
            continue;

        if(strcmp(str, "/quit") == 0)
        {
            quit_connection();
        }
        if (strcmp(str, "/help") == 0){
            print_help();
            continue;

        }

		send(client_socket, str, sizeof(str), 0);
	}	
}

// Receive message
void recv_message(int client_socket)
{
	while(!exit_flag)
	{
		char name[BUFFER_SIZE], str[BUFFER_SIZE];
		int color_code;
		int bytes_received = recv(client_socket, name, sizeof(name), 0);

		if(bytes_received<=0) {
            if(bytes_received == 0) {
                cout << "Server disconnected." << endl;
                exit_flag = true;
                close(client_socket);
                exit(0);
            }

			continue;
        }
		
		recv(client_socket, &color_code, sizeof(color_code), 0);
		recv(client_socket, str, sizeof(str), 0);
		eraseText(6);
		
		if(strcmp(name, "#NULL") != 0){
			cout << color(color_code) << name << " : " << def_col << str << endl;
        }
		else
			cout << color(color_code) << str << endl;
		
		cout << colors[1] << "You : " << def_col;

		fflush(stdout);
	}	
}

void print_help()
{
	ifstream helpFile;
	helpFile.open("help.txt");
	
	string line;
	while(getline(helpFile,line)){
		cout << line << endl;
	}
	helpFile.close();
}