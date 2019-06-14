#include <stdio.h> /* printf(), fprintf() */ 
#include <sys/socket.h> /* socket(), bind(), connect() */ 
#include <arpa/inet.h> /* sockaddr_in, inet_ntoa() */ 
#include <stdlib.h> /* atoi() */ 
#include <string.h> /* memset() */ 
#include <unistd.h> /* read(), write(), close() */ 
#include <signal.h>
#include <sys/wait.h>
#include "cJSON.h" //Biblioteka JSON
#include "cJSON.c"

#define MAXCLIENTS 50 
#define BUFWE 80
#define MAXBUFFOR 255

struct UserStruct {
    char Username[25];
    char PublicKey[180];
};

int clientsCounter = 0;

void ObslugaKlienta(int klientGniazdo);
void sig_child(int s);
char* CreateJSONUserList(struct UserStruct usersArray[],int length,int returnCode,char* message);

int main(int argc, char *argv[]) 
{ 
    int serverSocket; 
    int clientSocket; 
    struct sockaddr_in serverAddress; /* adres lokalny */ 
    struct sockaddr_in clientAddress; /* adres klienta */ 
    unsigned short serverPort; 
    unsigned int klientDl; /* długość struktury adresowej */
    struct UserStruct usersArray[MAXCLIENTS];
    char ConnectionBuffor[MAXBUFFOR];

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

    signal(SIGCHLD, sig_child);

    /* Obsługuj nadchodzące połączenia */ 
    for (;;) 
    { 
        int procesID;
        klientDl= sizeof(clientAddress); 
        if ((clientSocket = accept(serverSocket, (struct sockaddr *) &clientAddress, &klientDl)) < 0)
        {
            perror("accept() -nie udalo się"); 
            exit(1); 
        } 

        if(clientsCounter >= MAXCLIENTS)
        {
            char* JSONUserList = CreateJSONUserList(usersArray,clientsCounter,102,"Nie udało się połączyć z serwerem zbyt wiele klientów");
            if (write(clientSocket, JSONUserList, strlen(JSONUserList)) < 0)
            {
                perror("write() - nie udalo się");
            }
            close(clientSocket);
            sleep(1);
            continue;
        }

        printf("Przetwarzam klienta %s\n", inet_ntoa(clientAddress.sin_addr));

        if (read(clientSocket,ConnectionBuffor,MAXBUFFOR) < 0)
		{
			perror("read()");
			exit(1);
		}

        cJSON *json = cJSON_Parse(ConnectionBuffor);
        int requestCode = json->child->valueint;
        if(requestCode == 100)
        {
            strcpy(usersArray[clientsCounter].Username,json->child->next->valuestring);
            strcpy(usersArray[clientsCounter].PublicKey,json->child->next->next->valuestring);

            //length do zmiany na ClientsCounter
            char* JSONUserList = CreateJSONUserList(usersArray,1,101,"Zalogowano");

            if (write(clientSocket, JSONUserList, strlen(JSONUserList)) < 0)
            {
                perror("write() - nie udalo się");
                continue;
            }

        }

        cJSON_Delete(json);

        if ((procesID=fork()) < 0)
        {
		    printf("Fork error\n");
		    exit(-1);
	    }
        else if (procesID==0) {
            	//ObslugaKlienta(clientSocket);
            	exit(1);
        }
        else if(procesID > 0)
        {
            close(clientSocket);
            clientsCounter++;
        }
    } 

    close (serverSocket);
    return 0;
} 

void ObslugaKlienta(int klientGniazdo) 
{ 
    char echoBufor[BUFWE]; 
    int otrzTekstDl; /* Odbierz komunikat od klienta */ 
    otrzTekstDl = read(klientGniazdo, echoBufor, BUFWE); 
    if (otrzTekstDl < 0) 
    { 
        perror("read() -nie udalo się"); 
        exit(1); 
    } /* Odeślij otrzymany komunikat i odbieraj kolejne komunikaty do zakończeniatransmisji przez klienta */ 
    while (otrzTekstDl > 0) 
    { /* Odeślij komunikat do klienta */ 
        if (write(klientGniazdo, echoBufor, otrzTekstDl) != otrzTekstDl) 
        { 
            perror("write() -nie udalo się"); 
            exit(1); 
        } /* Sprawdź, czy są nowe dane do odebrania */ 
        if ((otrzTekstDl = read(klientGniazdo,echoBufor,BUFWE)) < 0) 
        { 
            perror("read() -nie udalo się"); 
            exit(1); 
        } 
    } 
    close(klientGniazdo); 
}

void sig_child(int s)
{
    while ( waitpid(-1, 0, WNOHANG) > 0 )
    clientsCounter-- ;
}

char* CreateJSONUserList(struct UserStruct usersArray[],int length,int returnCode,char* message)
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

    for (int index = 0; index < length; ++index)
    {
        cJSON *user = cJSON_CreateObject();

        cJSON_AddStringToObject(user, "username", usersArray[index].Username);
        cJSON_AddStringToObject(user, "publicKey", usersArray[index].PublicKey);

        cJSON_AddItemToArray(users,user);
    }

    userListJSON = cJSON_Print(userList);
    if (userListJSON == NULL) {
        fprintf(stderr, "Failed to parse UserList.\n");
    }

    cJSON_Delete(userList);
    return userListJSON;
}