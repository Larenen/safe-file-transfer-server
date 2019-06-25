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

/**
* @brief Maksymalna ilość osób na serwerze
 */
#define MAXCLIENTS 50 
/**
* @brief Maksymalny rozmiar bufora dla struktur z informacja o pliku
 */
#define BUFSENDFILESTRUCT 4096
/**
* @brief Maksymalny rozmiar bufora
 */
#define MAXBUFFOR 1024

/**
 * \brief Struktura przechowująca dane zalogowanego użytkownika
 */
struct UserStruct {
    char Username[25];
    char PublicKeyModulus[400];
    char PublicKeyExponent[100];
    int  Socket;
    bool Blocked;
};

/**
 * \brief Struktura przechowująca dane użytkowników na liście
 */
struct linked_list
{
    struct UserStruct *user;
    struct linked_list *next;
};

/**
* @brief Alias na strukture listy
 */
typedef struct linked_list node;
/**
* @brief Adres na początek listy
 */
node *head=NULL, 
/**
* @brief Adres na koniec listy
 */
node *last=NULL;

/**
* @brief Struktura przechowujaca argumenty wątku klienta
 */
struct WatekArg { int klientGniazdo;};

/**
* @brief Licznik przechowujący informacje o ilości osób na serwerze
 */
int clientsCounter = 0;

/**
 * @brief Funkcja tworzy liste użytkowników w formacie JSON
 * @param returnCode Kod zwrotny dla klienta jaki chcemy załączyć
 * @param message Wiadomość zwrotna dla klienta
 * @return Zwraca tablice char która zawiera liste użytkowników w formacie JSON
 *
 * Funkcja przechodzi po liscie z użytkownikami i dodaje ich pola takie jak nazwa użytkownika, klucz publiczny do obiektu JSON następnie serialzuje go tworząc z niego tablice charów.
 */
char* CreateJSONUserList(int returnCode,char* message);
/**
 * @brief Funkcja do zapisu pliku na dysk serwera.
 * @param filename Nazwa pliku pod jakim chcemy zapisać plik
 * @param content Zawartośc zapisywanego pliku
 * @return Zwraca 0 jeżeli się udało 1 jeżeli wystąpił błąd
 *
 * Funkcja tworzy i otwiera nowy plik o nazwie podanej jako pareamter, nastepnie zapisuje do tego pliku zawartość jaka została podana jako argument do funkcji.
 */
int SaveFile(char* filename,char* content);
/**
 * @brief Funkcja tworzy informacje o pliku w formacie JSON
 * @param returnCode Kod zwrotny dla klienta jaki chcemy załączyć
 * @param encryptedName Zaszyfrowana przy pomocy RSA nazwa pliku
 * @param encryptedKey Zaszyfrowany przy pomocy RSA klucz AES
 * @param encryptedIV Zaszyfrowany przy pomocy RSA wektor inicjujący AES.
 * @return Zwraca tablice char która zawiera informacje o pliku w formacie JSON
 *
 * Funkcja tworzy objekt w formacjie JSON przy pomocy podanych argumentów
 */
char* CreateJSONFileInfo(int returnCode,char * encryptedName,char * encryptedKey, char* encryptedIV);
/**
 * @brief Funkcja która jest wywoływana w wątku dla każdego nowego klienta
 * @param watekArg Argumenty dla wątku
 *
 *Funkcja która opisuje działanie wątku dla każdego nowego klienta który pojawi sie na serwerze. Obsługuje trzy funkcje jakimi są zapis pliku na serwerze, odebranie pliku z serwera oraz odrzucenie pliku do pobrania z serwera.
 */
void *ClientThread(void *watekArg);
/**
 * @brief Wczytuje plik do pamięci serwera w postaci tablicy char. Słuzy do wczytywania kluczy AES oraz nazwy pliku zaszyfrowanych przy pomocy RSA.
 * @param filename Nazwa pliku który chcemy wczytać
 * @param outarry Tablica do której chcemy wpisać plik
 * @return Zwraca 0 jeżeli się udało 1 jeżeli wystąpił błąd
 * @warning Pliki o maksymalnym rozmiarze 344 B
 *
 * Funkcja otwiera do czytania plik podany jak argument filename, następnie czyta z niego zapisując to tymczasowego bufora, nastepnie kopiuje buffor do zmiennej outarray
 */
