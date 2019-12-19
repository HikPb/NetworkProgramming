#define _GNU_SOURCE 
#include <stdio.h>          /* These are the usual header files */
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/uio.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <libgen.h>

#include "protocol.h"
#include "validate.h"
#include "status.h"
char current_user[255];
int requestId;
int client_sock;
struct sockaddr_in server_addr; /* server's address information */
pthread_t tid;
char choose;
Message *mess;
int isOnline = 0;
char** listPath;
char** listFolder;
char** listFile;
#define DIM(x) (sizeof(x)/sizeof(*(x)))


/*
* count number param of command
* @param temp
* @return number param
*/
int numberElementsInArray(char** temp) {
	int i;
	if (temp!=NULL){
		for (i = 0; *(temp + i); i++)
		{
			// count number elements in array
		}
		return i;
	}
	return 0;
}

/*
* init new socket - print error if have error
* @param message, int connSock
* @return new socket
*/
int initSock(){
	int newsock = socket(AF_INET, SOCK_STREAM, 0);
	if (newsock == -1 ){
		perror("\nError: ");
		exit(0);
	}
	return newsock;
}

/*
* handle show notify
* @param notify
* @return void
*/
void *showBubbleNotify(void *notify){	
	char command[200];
	sprintf(command, "terminal-notifier -message \"%s\"", notify);
	system(command);
	return NULL;
}

/*
* handle bind new socket to server
* @param connect port, serverAddr
* @return void
*/ 
void bindClient(int port, char *serverAddr){
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = inet_addr(serverAddr);
}

void toNameOfFile(char *fileName, char* name ) {
    char** tokens = str_split(fileName, '/');
    int i;
    for (i = 0; *(tokens + i); i++)
    {
        // count number elements in array
    }
    strcpy(name, *(tokens + i -1));
}
void removeFile(char* fileName) {
	// remove file
    if (remove(fileName) != 0)
    {
        perror("Following error occurred\n");
    }
}
/*
* handle upload file to server
* @param message
* @return void
*/
void uploadFile() {
	char fileName[30];
	char fullPath[100];
	int i;
	printf("\n------------------ Upload File ------------------\n");
	printf("Please choose folder you want to upload into it :\n");
	printf("1. %-15s./%-28sFolder\n",current_user,current_user);
	for(i =0; *(listFolder+i);i++){
		char *temp = strdup(listFolder[i]);
		printf("%d. %-15s%-30sFolder\n", i+2, basename(temp), listFolder[i]);
		free(temp);
	}
	char choose[10];
	int option;
	while(1){
		printf("\nChoose (Press 0 to cancel): ");
		scanf(" %s", choose);
		while(getchar() != '\n');
		option = atoi(choose);
		if((option >= 0) && (option <= i+1)) {
			break;
		} else {
			printf("Please Select Valid Options!!\n");
		}
	}
	if(option == 0) return;
	printf("Please input the path of file you want to upload:");
	scanf("%[^\n]",fullPath);
	Message msg, sendMsg, recvMsg;
	FILE* fptr;
	if ((fptr = fopen(fullPath, "rb+")) == NULL){
        printf("Error: File not found\n");
    }
    else {
		if(option == 1){
			toNameOfFile(fullPath,fileName);
			sprintf(msg.payload, "./%s/%s", current_user,fileName);
		}else {
			sprintf(msg.payload, "%s/%s", listFolder[option-2],fileName);
		}
		msg.length = strlen(msg.payload);
		msg.type = TYPE_UPLOAD_FILE;
		msg.requestId = requestId;
		sendMessage(client_sock,msg);
		receiveMessage(client_sock,&recvMsg);
		if(msg.type == TYPE_ERROR){
			printf("Error: File name is used\n");
			fclose(fptr);
		}else{
			long filelen;
			fseek(fptr, 0, SEEK_END);          // Jump to the end of the file
			filelen = ftell(fptr);             // Get the current byte offset in the file       
			rewind(fptr);    // pointer to start of file
			int sumByte = 0;
			while(!feof(fptr)) {
				int numberByteSend = PAYLOAD_SIZE;
				if((sumByte + PAYLOAD_SIZE) > filelen) {// if over file size
					numberByteSend = filelen - sumByte; 
				}
				char* buffer = (char *) malloc((numberByteSend) * sizeof(char));
				fread(buffer, numberByteSend, 1, fptr); // read buffer with size 
				memcpy(sendMsg.payload, buffer, numberByteSend);
				sendMsg.length = numberByteSend;
				sumByte += numberByteSend; //increase byte send
				if(sendMessage(client_sock, sendMsg) <= 0) { 
					printf("Connection closed!\n");
					break;
				}
				free(buffer);
				if(sumByte >= filelen) {
					break;
				}
			}
			sendMsg.length = 0;
        	sendMessage(client_sock, sendMsg);
		}
    }
}

