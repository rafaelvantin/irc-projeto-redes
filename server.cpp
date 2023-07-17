#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <mutex>

#define MAX_LEN 200
#define NUM_COLORS 6
#define PORT 3637
#define BUFFER_SIZE 4096
#define CHANNEL_SIZE 200

#define QUIT 1
#define PING 2
#define JOIN 3
#define KICK 4
#define MUTE 5
#define UNMUTE 6
#define WHOIS 7

using namespace std;

struct channel{
	string name;
	int admin;
};

struct terminal
{
	int id;
	string name;
	int socketFd;
    struct sockaddr_in address;
	thread th;
    string channel;
	bool isMute;
    bool endFlag;
};


vector<channel> channels;
vector<terminal> clients;
string def_col = "\033[0m";
string colors[] = {"\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m","\033[36m"};
int seed = 0;
mutex cout_mtx, clients_mtx;

string color(int code);


/**
 * @brief Set the name of the client
 * 
 * @param id 
 * @param name 
 */
void set_name(int id, char name[]);


/**
 * @brief Set the channel of the client, if channel does not exist, create a new channel and the client becomes the admin
 * 
 * @param id 
 * @param channel 
 */
void set_channel(int id, char channel[]);

void shared_print(string str, bool endLine);

/**
 * @brief Broadcast message to all clients except the sender int the same channel
 * 
 * @param message 
 * @param sender_id 
 */
void broadcast_message(string message, int sender_id);

/**
 * @brief Broadcast a number to all clients except the sender int the same channel
 * 
 * @param num 
 * @param sender_id  
 */
void broadcast_message(int num, int sender_id);

/**
 * @brief Broadcast a number to all clients except the sender int the same channel
 * 
 * @param num 
 * @param sender_id  
 */
void broadcast_message(string name, string text, int color, int sender_id);

/**
 * @brief Send message to a client
 * 
 * @param id 
 * @param message 
 */
void send_message_as_server(int id, string message);

/**
 * @brief Send message to a client with length information
 * 
 * @param name
 * @param text
 * @param color
 * @param socket 
 */
void send_whole_message(string name, string text, int color, int socket);

/**
 * @brief Receive message from a client checking for length information
 * 
 * @param outBuffer buffer to store the message
 * @param bufferSize size of the buffer
 * @param socket 
 * @return true read successfully
 * @return false read failed
 */
bool recv_whole_message(char *outBuffer, int bufferSize, int socket);

/**
 * @brief End connection with a client
 * 
 * @param id 
 */
void end_connection(int id);


/**
 * @brief Handle client's requests and messages
 * 
 * @param client_socket 
 * @param id 
 */
void handle_client(int client_socket, int id);


/**
 * @brief Get the index of a client in the clients vector
 * 
 * @param id 
 * @return int 
 */
int get_client_index(int id);


/**
 * @brief Check if a name is already taken, while is taken, ask for a new name
 * 
 * @param name 
 * @return int 
 */
void check_name(int client_socket, char name[]);

/**
 * @brief Check if a channel exists
 * 
 * @param channel 
 * @return int 
 */
int exisiting_channel(string channel);

/**
 * @brief Check if a name is already taken
 * 
 * @param name 
 * @return int 
 */
int exisiting_name(string name);



/**
 * @brief Check if a channel name is valid
 * 
 * @param channel_name 
 * @return true 
 * @return false 
 */