int LoadFile(char* filename,char* outarry);
/**
 * @brief Służy do wysłania aktywnym klientom listy aktywnych użytkowników.
 *
 *  Funkcja przechodzi po liscie aktywnych klientów sprawdzajac czy nie są oni zablokowani przez operacje przesyłania plików, jezeli nie to tworzy obiekt JSON z listą użytkowników i wysyła ją do aktualnie rozpatrywanego klienta.
 */
void SendRefreshUserList();
/**
 * @brief Sprząta po wątku klienta i zamyka go
 * @param klientGniazdo gniazdo klienta konczącego działanie
 *
 * Funkcja zamyka gniazdo klienta usuwa klienta z listy aktualnie dostępnych, zmniejsza licznik połączonych klientów, rozsyła do aktywnych klientów zaktualizowane liste użytkowników i kończy wątek klienta.
 */
void ClearClientThread(int klientGniazdo);
/**
 * @brief Losuje losową liczbe.
 * @return Zwraca wylosowana liczbę
 *
 * Funkcja losuje liczbe z zakresu o do 10000 która następnie służy do wygenerowania wartości dla folderu klienta w którym tymczasowo będą przechowywane przesyłane pliki.
 */
int GetRandomNumber();
/**
 * @brief Czyści i usuwa folder w którym tymczasowo były przechowowane pliki
 * @param folderName Nazwa folderu
 *
 * Funkcja przechodzi do folderu podanego jako argument a następnie czyści go z plików "File" , "EncryptedKey", "EncryptedIV", "FileName". Które były tymczasowo przechowywane na serwerze w ramach przesyłania pliku między klientami
 * następnie usuwa podany folder.
 */
void ClearFolder(char* folderName);
/**
 * @brief Loguje użytkownika do serwera.
 * @param clientSocket Gniazdo logowanego użytkownika.
 * @param json Obiekt w formacie JSON przechowujący dane użytkownika którego chcemy zalogować.
 * @return Zwraca 0 jeżeli się udało 1 jeżeli wystąpił błąd.
 *
 * Funkcja kopiuje dane użytkownika do struktury UserStruct następnie wysyła wiadomość o zalogowaniu i czeka na potwierdzenie, jeżeli je otrzyma dodaje użytkownika do listy aktywnych
 * użytkowników i wywoluje funkcje SendRefreshUserList(), która wysyła odswieżenie listy użytkowników wszystkim aktywnym klientom.
 */
int LoginUser(int clientSocket,cJSON *json);
/**
 * @brief Zapisuje plik wysyłany przez użytkownika na serwerze.
 * @param klientGniazdo Gniazdo klienta.
 * @param json Obiekt w formacie JSON przechowujący zaszyfrowana nazwe pliku, zaszyfrowany klucz AES, wektor inicjujący AES oraz odbiorce.
 * @return Zwraca 0 jeżeli się udało 1 jeżeli wystąpił błąd.
 *
 * Funkcja wyszukuje odbiorce, oraz nadawce w liscie klientów i blokuje im możliwość odbierania innych powiadomień niż tych dotyczących przesyłania pliku
 * Następnie losowany jest numer folderu oraz jest on tworzony
 * Zapisywane są do folderu tymczasowego zaszyfrowane pliki zawierajace klucz AES oraz nazwe pliku
 * Wysyłane jest powiadomienie o gotowści na przyjęcie własciwego pliku
 * Serwer tworzy nowy plik i zapisuje do niego zaszyfrowaną zawartośc wysłaną przez klienta
 * Po odebraniu pliku wysyła powiadoeminie odbiorcy o dostępności pliku do pobrania
 */
int SaveFileOnServer(int klientGniazdo, const cJSON *json);
/**
 * @brief Wysyła plik odbiorcy zapisany na serwerze
 * @param klientGniazdo Gniazdo klienta.
 * @param json Obiekt w formacie JSON przechowujący nazwe folderu z którego odbiorac ma otrzymać plik.
 * @return Zwraca 0 jeżeli się udało 1 jeżeli wystąpił błąd.
 *
 * Funkcja przechodzi do podanego folderu i wczytuje do pamieci zaszyfrowany klucz AES, zaszyfrowany wektor inicjujący oraz zaszyfrowana nazwe pliku
 * Następnie wszystkie te informacje wklada do obietu typu JSON i wysyła do klienta czekając na potwierdzenie
 * Jeżeli serwer otrzymał potwierdzenie rozpoczyna czytanie pliku i przesylanie do klienta
 * Po poprawnym przesłaniu pliku czyści folder z jego zawartości
 */