void handleSearchFile(char *fileName, char *listResult){
	int i;
	for(i=0;*(listFile+i);i++){
		if(strcmp(listFile[i],fileName)==0){
			strcat(listResult,listFile[i]);
			strcat(listResult,"\n");
		}
	}

}

void getDirectory(){
	Message sendMsg, recvMsg1, recvMsg2, recvMsg3;
	sendMsg.type = TYPE_REQUEST_DIRECTORY;
	sendMsg.requestId = requestId;
	sendMsg.length = 0;
	sendMessage(client_sock,sendMsg);
	//receive list path, list folder path, list file path from server 
	receiveMessage(client_sock,&recvMsg1);
	receiveMessage(client_sock,&recvMsg2);
	receiveMessage(client_sock,&recvMsg3);
	printf("%s",recvMsg3.payload);
	if(recvMsg1.length>0) listPath = str_split(recvMsg1.payload, '\n');
	if(recvMsg2.length>0) listFolder = str_split(recvMsg2.payload, '\n');
	if(recvMsg3.length>0) listFile = str_split(recvMsg3.payload, '\n');
	
}
/*
* show list user who have file
* @param 
* @return void
*/
void showDirectory() {
	char root[100];
	strcpy(root,"./");
	strcat(root,current_user);
	printf("\n---------------- Your Directory ----------------\n");
	int i;
	printf("   %-15s%-30s%-6s\n","Name","Path","Type");
	if(numberElementsInArray(listFolder)>0){
		for (i = 0; *(listFolder + i); i++){
			/*The POSIX version of dirname and basename may modify the content of the argument. 
			Hence, we need to strdup the local_file.*/
			char *temp = strdup(listFolder[i]); 
			char *temp2= strdup(listFolder[i]);
			if(strcmp(dirname(temp),root)==0)
			printf("%d. %-15s%-30sFolder\n", i+1, basename(temp2), listFolder[i]);
			free(temp);
			free(temp2);
		}
	}
	if(numberElementsInArray(listFile)>0){
		for (i = 0; *(listFile + i); i++){
			char *temp = strdup(listFile[i]); 
			char *temp2= strdup(listFile[i]);
			if(strcmp(dirname(temp),root)==0)
			printf("%d. %-15s%-30sFile\n", i+1, basename(temp2), listFile[i]);
			free(temp);
			free(temp2);
		}
	}
}

/*
* receive file from server and save
* @param filename, path
* @return void
*/
int handleSelectDownloadFile(char *selectLink) {
	char fileName[100];
	char listResult[1000];
	printf("Please Input Download File Name: ");
	scanf("%[^\n]s", fileName);
	handleSearchFile(fileName,listResult);
	char** tmp = str_split(listResult, '\n');
	int i;
	printf("   %-15s%-30s\n","Name","Path");
	for(i=0;*(*tmp+1);i++){
		printf("%d. %-15s%-30s\n", i+1,fileName , tmp[i]);
	}
	char choose[10];
    int option;
    while(1) {
	    printf("\nPlease select to download (Press 0 to cancel): ");
	    scanf(" %s", choose);
		while(getchar() != '\n');
		option = atoi(choose);
		if((option >= 0) && (option <= i)) {
			break;
		} else {
			printf("Please Select Valid Options!!\n");
		}
	}
	
	if(option == 0) {
		return -1;
	}
	else {
		strcpy(selectLink, tmp[option -1]);
	}
	return 1;
}

