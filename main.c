#include <stdio.h> /* printf(), fprintf() */ 
#include <sys/socket.h> /* socket(), bind(), connect() */ 
#include <arpa/inet.h> /* sockaddr_in, inet_ntoa() */ 
#include <stdlib.h> /* atoi() */ 
#include <string.h> /* memset() */ 
#include <unistd.h> /* read(), write(), close() */ 
#include <signal.h>
#include <sys/wait.h>
#include "cJSON.h" //Biblioteka JSON
#include "cJSON.c"  //Biblioteka JSON
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>

#define MAXCLIENTS 50 
#define BUFSENDFILESTRUCT 4096
#define MAXBUFFOR 1024

struct UserStruct {
    char Username[25];
    char PublicKeyModulus[400];
    char PublicKeyExponent[100];
    int  Socket;
    bool Blocked;
};

struct linked_list
{
    struct UserStruct *user;
    struct linked_list *next;
};

typedef struct linked_list node;
node *head=NULL, *last=NULL;

struct WatekArg { int klientGniazdo;};

int clientsCounter = 0;

char* CreateJSONUserList(int returnCode,char* message);
int SaveFile(char* filename,char* content);
char* CreateJSONFileInfo(int returnCode,char * encryptedName,char * encryptedKey, char* encryptedIV);
void *ClientThread(void *watekArg);
int LoadFile(char* filename,char* outarry);
int SendRefreshUserList();
void ClearClientThread(int klientGniazdo,bool sendMessage);
int GetRandomNumber();
void ClearFolder(char* folderName);
int LoginUser(int clientSocket, void *ConnectionBuffor,cJSON *json);

void print_linked_list();
void insert_at_last(struct UserStruct newUser);
void delete_item(int socket);
node* search_item_by_mod(char modulus[]);
node* search_item_by_socket(int socket);




int main(int argc, char *argv[])
{ 
    int serverSocket; 
    int clientSocket; 
    struct sockaddr_in serverAddress; /* adres lokalny */ 
    struct sockaddr_in clientAddress; /* adres klienta */ 
    unsigned short serverPort; 
    unsigned int klientDl; /* długość struktury adresowej */
    char ConnectionBuffor[MAXBUFFOR];
    pthread_t watekID;
    struct WatekArg *watekArg;

    if (argc != 2) 
    { 
        fprintf(stderr, "Użycie: %s <Serwer Port>\n", argv[0]); 
        exit(1); 
    } 
    serverPort = atoi(argv[1]); 
    
    /* brak spr *//* Utwórz gniazdo dla przychodzących połączeń */ 
    if ((serverSocket = socket(PF_INET, SOCK_STREAM, 0)) < 0) 
    { 
        perror("socket() -nie udalo się");
        exit(1); 
    } 

    /* Zbuduj lokalną strukturę adresową */ 
    memset(&serverAddress, 0, sizeof(serverAddress)); 
    serverAddress.sin_family = AF_INET; 
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY); 
    serverAddress.sin_port = htons(serverPort);/* Przypisz gniazdu lokalny adres */ 
    if (bind(serverSocket,(struct sockaddr *) &serverAddress,sizeof(serverAddress)) < 0) 
    {
        perror("bind() -nie udalo się");
        exit(1); 
    } 
    
    /* Ustaw gniazdo w trybie biernym -przyjmowania połączeń*/ 
    if (listen(serverSocket, MAXCLIENTS) < 0) 
    {
        perror("listen() -nie udalo się");
        exit(1); 
    }

    /* Obsługuj nadchodzące połączenia */ 
    for (;;) 
    { 
        int procesID;
        klientDl= sizeof(clientAddress);
        //Akceptuj połączenie
        if ((clientSocket = accept(serverSocket, (struct sockaddr *) &clientAddress, &klientDl)) < 0)
        {
            perror("accept() -nie udalo się");
            continue;
        } 

        if(clientsCounter >= MAXCLIENTS)
        {
            if (write(clientSocket, "102", strlen("102")) < 0)
            {
                perror("write() - nie udalo się");
            }
            close(clientSocket);
            sleep(1);
            continue;
        }

        clientsCounter++;
        printf("Przetwarzam klienta %s\n", inet_ntoa(clientAddress.sin_addr));
        //Przyjmij dane do logowania od klienta
        if (read(clientSocket,ConnectionBuffor,MAXBUFFOR) < 0)
		{
			perror("Nie udało się zalogować użytkownika!");
			close(clientSocket);
			clientsCounter--;
            continue;
		}

        cJSON *json = cJSON_Parse(ConnectionBuffor);
        if(json == NULL)
        {
            perror("Nie udało się zalogować użytkownika!");
            close(clientSocket);
            clientsCounter--;
            continue;
        }

        int requestCode = json->child->valueint;
        //Jezeli klient prosił o logowanie dodaj go do listy klientów i odeślij liste
        if(requestCode == 100)
        {
            if(LoginUser(clientSocket, ConnectionBuffor, json) != 0)
                continue;
        }

        cJSON_Delete(json);

        if ((watekArg = (struct WatekArg *) malloc(sizeof(struct WatekArg))) == NULL)
        {
            if (write(clientSocket, "500", strlen("500")) < 0)
            {
                printf("Nie udało się wysłać komunikatu o błedzie serwera do klienta %d",clientSocket);
            }

            close(clientSocket);
            clientsCounter--;
            delete_item(clientSocket);
        }

        watekArg -> klientGniazdo = clientSocket;

        if (pthread_create(&watekID, NULL, ClientThread, (void *) watekArg) != 0)
        {
            if (write(clientSocket, "500", strlen("500")) < 0)
            {
                printf("Nie udało się wysłać komunikatu o błedzie serwera do klienta %d",clientSocket);
            }

            close(clientSocket);
            clientsCounter--;
            delete_item(clientSocket);
        }

    }
}

