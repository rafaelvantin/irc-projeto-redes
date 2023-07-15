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
#define PORT 8080
#define BUFFER_SIZE 4096

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
	int socket;
	thread th;
    string channel;
};


vector<channel> channels;
vector<terminal> clients;
string def_col="\033[0m";
string colors[]={"\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m","\033[36m"};
int seed=0;
mutex cout_mtx,clients_mtx;

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
 * @brief Send message to a client
 * 
 * @param id 
 * @param message 
 */
void send_message(int id, string message);


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


int main()
{
	int server_socket;

    cout << "Server is running on port " << PORT << endl;
    cout<< "Creating socket..." << endl;

	if((server_socket=socket(AF_INET,SOCK_STREAM,0))==-1)
	{
		perror("socket: ");
		exit(-1);
	}

	struct sockaddr_in server;
	server.sin_family=AF_INET;
	server.sin_port=htons(PORT);
	server.sin_addr.s_addr=INADDR_ANY;
	bzero(&server.sin_zero,0);

	if((bind(server_socket,(struct sockaddr *)&server,sizeof(struct sockaddr_in)))==-1)
	{
		perror("bind error: ");
		exit(-1);
	}

	if((listen(server_socket,8))==-1)
	{
		perror("listen error: ");
		exit(-1);
	}

	struct sockaddr_in client;
	int client_socket;
	unsigned int len=sizeof(sockaddr_in);

	cout<<colors[NUM_COLORS-1]<<"\n\t  ====== Welcome to the chat-room ======   "<<endl<<def_col;

	while(1)
    {
        cout << "Waiting for connections..." << endl;
		if((client_socket=accept(server_socket,(struct sockaddr *)&client,&len))==-1)
		{
			perror("accept error: ");
			exit(-1);
		}
		seed++;
		thread t(handle_client,client_socket,seed);
		lock_guard<mutex> guard(clients_mtx);
		clients.push_back({seed, string("Anonymous"),client_socket,(move(t))});
	}

	for(int i=0; i<clients.size(); i++)
	{
		if(clients[i].th.joinable())
			clients[i].th.join();
	}

	close(server_socket);
	return 0;
}

string color(int code)
{
	return colors[code%NUM_COLORS];
}

// Set name of client
void set_name(int id, char name[])
{
	for(int i=0; i<clients.size(); i++)
	{
			if(clients[i].id==id)	
			{
				clients[i].name=string(name);
			}
	}	
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

    for(int i=0; i<clients.size(); i++)
    {
            if(clients[i].id==id)	
            {
                cout << "Setting channel to " << channel << endl;
                clients[i].channel=string(channel);
            }
    }	
}

// For synchronisation of cout statements
void shared_print(string str, bool endLine=true)
{	
	lock_guard<mutex> guard(cout_mtx);
	cout<<str;
	if(endLine)
			cout<<endl;
}

int get_client_index(int id)
{
    for(int i=0; i<clients.size(); i++)
    {
        if(clients[i].id==id)
            return i;
    }
	return -1;
}

// Broadcast message to all clients except the sender
void broadcast_message(string message, int sender_id)
{
	char temp[MAX_LEN];
	strcpy(temp,message.c_str());
    int index = get_client_index(sender_id);
    string channel = clients[index].channel;
	for(int i=0; i<clients.size(); i++)
	{
		if(clients[i].id!=sender_id && clients[i].channel == channel)
		{
			send(clients[i].socket,temp,sizeof(temp),0);
		}
	}		
}

// Broadcast a number to all clients except the sender
void broadcast_message(int num, int sender_id)
{
    int index = get_client_index(sender_id);
    string channel = clients[index].channel;
	for(int i=0; i<clients.size(); i++)
	{
		if(clients[i].id!=sender_id && clients[i].channel == channel)
		{
			send(clients[i].socket,&num,sizeof(num),0);
		}
	}		
}

void end_connection(int id)
{
	for(int i=0; i<clients.size(); i++)
	{
		if(clients[i].id==id)	
		{
			lock_guard<mutex> guard(clients_mtx);
			clients[i].th.detach();
			clients.erase(clients.begin()+i);
			close(clients[i].socket);
			break;
		}
	}				
}

void handle_client(int client_socket, int id)
{
	char name[MAX_LEN],str[MAX_LEN], channel[MAX_LEN];

    recv(client_socket,name,sizeof(name),0);
    check_name(id,name);

    recv(client_socket,channel,sizeof(channel),0);
    set_channel(id,channel);


	// Display welcome message
	string welcome_message=string(name)+string(" has joined");
	broadcast_message("#NULL", id);	
	broadcast_message(id,id);								
	broadcast_message(welcome_message,id);	
	shared_print(color(id)+welcome_message+def_col);
	
	while(1)
	{
		int bytes_received=recv(client_socket,str,sizeof(str),0);
		if(bytes_received<=0)
			return;
        if(str[0] == '/'){
			if(strcmp(str,"/quit")==0)
			{
				// Display leaving message
				string message=string(name)+string(" has left");		
				broadcast_message("#NULL",id);			
				broadcast_message(id,id);						
				broadcast_message(message,id);
				shared_print(color(id)+message+def_col);
				end_connection(client_socket);							
				return;
			}
			if(strcmp(str,"/ping")==0)
			{
				send_message(id,"pong");
				continue;
			}
			if(string(str).substr(0,5)=="/join")
			{
				string new_channel = string(str).substr(6);
				set_channel(id, (char *)new_channel.c_str());
				string welcome_message=string(name)+string(" has joined");
				broadcast_message("#NULL",id);	
				broadcast_message(id,id);								
				broadcast_message(welcome_message,id);	
				shared_print(color(id)+welcome_message+def_col);
				continue;
			}
			if(string(str).substr(0,9)== "/nickname")
			{
				if(string(str).length() < 10)
				{
					send_message(id,"Invalid command");
					continue;
				}
				string new_name = string(str).substr(10);
				if(exisiting_name(new_name) != -1){
					send_message(id,"Name already taken");
					continue;
				}
				set_name(id, (char *)new_name.c_str());
				
				strcpy(name,new_name.c_str());
				
				continue;
			}


			send_message(client_socket,"Invalid command");

		}
		broadcast_message(string(name),id);					
		broadcast_message(id,id);		
		broadcast_message(string(str),id);
		shared_print(color(id)+name+" : "+def_col+str);		
	}	
}

void check_name(int id, char name[]){

	int index = get_client_index(id);
	int client_socket = clients[index].socket; 

    do{
        if(exisiting_name(string(name)) == -1)
		{
			break;
		}
        else
        {
            string message = "#NAME_TAKEN";
            send(client_socket,message.c_str(),sizeof(message),0);
            char newName[MAX_LEN];

            recv(client_socket,newName,sizeof(newName),0);
            strcpy(name,newName);
            
        }

    }while(1);
    send(client_socket,"#NAME_OK",sizeof("#NAME_OK"),0);
	set_name(id,name);
}

void send_message(int id, string message){
	int index = get_client_index(id);
	int client_socket = clients[index].socket;

	char temp[MAX_LEN];
	strcpy(temp,message.c_str());
	string server = "Server";
	send(client_socket,server.c_str(),sizeof(server),0);
	int num = 0;
	send(client_socket,&num,sizeof(num),0);
	send(client_socket,temp,sizeof(temp),0);
}


int exisiting_name(string name){
	for(int i=0; i<clients.size(); i++)
	{
		if(clients[i].name == name)
			return i;
	}
	return -1;
}