int download(char *link){
	Message sendMsg, recvMsg;
	FILE * fptr;
	char savePath[50];
	sendMsg.type = TYPE_REQUEST_DOWNLOAD;
	sendMsg.requestId = requestId;
	strcpy(sendMsg.payload,link);
	sendMsg.length= strlen(sendMsg.payload);
	sendMessage(client_sock,sendMsg);
	receiveMessage(client_sock,&recvMsg);
	if(recvMsg.type != TYPE_ERROR){
		printf("Please Input Saved Path in Local: ");
		scanf("%[^\n]s", savePath);
		fptr = fopen(savePath, "w+");
		while(1) {
			receiveMessage(client_sock, &recvMsg);
			if(recvMsg.type == TYPE_ERROR) {
				fclose(fptr);
				removeFile(savePath);
				return -1;
			}
			if(recvMsg.length > 0) { 
				fwrite(recvMsg.payload, recvMsg.length, 1, fptr);
			} else {
				break;
			}
		}
		fclose(fptr);
		return 1;
	}else return -1;
}
/*
* method download
* @param filename, path
* @return void
*/
void downloadFile() {
	char selectLink[50];
	if(handleSelectDownloadFile(selectLink)==1){
		printf("......................Donwloading..........\n");
		if(download(selectLink) == -1) {
			showBubbleNotify("Error: Something Error When Downloading File!!");
			printf("Error: Something Error When Downloading File!!\n");
			return;
		}
		char message[100];
		sprintf(message, "...Donwload Success..\n");
		showBubbleNotify(message);
		//printf("...Donwload Success...");
	}else return;
}

/*
* how to use system manual
* @param 
* @return void
*/
void manual() {
	printf("\n---- For search and download file from system press 1 and type file name\n");
	printf("---- For view list file in your folder press 2\n");
	
	char choose;	
	while(1) {
		printf("Press Q/q for back to main menu: ");
		scanf(" %c", &choose);
		while(getchar() != '\n');
		if((choose == 'q') || (choose == 'Q')) break;
	}
	return;
}

// connect client to server
// parameter: client socket, server address
// if have error, print error and exit
void connectToServer(){
	if(connect(client_sock, (struct sockaddr*) (&server_addr), sizeof(struct sockaddr)) < 0){
		printf("\nError!Can not connect to sever! Client exit imediately!\n");
		exit(0);
	
	}
}

/*
* print waiting message to screen when waitng download
* @param
* @return void
*/
void printWatingMsg() {
	printf("\n..................Please waiting................\n");
}

// get username and password from keyboard to login
void getLoginInfo(char *str){
	char username[255];
	char password[255];
	printf("Enter username?: ");
	scanf("%[^\n]s", username);
	while(getchar() != '\n');
	printf("Enter password?: ");
	scanf("%[^\n]s", password);
	while(getchar()!='\n');
	sprintf(mess->payload, "LOGIN\nUSER %s\nPASS %s", username, password);
	strcpy(str, username);
}

/*
* get login info and login
* @param current user
* @return void
*/
void loginFunc(char *current_user){
	char username[255];
	mess->type = TYPE_AUTHENTICATE;
	getLoginInfo(username);
	mess->length = strlen(mess->payload);
	sendMessage(client_sock, *mess);
	receiveMessage(client_sock, mess);
	if(mess->type != TYPE_ERROR){
		isOnline = 1;
		strcpy(current_user, username);
		requestId = mess->requestId;
		getDirectory();
	} else {
		showBubbleNotify("Error: Login Failed!!");
	}
	printf("%s\n", mess->payload);
}

/*
* get register info 
* @param user
* @return void
*/
int getRegisterInfo(char *user){
	char username[255], password[255], confirmPass[255];
	printf("Username: ");
	scanf("%[^\n]s", username);
	printf("Password: ");
	while(getchar()!='\n');
	scanf("%[^\n]s", password);
	printf("Confirm password: ");
	while(getchar()!='\n');
	scanf("%[^\n]s", confirmPass);
	while(getchar()!='\n');
	if(!strcmp(password, confirmPass)){
		sprintf(mess->payload, "REGISTER\nUSER %s\nPASS %s", username, password);
		strcpy(user, username);
		return 1;
	}
	else{
		printf("Confirm password invalid!\n");
		return 0;
	}
	
}