void *ClientThread(void *watekArg)
{ 
    char JSONFileStruct[BUFSENDFILESTRUCT];
    int length; /* Odbierz komunikat od klienta */
    int klientGniazdo;

    pthread_detach(pthread_self());
    klientGniazdo = ((struct WatekArg *) watekArg) -> klientGniazdo;
    free(watekArg);

    for(;;)
    {
        length = read(klientGniazdo, JSONFileStruct, BUFSENDFILESTRUCT);

        if (length <= 0) {
            perror("read() -nie udalo się");
            ClearClientThread(klientGniazdo,false);
        }

        cJSON *json = cJSON_Parse(JSONFileStruct);
        int requestCode = json->child->valueint;

        if(requestCode == 105)
        {
            struct stat st = {0};
            char* EncryptedName = json->child->next->valuestring;
            char* EncryptedKey = json->child->next->next->next->valuestring;
            char* EncryptedIV = json->child->next->next->next->next->valuestring;
            char* Receiver = json->child->next->next->next->next->next->valuestring;

            node* sender = search_item_by_socket(klientGniazdo);
            sender->user->Blocked = true;

            node* receiver = search_item_by_mod(Receiver);
            receiver->user->Blocked = true;
            int receiverSocket = receiver->user->Socket;

            time_t t = time(NULL);
            struct tm tm = *localtime(&t);
            int intFolderName = receiverSocket + tm.tm_mday + tm.tm_hour + tm.tm_min + GetRandomNumber();
            char folderName[12];
            sprintf(folderName, "%d", intFolderName);

            if (stat(folderName, &st) == -1)
            {
                if(mkdir(folderName, 0700) == -1)
                {
                    printf("Nie udało się stworzyć folderu\n");
                    ClearClientThread(klientGniazdo,true);
                }

                chdir(folderName);

                SaveFile("FileName",EncryptedName);
                SaveFile("EncryptedKey",EncryptedKey);
                SaveFile("EncryptedIV",EncryptedIV);

                //Wyslanie do klienta wiadomosci o gotowosci na przyjecie pliku
                if (write(klientGniazdo, "106", strlen("106")) < 0)
                {
                    perror("write() - nie udalo się");
                    ClearClientThread(klientGniazdo,false);
                }

                int fd = open("File", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

                if (fd == -1)
                {
                    fprintf(stderr, "Could not open destination file.\n");
                    ClearClientThread(klientGniazdo,true);
                }

                int readCounter = 0;
                char buf[MAXBUFFOR];
                int writeCounter;
                char* bufptr;

                while((readCounter = read(klientGniazdo, buf, MAXBUFFOR)) > 0)
                {

                    bufptr = buf;

                    writeCounter = write(fd, bufptr, readCounter);

                    if(writeCounter < 0)
                    {
                        printf("Blad przy zapisie do pliku!\n");
                        ClearClientThread(klientGniazdo,true);
                    }
                }
                close(fd);

                char* request = NULL;

                cJSON *requestJSON = cJSON_CreateObject();
                cJSON_AddNumberToObject(requestJSON,"requestCode",107);
                cJSON_AddNumberToObject(requestJSON,"folderName",intFolderName);
                cJSON_AddStringToObject(requestJSON,"sender",sender->user->Username);

                request = cJSON_Print(requestJSON);
                if (request == NULL) {
                    fprintf(stderr, "Failed to parse request.\n");
                }

                cJSON_Delete(requestJSON);

                //Wyslanie do adresata, wiadomosci o gotowosci pliku do sciagniecia
                if (write(receiverSocket, request, strlen(request)) < 0)
                {
                    perror("write() - nie udalo się");
                    continue;
                }
                chdir("..");
                sender->user->Blocked = false;
                ClearClientThread(klientGniazdo,false);
            }

        }
        else if(requestCode == 108)
        {
            char* folderName = json->child->next->valuestring;
            chdir(folderName);

            char EncryptedName[344];
            LoadFile("FileName",EncryptedName);
            char EncryptedKey[344];
            LoadFile("EncryptedKey",EncryptedKey);
            char EncryptedIV[344];
            LoadFile("EncryptedIV",EncryptedIV);

            char* fileInfo =  CreateJSONFileInfo(110,EncryptedName,EncryptedKey,EncryptedIV);

            if (write(klientGniazdo, fileInfo, strlen(fileInfo)) < 0)
            {
                perror("write() - nie udalo się");
                ClearClientThread(klientGniazdo,false);
            }

            char requestCodeChar[3];
            int length; /* Odbierz komunikat od klienta */

            length = read(klientGniazdo, requestCodeChar, 3);

            if (length <= 0) {
                perror("read() -nie udalo się");
                ClearClientThread(klientGniazdo,false);
            }

            int requestCode = atoi(requestCodeChar);

            if(requestCode == 111)
            {
                char* bufptr;
                char buf[MAXBUFFOR];
                /* otworz plik do czytania */
                int fd = open("File", O_RDONLY , S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

                if (fd == -1)
                {
                    fprintf(stderr, "Could not open file for reading!\n");
                    ClearClientThread(klientGniazdo,true);
                }
                int readCounter = 0;
                int writeCounter = 0;

                /* czytaj plik i przesylaj do klienta */
                while((readCounter = read(fd, buf, MAXBUFFOR)) > 0)
                {
                    writeCounter = 0;
                    bufptr = buf;
                    while (writeCounter < readCounter)
                    {

                        readCounter -= writeCounter;
                        bufptr += writeCounter;
                        writeCounter = write(klientGniazdo, bufptr, readCounter);
                        if (writeCounter == -1)
                        {
                            fprintf(stderr, "Could not write file to client!\n");
                            break;
                        }
                    }
                }
                ClearFolder(folderName);
                node* receiver = search_item_by_socket(klientGniazdo);
                receiver->user->Blocked = false;
                ClearClientThread(klientGniazdo,false);
            }


        }
        else if(requestCode == 109)
        {
            char* folderName = json->child->next->valuestring;

            if (chdir(folderName) != 0)
                perror("chdir() failed");

            ClearFolder(folderName);

            ClearClientThread(klientGniazdo,true);
        }
    }

    close(klientGniazdo); 
}

int LoginUser(int clientSocket, void *ConnectionBuffor, cJSON *json)
{
    struct UserStruct newUser;

    strcpy(newUser.Username, json->child->next->valuestring);
    strcpy(newUser.PublicKeyModulus, json->child->next->next->valuestring);
    strcpy(newUser.PublicKeyExponent, json->child->next->next->next->valuestring);
    newUser.Socket = clientSocket;
    newUser.Blocked = false;

    if (write(clientSocket, "101", strlen("101")) < 0) {
        perror("write() - nie udalo się wysłać listy użytkowników");
        close(clientSocket);
        clientsCounter--;
        return -1;
    }

    if (read(clientSocket, ConnectionBuffor, MAXBUFFOR) < 0) {
        perror("Nie udało się zalogować użytkownika!");
        close(clientSocket);
        clientsCounter--;
        return -1;
    }

    insert_at_last(newUser);
    SendRefreshUserList();
    return 0;
}

//Tworzy lite userow jako obiekt typu JSON
char* CreateJSONUserList(int returnCode,char* message)
{
    char* userListJSON = NULL;

    cJSON *userList = cJSON_CreateObject();
    cJSON_AddNumberToObject(userList,"returnCode",returnCode);
    cJSON_AddStringToObject(userList,"message",message);
    cJSON *users = cJSON_AddArrayToObject(userList, "users");

    if (users == NULL)
    {
        cJSON_Delete(userList);
        return userListJSON;
    }

    node *myList;
    myList = head;

    while(myList!=NULL)
    {
        cJSON *user = cJSON_CreateObject();

        cJSON_AddStringToObject(user, "username", myList->user->Username);
        cJSON_AddStringToObject(user, "message", message);
        cJSON_AddStringToObject(user, "publicKeyModulus", myList->user->PublicKeyModulus);
        cJSON_AddStringToObject(user, "publicKeyExponent", myList->user->PublicKeyExponent);

        cJSON_AddItemToArray(users,user);

        myList = myList->next;
    }

    userListJSON = cJSON_Print(userList);
    if (userListJSON == NULL) {
        fprintf(stderr, "Failed to parse UserList.\n");
    }

    cJSON_Delete(userList);
    return userListJSON;
}

char* CreateJSONFileInfo(int returnCode,char * encryptedName,char * encryptedKey, char* encryptedIV)
{
    char* FileInfoJSON = NULL;

    cJSON *fileInfo = cJSON_CreateObject();
    cJSON_AddNumberToObject(fileInfo,"returnCode",returnCode);
    cJSON_AddStringToObject(fileInfo,"encryptedName",encryptedName);
    cJSON_AddStringToObject(fileInfo,"encryptedKey",encryptedKey);
    cJSON_AddStringToObject(fileInfo,"encryptedIV",encryptedIV);

    FileInfoJSON = cJSON_Print(fileInfo);
    if (FileInfoJSON == NULL) {
        fprintf(stderr, "Failed to parse UserList.\n");
    }

    cJSON_Delete(fileInfo);
    return FileInfoJSON;
}

int SaveFile(char* filename,char* content)
{
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if (fd == -1)
    {
        fprintf(stderr, "Could not open destination file, using stdout.\n");
        return -1;
    }

    if(write(fd, content, strlen(content)) < 0)
    {
        printf("Blad przy zapisie do pliku!\n");
        return -1;
    }

    close(fd);

    return 0;
}

int LoadFile(char* filename,char* outarry)
{
    char buf[344];
    int fd = open(filename, O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if (fd == -1)
    {
        fprintf(stderr, "Could not open destination file.\n");
        return -1;
    }

    /* czytaj plik i przesylaj do klienta */
    if((read(fd, buf, MAXBUFFOR)) < 0)
    {
        fprintf(stderr, "Could not read destination file.\n");
        return -1;
    }

    strcpy(outarry,buf);


    return 0;
}

void insert_at_last(struct UserStruct newUser)
{
    node *temp_node;
    temp_node = (node *) malloc(sizeof(node));
    temp_node->user = (struct UserStruct*) malloc(sizeof(struct UserStruct));

    temp_node->user->Socket = newUser.Socket;
    strcpy (temp_node->user->Username,newUser.Username);
    strcpy (temp_node->user->PublicKeyModulus,newUser.PublicKeyModulus);
    strcpy (temp_node->user->PublicKeyExponent,newUser.PublicKeyExponent);
    temp_node->next=NULL;

    //For the 1st element
    if(head==NULL)
    {
        head=temp_node;
        last=temp_node;
    }
    else
    {
        last->next=temp_node;
        last=temp_node;
    }
    print_linked_list();
}

void delete_item(int socket)
{
    node *myNode = head, *previous=NULL;
    int flag = 0;

    while(myNode!=NULL)
    {
        if(myNode->user->Socket == socket)
        {
            if(previous==NULL)
                head = myNode->next;
            else
                previous->next = myNode->next;

            //printf("%d uzytkowik o podanym socketcie zostal usuniety\n",s);

            flag = 1;
            free(myNode->user);
            free(myNode); //need to free up the memory to prevent memory leak
            break;
        }

        previous = myNode;
        myNode = myNode->next;
    }

    if(flag==0)
        printf("Key not found!\n");

    print_linked_list();
}


node* search_item_by_mod(char modulus[])
{
    node *searchNode = head;


    while(searchNode!=NULL)
    {
        if(strcmp(searchNode->user->PublicKeyModulus, modulus) == 0)
        {
            printf("Zwracany user po mod: %d\n",searchNode->user->Socket);

            return searchNode;
        }
        else
            searchNode = searchNode->next;
    }

    return NULL;

}

node* search_item_by_socket(int socket)
{
    node *searchNode = head;


    while(searchNode!=NULL)
    {
        if(searchNode->user->Socket == socket)
        {
            printf("Zwracany user po sokecie: %d\n",searchNode->user->Socket);

            return searchNode;
        }
        else
            searchNode = searchNode->next;
    }

    return NULL;

}


void print_linked_list()
{
    printf("\nYour full linked list is\n");

    node *myList;
    myList = head;

    while(myList!=NULL)
    {
        printf("Sokcet:%d \n", myList->user->Socket);
        printf("Username:%s \n", myList->user->Username);
        printf("PublicKeyModulus:%s \n", myList->user->PublicKeyModulus);
        printf("PublicKeyExponent:%s \n\n", myList->user->PublicKeyExponent);

        myList = myList->next;
    }
    puts("");
}

int SendRefreshUserList()
{
    char* JSONUserList = CreateJSONUserList(112,"Zalogowano");

    node *myList;
    myList = head;

    while(myList!=NULL)
    {
        if (myList->user->Blocked == false && write(myList->user->Socket, JSONUserList, strlen(JSONUserList)) < 0)
        {
            printf("Nie udało się wysłać nowej listy do klienta %d",myList->user->Socket);
            continue;
        }

        myList = myList->next;
    }
}

void ClearClientThread(int klientGniazdo,bool sendMessage)
{
    if(sendMessage)
    {
        if (write(klientGniazdo, "500", strlen("500")) < 0)
        {
            printf("Nie udało się wysłać komunikatu o błedzie serwera do klienta %d\n",klientGniazdo);
        }
    }

    close(klientGniazdo);
    delete_item(klientGniazdo);
    SendRefreshUserList();
    clientsCounter--;
    int ret = -1;
    pthread_exit(&ret);
}

int GetRandomNumber()
{
    int i, zarodek;
    time_t tt;
    zarodek = time(&tt);
    srand(zarodek);

    return rand()%10000;
}

void ClearFolder(char* folderName)
{

    if (remove("FileName") != 0)
        perror("remove() failed");

    if (remove("EncryptedKey") != 0)
        perror("remove() failed");

    if (remove("EncryptedIV") != 0)
        perror("remove() failed");

    if (remove("File") != 0)
        perror("remove() failed");

    chdir("..");

    if (remove(folderName) != 0)
        perror("remove() failed");
}