bool is_valid_channel_name(string channel_name);
int main()
{
	// Create socket

	int server_socket;

    cout << "Server is running on port " << PORT << endl;
    cout << "Creating socket..." << endl;

	if((server_socket = socket(AF_INET,SOCK_STREAM,0)) == -1)
	{
		perror("socket: ");
		exit(-1);
	}

	// Bind socket fd to socket address and make it listen

	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(PORT);
	server.sin_addr.s_addr = INADDR_ANY;
	bzero(&server.sin_zero, 0);

	if((bind(server_socket, (struct sockaddr *)&server,sizeof(struct sockaddr_in))) == -1)
	{
		perror("bind error: ");
		exit(-1);
	}

	if((listen(server_socket, 8)) == -1)
	{
		perror("listen error: ");
		exit(-1);
	}

	// Setup client connections
	int client_socket;
	unsigned int len = sizeof(sockaddr_in);

	cout << colors[NUM_COLORS-1] << "\n\t  ====== Welcome to the chat-room ======   " << endl << def_col;

	while(1)
    {
        cout << "Waiting for connections..." << endl;

        struct sockaddr_in client;

		if((client_socket = accept(server_socket, (struct sockaddr *)&client, &len)) == -1)
		{
			perror("accept error: ");
			exit(-1);
		}

		seed++;
		thread t(handle_client, client_socket, seed);
		lock_guard<mutex> guard(clients_mtx);

		clients.push_back({
            seed, 
            string("Anonymous"),
            client_socket,
            client,
            (move(t)),
            "",
            false,
            false
        });
	}

	for(int i = 0; i < clients.size(); i++)
	{
		if(clients[i].th.joinable()) {
			clients[i].th.join();
            shared_print("Thread " + to_string(i) + " joined", true);
        }
	}

	close(server_socket);
	return 0;
}

string color(int code)
{
	return colors[code % NUM_COLORS];
}

// Set name of client
void set_name(int id, char name[])
{
	int client_index = get_client_index(id);
	clients[client_index].name = string(name);
}

int exisiting_channel(string channel)
{
	for(int i=0; i<channels.size(); i++)
	{
		if(channels[i].name == channel)
			return i;
	}

	return -1;
}

void set_channel(int id, char channel[])
{

	// If channel does not exist, create a new channel
	if(exisiting_channel(string(channel)) == -1)
	{
		struct channel new_channel;
		new_channel.name = string(channel);
		new_channel.admin = id;
		channels.push_back(new_channel);
	}

	int client_index = get_client_index(id);
	
	if(client_index == -1)
		cout << "Error in setting channel, no client found of id " << id << endl;
	else
	{
		cout << "Setting channel to " << channel << endl;
		clients[client_index].channel = string(channel);
	}
}

// For synchronisation of cout statements
void shared_print(string str, bool endLine = true)
{	
	lock_guard<mutex> guard(cout_mtx);
	cout << str;

	if(endLine)
		cout << endl;
}

int get_client_index(int id)
{
    for(int i = 0; i < clients.size(); i++)
    {
        if(clients[i].id == id)
            return i;
    }
	return -1;
}

int get_client_by_name(string name)
{
    for(int i = 0; i < clients.size(); i++)
    {
        if(clients[i].name == name)
            return clients[i].id;
    }
	return -1;

}

int get_channel_index(string name)
{
    for(int i = 0; i < channels.size(); i++)
    {
        if(channels[i].name == name)
            return i;
    }
	return -1;

}

// Broadcast message to all clients except the sender
void broadcast_message(string message, int sender_id)
{
	// Prepara msg
	char temp[BUFFER_SIZE];
	strcpy(temp, message.c_str());
    
	// Get client channel
	int index = get_client_index(sender_id);
    string channel = clients[index].channel;
	
	// Send msg to clients from same channel
	for(int i = 0; i < clients.size(); i++)
	{
		if(clients[i].id != sender_id && clients[i].channel == channel)
		{
			send(clients[i].socketFd, temp, sizeof(temp), 0);
		}
	}		
}

void broadcast_message(string name, string text, int color, int sender_id)
{
    // Get client channel
    int index = get_client_index(sender_id);
    string channel = clients[index].channel;
    
    // Send msg to clients from same channel
    for(int i = 0; i < clients.size(); i++)
    {
        if(clients[i].id != sender_id && clients[i].channel == channel)
        {
            send_whole_message(name, text, color, clients[i].socketFd);
        }
    }		
}

// Broadcast a number to all clients except the sender
void broadcast_message(int num, int sender_id)
{
	// Get client channel
    int index = get_client_index(sender_id);
    string channel = clients[index].channel;

	// Send msg to clients from same channel
	for(int i = 0; i < clients.size(); i++)
	{
		if(clients[i].id != sender_id && clients[i].channel == channel)
		{
			send(clients[i].socketFd, &num, sizeof(num), 0);
		}
	}		
}

