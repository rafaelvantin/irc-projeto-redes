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
#define PORT 3637
#define BUFFER_SIZE 4096

using namespace std;

bool exit_flag = false;
thread t_send, t_recv;
int client_socket;
int client_id;
string def_col = "\033[0m";
string colors[] = {"\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m", "\033[36m"};

// Message structure
// <name_len> <text_len> <color_code> <name> <text>
typedef struct message
{
    bool is_null;
    int name_len;
    int text_len;
    int color_code;
    char name[BUFFER_SIZE];
    char text[BUFFER_SIZE];
} message_t;

void quit_connection(bool forceQuitApp);
void catch_ctrl_c(int signal);
string color(int code);
void eraseText(int cnt);
void send_message_worker(int client_socket);
void recv_message_worker(int client_socket);
void send_message(string text, int client_socket);
message_t read_message(int client_socket);
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
	    send_message(name, client_socket);

        //Check if name is already taken
        char response[BUFFER_SIZE];
        memset(response, 0, sizeof(response));
        int length;
        recv(client_socket, &length, sizeof(length), MSG_WAITALL);
        recv(client_socket, response, length, MSG_WAITALL);
    	
        fflush(stdout);

        if(strcmp(response, "#NAME_TAKEN") == 0) {
            cout << "Name already taken. Please choose another name." << endl;
            continue;
        }
        else if(strcmp(response, "#NAME_OK") == 0) {
            // Receive client id
            recv(client_socket, &client_id, sizeof(client_id), MSG_WAITALL);

            cout << "Name accepted." << endl;

            break;
        }
        else {
            cout << "Error: " << response << endl;
            exit(-1);
        }
    }

	// Start channel logic

	cout << colors[NUM_COLORS-1] << "\n\t  ====== Welcome to the chat-room ======   " << endl << def_col;

	thread t1(send_message_worker, client_socket);
	thread t2(recv_message_worker, client_socket);

	t_send = move(t1);
	t_recv = move(t2);

	if(t_send.joinable())
		t_send.join();

	if(t_recv.joinable())
		t_recv.join();

    close(client_socket);

    cout << "\nQuitting..." << endl;
			
	return 0;
}

void quit_connection(bool forceQuitApp = false)
{
    // Inform server that client is quitting
    send_message("/quit", client_socket);

    exit_flag = true;

    // Shutdown socket so that recv() in recv_message_worker() returns 0
    // making it able to quit
    shutdown(client_socket, SHUT_RD);

    if(forceQuitApp) {
        pthread_cancel(t_send.native_handle());
    }
}

// Handler for "Ctrl + C"
void catch_ctrl_c(int signal) 
{
    quit_connection(true);
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

// Send message to server
void send_message(string text, int client_socket)
{
    char text_buffer[BUFFER_SIZE];
    memset(text_buffer, 0, sizeof(text_buffer));

    int text_len = text.length();

    strncpy(text_buffer, text.c_str(), BUFFER_SIZE);

    send(client_socket, &text_len, sizeof(text_len), 0); 
    send(client_socket, text.c_str(), text_len, 0);
}

// Main send thread
void send_message_worker(int client_socket)
{
	while(!exit_flag)
	{
		cout << color(client_id) << "You : " << def_col;
		
		char str[BUFFER_SIZE];
		cin.getline(str, BUFFER_SIZE);

        if(cin.eof()) {
            quit_connection();
            continue;
        }
        
		if(strlen(str) == 0)
            continue;

        if(strcmp(str, "/quit") == 0)
        {
            quit_connection();
            continue;
        }
        if (strcmp(str, "/help") == 0){
            print_help();
            continue;

        }
        send_message(string(str), client_socket);
	}	

    return;
}

// Main recv thread
void recv_message_worker(int client_socket)
{
	while(!exit_flag)
	{
        message_t msg = read_message(client_socket);
        if(msg.is_null) {
            // Error or server disconnected
            exit_flag = true;
            pthread_cancel(t_send.native_handle());
            continue;
        }
		eraseText(6);
		
		if(strcmp(msg.name, "#NULL") != 0){
			cout << color(msg.color_code) << msg.name << " : " << def_col << msg.text << endl;
        }
		else
			cout << color(msg.color_code) << msg.text << endl;
		
		cout << color(client_id) << "You : " << def_col;

		fflush(stdout);
	}	

    return;
}

message_t read_message(int client_socket)
{
    message_t msg;
    memset(&msg, 0, sizeof(msg));

    int bytes_read;

    // Read message header
    bytes_read = recv(client_socket, &msg.name_len, sizeof(msg.name_len), MSG_WAITALL);
    if(bytes_read <= 0) {
        msg.is_null = true;
        return msg;
    }

    bytes_read = recv(client_socket, &msg.text_len, sizeof(msg.text_len), MSG_WAITALL);
    if(bytes_read = 0) {
        msg.is_null <= true;
        return msg;
    }

    bytes_read = recv(client_socket, &msg.color_code, sizeof(msg.color_code), MSG_WAITALL);
    if(bytes_read <= 0) {
        msg.is_null = true;
        return msg;
    }

    // Read message body
    bytes_read = recv(client_socket, msg.name, msg.name_len, MSG_WAITALL);
    if(bytes_read <= 0) {
        msg.is_null = true;
        return msg;
    }

    bytes_read = recv(client_socket, msg.text, msg.text_len, MSG_WAITALL);
    if(bytes_read <= 0) {
        msg.is_null = true;
        return msg;
    }

    msg.is_null = false;
    return msg;
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