int SendFileFromServer(int klientGniazdo, const cJSON *json);

/**
 * @brief Drukuje zawrtość listy użtkowników
 * Funkcja przechodzi przez liste z użytkownikami i drukuje jej zawartość na konsole
 */
void print_linked_list();
/**
 * @brief Dodaje użytkownika na koniec listy
 * @param newUser Struktura zawierająca dane nowego użytkownika
 */
void insert_at_last(struct UserStruct newUser);
/**
 * @brief Usuwa użytkownika
 * @param socket Gniazdo użytkownika usuwanego
 *
 * Funkcja przechodzi przez liste z użytkownikami i szuka użytkownika o zadanym gniedzie, nastepnie po znalezeniu usuwa go z listy
 */
void delete_item(int socket);
/**
 * @brief Wyszukuje użytkownika po modulo klucza publicznego
 * @param modulus Mod klucza głownego użytkownika
 * @return Węzeł zawierający strukture szukanego użytkownika
 *
 * Funkcja przechodzi przez liste z użytkownikami i szuka użytkownika o zadanym modulo klucza publicznego.
 */
node* search_item_by_mod(char modulus[]);
/**
 * Wyszukuje użytkownika po gniezdzie
 * @param socket GNiazdo użytkownika
 * @return Węzeł zawierający strukture szukanego użytkwonika
 *
 * Funkcja przechodzi przez liste z użytkownikami i szuka użytkownika o zadanym gniezdzie.
 */
node* search_item_by_socket(int socket);


/**
* @brief Głowna funkcja serwera zajmująca sie logowaniem użytkowników i przedzielaniu ich do nowo utworzonych wątków
* @param argc Ilość argumentów podanych w wywołaniu
* @param argv Tablica z parametrami wywołania
 */
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
            if(LoginUser(clientSocket, json) != 0)
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
            ClearClientThread(klientGniazdo);
        }

        cJSON *json = cJSON_Parse(JSONFileStruct);
        int requestCode = json->child->valueint;

        if(requestCode == 105)
        {
            if(SaveFileOnServer(klientGniazdo, json) == -1)
                perror("Nie udało się zapisać pliku na serwerze!\n");

            ClearClientThread(klientGniazdo);

        }
        else if(requestCode == 108)
        {
            if(SendFileFromServer(klientGniazdo, json) == -1)
                perror("Nie udało się wysłać pliku do klienta\n");

            ClearClientThread(klientGniazdo);
        }
        else if(requestCode == 109)
        {
            char* folderName = json->child->next->valuestring;

            if (chdir(folderName) != 0)
                perror("chdir() failed");

            ClearFolder(folderName);

            node *receiver = search_item_by_socket(klientGniazdo);
            receiver->user->Blocked = false;
        }
    }
}