bool authAdmin(int id)
{
	int client_index = get_client_index(id);
	int channel_index = get_channel_index(clients[client_index].channel);

	if(channels[channel_index].admin != id)
	{
		send_message_as_server(id, "You are not the admin of this channel.");
		return false;
	}

	return true;
}

void mute(int callerId, string targetName)
{
	int targetId = get_client_by_name(targetName);
    if(targetId == -1)
    {
        send_message_as_server(callerId, "No user found with that name.");
        return;
    }

    int targetIndex = get_client_index(targetId);
    int callerIndex = get_client_index(callerId);

	if(clients[callerIndex].channel != clients[targetIndex].channel)
	{
		send_message_as_server(callerId, "No user found with that name.");
	}
	else
	{
		if(clients[targetIndex].isMute)
		{
			send_message_as_server(callerId, "User is already muted.");
		}
		else
		{
			clients[targetIndex].isMute = true;
			send_message_as_server(callerId, "User muted.");
		}
	}
}

void unmute(int callerId, string targetName)
{
	int targetId = get_client_by_name(targetName);
    if(targetId == -1)
    {
        send_message_as_server(callerId, "No user found with that name.");
        return;
    }

    int targetIndex = get_client_index(targetId);
    int callerIndex = get_client_index(callerId);

	if(clients[callerIndex].channel != clients[targetIndex].channel)
	{
		send_message_as_server(callerId, "No user found with that name.");
	}
	else
	{
		if(!clients[targetIndex].isMute)
		{
			send_message_as_server(callerId, "User is already unmuted.");
		}
		else
		{
			clients[targetIndex].isMute = false;
			send_message_as_server(callerId, "User unmuted.");
		}
	}
}

void whois(int callerId, string targetName)
{
    if(!authAdmin(callerId))
        return;

    int targetId = get_client_by_name(targetName);
    if(targetId == -1)
    {
        send_message_as_server(callerId, "No user found with that name.");
        return;
    }

    int targetIndex = get_client_index(targetId);
    int callerIndex = get_client_index(callerId);

    // Só mostra informações de usuários do canal do admin
    if(clients[targetIndex].channel != clients[callerIndex].channel)
    {
        send_message_as_server(callerId, "No user found with that name.");
        return;
    }

    string message = "\nName: " + clients[targetIndex].name + "\n";
    message += "Id: " + to_string(clients[targetIndex].id) + "\n";
    message += "Muted: ";
    message += clients[targetIndex].isMute ? "Yes\n" : "No\n";
    message += "IP: ";
    message += inet_ntoa(clients[targetIndex].address.sin_addr);
    message += "\nPort: ";
    message += to_string(ntohs(clients[targetIndex].address.sin_port));

    send_message_as_server(callerId, message);
}

void kick(int callerId, string targetName)
{
    if(!authAdmin(callerId))
        return;

    int targetId = get_client_by_name(targetName);
    if(targetId == -1)
    {
        send_message_as_server(callerId, "No user found with that name.");
        return;
    }

    int targetIndex = get_client_index(targetId);
    int callerIndex = get_client_index(callerId);

    // Só pode expulsar de usuários do canal do admin
    if(clients[targetIndex].channel != clients[callerIndex].channel)
    {
        send_message_as_server(callerId, "No user found with that name.");
        return;
    }

    send_message_as_server(targetId, "You were kicked from the channel.");
    end_connection(targetId);

    string message = "User " + clients[targetIndex].name + " was kicked from the channel.";
    /*broadcast_message("Server", callerId);
    broadcast_message(0, callerId);
    broadcast_message(message, callerId);*/
    broadcast_message("Server", message, 0, callerId);
    send_message_as_server(callerId, message);
}

void end_connection(int id)
{
	for(int i = 0; i < clients.size(); i++)
	{
		if(clients[i].id == id)	
		{
			lock_guard<mutex> guard(clients_mtx);

            clients[i].endFlag = true;

            shutdown(clients[i].socketFd, SHUT_RDWR);
            close(clients[i].socketFd);
			
            break;
		}
	}				
}