/*
* get register info and login
* @param current user
* @return void
*/
void registerFunc(char *current_user){
	char username[255];
	if(getRegisterInfo(username)){
		mess->type = TYPE_AUTHENTICATE;
		mess->length = strlen(mess->payload);
		sendMessage(client_sock, *mess);
		receiveMessage(client_sock, mess);
		if(mess->type != TYPE_ERROR){
			isOnline = 1;
			strcpy(current_user, username);
			requestId = mess->requestId;
			//getDirectory();
		} else {
			showBubbleNotify("Error: Register Failed!!");
		}
		printf("%s\n", mess->payload);
	}
}

/*
* logout from system
* @param current user
* @return void
*/
void logoutFunc(char *current_user){
	mess->type = TYPE_AUTHENTICATE;
	sprintf(mess->payload, "LOGOUT\n%s", current_user);
	mess->length = strlen(mess->payload);
	sendMessage(client_sock, *mess);
	receiveMessage(client_sock, mess);
	if(mess->type != TYPE_ERROR){
		isOnline = 0;
		current_user[0] = '\0';
		requestId =0;
	}
	printf("%s\n", mess->payload);
}

/*
* get show first menu of application 
* @param 
* @return void
*/
void menuAuthenticate() {
	printf("\n------------------Storage System------------------\n");
	printf("\n1 - Login");
	printf("\n2 - Register");
	printf("\n3 - Exit");
	printf("\nChoose: ");
}

/*
* get show main menu of application 
* @param 
* @return void
*/
void mainMenu() {
	printf("\n------------------Storage System------------------\n");
	printf("\n1 - Upload file");
	printf("\n2 - Download File");
	printf("\n3 - User Manual");
	printf("\n4 - Logout");
	printf("\nPlease choose: ");
}


/*
* get check type request of authenticate
* @param 
* @return void
*/
void authenticateFunc() {
	menuAuthenticate();
	scanf(" %c", &choose);
	while(getchar() != '\n');
	switch (choose){
		case '1':
			loginFunc(current_user);
			break;
		case '2':
			registerFunc(current_user);
			break;
		case '3': 
			exit(0);
		default:
			printf("Syntax Error! Please choose again!\n");
	}
	
}

/*
* check type file request
* @param 
* @return void
*/
void requestFileFunc() {
	showDirectory();
	mainMenu();
	scanf(" %c", &choose);
	while(getchar() != '\n');
	switch (choose) {
		case '1':
			uploadFile();
			break;
		case '2':
			downloadFile();
			break;
		case '3':
			manual();
			break;
		case '4':
			logoutFunc(current_user);
			break;
		default:
			printf("Syntax Error! Please choose again!\n");
	}
}

// communicate from client to server
// send and recv message with server
void communicateWithUser(){
	while(1) {
		if(!isOnline) {
			authenticateFunc();
		} else {
			requestFileFunc();
		}
	}
}

/*
* Main function
* @param int argc, char** argv
* @return 0
*/
int main(int argc, char const *argv[])
{
	// check valid of IP and port number 
	if(argc!=3){
		printf("Error!\nPlease enter two parameter as IPAddress and port number!\n");
		exit(0);
	}
	char *serAddr = malloc(sizeof(argv[1]) * strlen(argv[1]));
	strcpy(serAddr, argv[1]);
	int port = atoi(argv[2]);
	mess = (Message*) malloc (sizeof(Message));
	mess->requestId = 0;
 	if(!validPortNumber(port)) {
 		perror("Invalid Port Number!\n");
 		exit(0);
 	}
	if(!checkIP(serAddr)) {
		printf("Invalid Ip Address!\n"); // Check valid Ip Address
		exit(0);
	}
	strcpy(serAddr, argv[1]);
	if(!hasIPAddress(serAddr)) {
		printf("Not found information Of IP Address [%s]\n", serAddr); // Find Ip Address
		exit(0);
	}

	//Step 1: Construct socket
	client_sock = initSock();
	//Step 2: Specify server address

	bindClient(port, serAddr);
	
	//Step 3: Request to connect server
	connectToServer();

	//Step 4: Communicate with server			
	communicateWithUser();

	close(client_sock);
	return 0;
}