int SendFileFromServer(int klientGniazdo, const cJSON *json) {
    char *folderName = json->child->next->valuestring;
    chdir(folderName);

    char EncryptedName[344];
    LoadFile("FileName", EncryptedName);
    char EncryptedKey[344];
    LoadFile("EncryptedKey", EncryptedKey);
    char EncryptedIV[344];
    LoadFile("EncryptedIV", EncryptedIV);

    char *fileInfo = CreateJSONFileInfo(110, EncryptedName, EncryptedKey, EncryptedIV);

    if (write(klientGniazdo, fileInfo, strlen(fileInfo)) < 0) {
        perror("write() - nie udalo się");
        return -1;
    }

    char requestCodeChar[3];
    int length; /* Odbierz komunikat od klienta */

    length = read(klientGniazdo, requestCodeChar, 3);

    if (length <= 0) {
        perror("read() -nie udalo się");
        return -1;
    }

    int requestCode = atoi(requestCodeChar);

    if (requestCode == 111) {
        char *bufptr;
        char buf[MAXBUFFOR];
        /* otworz plik do czytania */
        int fd = open("File", O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

        if (fd == -1) {
            fprintf(stderr, "Could not open file for reading!\n");
            return -1;
        }
        int readCounter = 0;
        int writeCounter = 0;

        /* czytaj plik i przesylaj do klienta */
        while ((readCounter = read(fd, buf, MAXBUFFOR)) > 0) {
            writeCounter = 0;
            bufptr = buf;
            while (writeCounter < readCounter) {

                readCounter -= writeCounter;
                bufptr += writeCounter;
                writeCounter = write(klientGniazdo, bufptr, readCounter);
                if (writeCounter == -1) {
                    fprintf(stderr, "Could not write file to client!\n");
                    break;
                }
            }
        }
        ClearFolder(folderName);
        node *receiver = search_item_by_socket(klientGniazdo);
        receiver->user->Blocked = false;
        return 0;
    } else {
        perror("Nie poprawny requestCode\n");
        return -1;
    }
}

int SaveFileOnServer(int klientGniazdo, const cJSON *json) {
    struct stat st = {0};
    char *EncryptedName = json->child->next->valuestring;
    char *EncryptedKey = json->child->next->next->valuestring;
    char *EncryptedIV = json->child->next->next->next->valuestring;
    char *Receiver = json->child->next->next->next->next->valuestring;

    node *sender = search_item_by_socket(klientGniazdo);
    sender->user->Blocked = true;

    node *receiver = search_item_by_mod(Receiver);
    receiver->user->Blocked = true;
    int receiverSocket = receiver->user->Socket;

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    int intFolderName = receiverSocket + tm.tm_mday + tm.tm_hour + tm.tm_min + GetRandomNumber();
    char folderName[12];
    sprintf(folderName, "%d", intFolderName);

    if (stat(folderName, &st) == -1) {
        if (mkdir(folderName, 0700) == -1) {
            printf("Nie udało się stworzyć folderu\n");
            return -1;
        }

        chdir(folderName);

        SaveFile("FileName", EncryptedName);
        SaveFile("EncryptedKey", EncryptedKey);
        SaveFile("EncryptedIV", EncryptedIV);

        //Wyslanie do klienta wiadomosci o gotowosci na przyjecie pliku
        if (write(klientGniazdo, "106", strlen("106")) < 0) {
            perror("write() - nie udalo się");
            return -1;
        }

        int fd = open("File", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

        if (fd == -1) {
            fprintf(stderr, "Could not open destination file.\n");
            return -1;
        }

        int readCounter = 0;
        char buf[MAXBUFFOR];
        int writeCounter;
        char *bufptr;

        while ((readCounter = read(klientGniazdo, buf, MAXBUFFOR)) > 0) {

            bufptr = buf;

            writeCounter = write(fd, bufptr, readCounter);

            if (writeCounter < 0) {
                printf("Blad przy zapisie do pliku!\n");
                return -1;
            }
        }
        close(fd);

        char *request = NULL;

        cJSON *requestJSON = cJSON_CreateObject();
        cJSON_AddNumberToObject(requestJSON, "requestCode", 107);
        cJSON_AddNumberToObject(requestJSON, "folderName", intFolderName);
        cJSON_AddStringToObject(requestJSON, "sender", sender->user->Username);

        request = cJSON_Print(requestJSON);
        if (request == NULL) {
            fprintf(stderr, "Failed to parse request.\n");
            return -1;
        }

        cJSON_Delete(requestJSON);

        //Wyslanie do adresata, wiadomosci o gotowosci pliku do sciagniecia
        if (write(receiverSocket, request, strlen(request)) < 0) {
            perror("write() - nie udalo się");
            return -1;
        }
        chdir("..");
        sender->user->Blocked = false;
        return 0;
    } else {
        perror("stat() - taki folder juz istnieje");
        return -1;
    }
}

int LoginUser(int clientSocket, cJSON *json)
{
    struct UserStruct newUser;
    char ConnectionBuffor[MAXBUFFOR];

    strcpy(newUser.Username, json->child->next->valuestring);
    strcpy(newUser.PublicKeyModulus, json->child->next->next->valuestring);
    strcpy(newUser.PublicKeyExponent, json->child->next->next->next->valuestring);
    newUser.Socket = clientSocket;
    newUser.Blocked = false;

    if (write(clientSocket, "101", strlen("101")) < 0) {
        perror("write() - nie udalo się wysłać potwierdzenia zalogowania");
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

void SendRefreshUserList()
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

void ClearClientThread(int klientGniazdo)
{
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