void handle_client(int client_socket, int id)
{
	char name[MAX_LEN], str[BUFFER_SIZE], channel[MAX_LEN];

    recv_whole_message(name, sizeof(name), client_socket);
    check_name(id, name);

	/*
    recv(client_socket, channel, sizeof(channel), 0);
    set_channel(id, channel);
*/

	// Display welcome message
	string welcome_message = string(name) + string(" has joined");
	/*broadcast_message("#NULL", id);	
	broadcast_message(id, id);								
	broadcast_message(welcome_message, id);	*/
    broadcast_message("#NULL", welcome_message, id, id);
	shared_print(color(id) + welcome_message + def_col);

	int client_index = get_client_index(id);
	
	while(!clients[client_index].endFlag)
	{
		bool got_message = recv_whole_message(str, sizeof(str), client_socket);
		
		if(!got_message) {
            string message = string(name) + string(" has left");		
            /*broadcast_message("#NULL", id);			
            broadcast_message(id, id);						
            broadcast_message(message, id);*/
            broadcast_message("#NULL", message, id, id);
            shared_print(color(id) + message + def_col);

            end_connection(id);

			continue;
        }
        
		if(str[0] == '/')
		{

			if(strcmp(str, "/quit") == 0)
			{
				// Display leaving message
				string message = string(name) + string(" has left");		
				/*broadcast_message("#NULL", id);			
				broadcast_message(id, id);						
				broadcast_message(message, id);*/
                broadcast_message("#NULL", message, id, id);
				shared_print(color(id) + message + def_col);
				end_connection(id);							
				continue;
			}

			if(strcmp(str, "/ping") == 0)
			{
				send_message_as_server(id, "pong");
				continue;
			}

			if(string(str).substr(0,5) == "/mute")
			{
				if(!authAdmin(id))
					continue;

				mute(id, string(str).substr(6));
				continue;
			}

			if(string(str).substr(0,7) == "/unmute")
			{
				if(!authAdmin(id))
					continue;

				unmute(id, string(str).substr(8));
				continue;
			}

			if(string(str).substr(0,5) == "/join")
			{
				string new_channel = string(str).substr(6);
				if(!is_valid_channel_name(new_channel))
				{
					send_message_as_server(id, "Invalid channel name");
					continue;
				}
				set_channel(id, (char *)new_channel.c_str());
				string welcome_message = string(name) + string(" has joined");
				/*broadcast_message("#NULL", id);	
				broadcast_message(id, id);								
				broadcast_message(welcome_message, id);	*/
                broadcast_message("#NULL", welcome_message, id, id);
				shared_print(color(id) + welcome_message + def_col);
				continue;
			}

			if(string(str).substr(0,9) == "/nickname")
			{
				if(string(str).length() < 10)
				{
					send_message_as_server(id, "Invalid command");
					continue;
				}

				string new_name = string(str).substr(10);
				if(new_name.length() > 50)
				{
					send_message_as_server(id, "Name too long");
					continue;
				}
				if(exisiting_name(new_name) != -1)
				{
					send_message_as_server(id, "Name already taken");
					continue;
				}
				set_name(id, (char *)new_name.c_str());
				
				strcpy(name, new_name.c_str());
				
				continue;
			}

            if(string(str).substr(0,6) == "/whois")
            {
                string whois_name = string(str).substr(7);
                
                whois(id, whois_name);
                
                continue;
            }

            if(string(str).substr(0,5) == "/kick")
            {
                string kick_name = string(str).substr(6);
                
                kick(id, kick_name);
                
                continue;
            }


			send_message_as_server(client_socket, "Invalid command");

		}

		if(clients[client_index].channel == "")
		{
			send_message_as_server(id, "You are not in a channel.");
			continue;
		}

		if(clients[client_index].isMute)
		{
			send_message_as_server(id, "You are muted.");
			continue;
		}

		/*broadcast_message(string(name), id);					
		broadcast_message(id, id);		
		broadcast_message(string(str), id);*/
        broadcast_message(string(name), string(str), id, id);
		shared_print(color(id) + name + " : " + def_col + str);		
	}	

    // Clean up

    lock_guard<mutex> guard(clients_mtx);

    // Detach thread, remove client from vector and kill thread

    clients[client_index].th.detach();

    clients.erase(clients.begin() + client_index);

    return;
}

void check_name(int id, char name[])
{
	int client_index = get_client_index(id);
	int client_socket = clients[client_index].socketFd; 

    do{
        if(exisiting_name(string(name)) == -1)
		{
			break;
		}
        else
        {
            string message = "#NAME_TAKEN";
            send(client_socket,message.c_str(), sizeof(message), 0);
            
            char new_name[MAX_LEN];
            recv_whole_message(new_name, sizeof(new_name), client_socket);
            strcpy(name, new_name);
        }

    }while(1);

    send(client_socket, "#NAME_OK", sizeof("#NAME_OK"), 0);
	set_name(id, name);
}

void send_message_as_server(int id, string message)
{
	int index = get_client_index(id);
	int client_socket = clients[index].socketFd;

	/*char temp[BUFFER_SIZE];
    memset(temp, 0, sizeof(temp));
	strcpy(temp, message.c_str());

    int len = strlen(temp);

	// Send origin 
	string server = "Server";
    char server_name[BUFFER_SIZE];
    memset(server_name, 0, sizeof(server_name));
    strcpy(server_name, server.c_str());
    int server_len = strlen(server_name);
	send(client_socket, server_name, server_len, 0);
	
	// Send msg color
	int num = 0;
	send(client_socket, &num, sizeof(num), 0);
	
	// Send msg
	send(client_socket, temp, len, 0);*/

    send_whole_message("Server", message, 0, client_socket);
}


int exisiting_name(string name)
{
	for(int i = 0; i < clients.size(); i++)
	{
		if(clients[i].name == name)
			return i;
	}

	return -1;
}


bool is_valid_channel_name(string channel_name) {

    // Checks if the channel name has an invalid size.
    if(channel_name.length() > CHANNEL_SIZE) // Checks for valid size.
        return false;

    // Checks for invalid stater characters.
    if(channel_name[0] != '&' && channel_name[0] != '#')
        return false;

    // Checks for invalid characters on the whole channel name.
    for(auto iter = channel_name.begin(); iter != channel_name.end(); iter++) { 
        if(*iter == ' ' || *iter == 7 || *iter == ',')
            return false;
    }

    // If nothing invalid was found the channel name is valid.
    return true;

}

void send_whole_message(string name, string text, int color, int socket) {
    // Setup buffers
    char name_buffer[BUFFER_SIZE];
    char text_buffer[BUFFER_SIZE];
    memset(name_buffer, 0, sizeof(name_buffer));
    memset(text_buffer, 0, sizeof(text_buffer));

    // Get name and text sizes
    int name_size = name.length();
    int text_size = text.length();

    // Copy name and text to buffers
    strncpy(name_buffer, name.c_str(), BUFFER_SIZE);
    strncpy(text_buffer, text.c_str(), BUFFER_SIZE);

    // Build header with name size, text size and color
    int header[3] = {name_size, text_size, color};

    // Send header
    send(socket, header, sizeof(header), 0);

    // Send name and text
    send(socket, name_buffer, name_size, 0);
    send(socket, text_buffer, text_size, 0);

    return;
}

bool recv_whole_message(char *outBuffer, int bufferSize, int socket) {
    // Setup buffer
    char textBuffer[BUFFER_SIZE];
    memset(textBuffer, 0, sizeof(textBuffer));

    int bytesReceived = 0;

    // Receive msg size
    int size;
    bytesReceived = recv(socket, &size, sizeof(size), MSG_WAITALL);
    if(bytesReceived <= 0)
        return false;

    // Receive msg
    bytesReceived = recv(socket, textBuffer, size, MSG_WAITALL);
    if(bytesReceived <= 0)
        return false;

    // Copy msg to outBuffer
    memset(outBuffer, 0, bufferSize);
    strncpy(outBuffer, textBuffer, bufferSize);

    return true;
}