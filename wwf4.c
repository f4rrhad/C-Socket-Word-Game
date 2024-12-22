#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <stdbool.h>
#include <ctype.h>

//Initialize all the functions at the top of the class
int initialization();
int gameloop(int client_socket);
void teardown();
bool isDone();
int displayWord(int client_socket);
int acceptInput(int client_socket, const char* buffer);
int getLetterDistribution(char *str, int *LetterCounts);
int compareCounts(int *counts1, int *counts2);
struct wordListNode* getRandomWord(int totalCount);
void findWords(char* masterWord);
int cleanupGameListNodes();
int cleanupWordListNodes();
void serveFile(int client_socket);


//defining the structure of a wordListNode
typedef struct wordListNode {
    char data[30];
    struct wordListNode *next;
} wordListNode;

//Global root and tail for wordListNode
wordListNode *root = NULL;
wordListNode *tail = NULL;

//defining the structure of a gameListNode
typedef struct gameListNode {
    char data[30];
    bool found;
    struct gameListNode *next;
} gameListNode;

//Global root for gameListNode
gameListNode* gameListRoot = NULL;
gameListNode* end = NULL;

struct wordListNode* masterWord = NULL;

typedef struct {
    bool gameInitialized;
    bool gameCompleted;
    struct wordListNode* masterWord;
} GameState;
GameState gameState = {false, false, NULL};
pthread_mutex_t game_mutex = PTHREAD_MUTEX_INITIALIZER;

//initialize's the random number distribution
int initialization()
{
    srand(time(NULL));
    FILE *fp;
    char str[1000];
    char *word;
    int count = 0;
    //Opens the file 2of12.txt
    fp = fopen("2of12.txt", "r");
    //First of all it check if we got the null value for fp then it will print a statement
    if (NULL == fp){
        printf("File can't be opened \n");
        return count;
    }
    else{
        //reading line by line till EOF
        while (fgets(str, sizeof(str), fp) != NULL){
        //splitting the line to words
            word = strtok(str, "\n");
            while (word != NULL){
                //allocating mem for newNode and checking if it was successfull
                wordListNode *newNode = malloc(sizeof(wordListNode));
                if(newNode == NULL){
                    printf("memory allocation failed");
                    cleanupWordListNodes();
                    fclose(fp);
                    exit(1);
                }
                if(strlen(word) < sizeof(newNode->data)){
                    //creating a newNode for each word and adding it to the list
                    strncpy(newNode->data, word, sizeof(newNode->data)-1);
                    newNode->data[sizeof(newNode->data)-1] = '\0';
                }else{
                    printf("Word is longer than 30 characters: %s\n",word);
                }
                newNode->next = NULL;
                
                if(root == NULL){
                    root = newNode;
                    tail = newNode;
                }
                else{
                    tail->next = newNode;
                    tail = tail->next;
                }
                //keeping a count of the words that we add to the linkedlist
                count++;
                word = strtok(NULL, "\n");
            }
        }
    }
    //closing the file and returing count
    fclose(fp);
    return count;
}

struct wordListNode* getRandomWord(int totalCount) {
    //getting the random number and total num of words from initialization
    int max = totalCount;
    int index = rand() % totalCount;
    int count = 0;
    struct wordListNode* current = root;
    //Walking the linkedlist to get ot nth entry
    while (current != NULL) {
        if(index == count){
            break;
        }
        current = current->next;
        count++;
    }
    //searching for a word thats more than 6 chars
    while(current != NULL && strlen(current->data) < 6){
        current = current->next;
    }
    //returing the word if found otherwise returning null if we reach end of dictionary without finding one
    if(current != NULL){
        return current;
    }else{
        return NULL;
    }
}


void findWords(char* masterWord)
{
    struct wordListNode* initial = root;
    
    int masterWordLetterCounts[26] = {0};
    int dictionaryWordCounts[26] = {0};
    
    getLetterDistribution(masterWord, masterWordLetterCounts);
    
    while (initial != NULL) {
        //calling the getLetterDistribution for each dictionary word
        getLetterDistribution(initial->data, dictionaryWordCounts);
        //comparing the counts for the dictonary word and the masterword if they match then the word is added to the game list
        
        if(compareCounts(masterWordLetterCounts, dictionaryWordCounts) == 1){
            //allocating memory for the new gameListNode
            gameListNode *newNode = malloc(sizeof(gameListNode));
            if(newNode == NULL){
                printf("memory allocation failed");
                cleanupGameListNodes();
                gameListRoot = NULL; // Reset the game list root
                end = NULL; // Reset the end pointer
                return;
            }
            //creating a newNode for each word and adding it to the list
            strncpy(newNode->data, initial->data, sizeof(newNode->data)-1);
            newNode->data[sizeof(newNode->data)-1] = '\0';
            //modified found to false
            newNode->found = false;
            newNode->next = NULL;
            //Adding newNode to gamelist
            if(gameListRoot == NULL){
                gameListRoot = newNode;
                end = newNode;
            }
            else{
                end->next = newNode;
                end = end->next;
            }
        }
        initial = initial->next;
    }
}

//template for the webpage
const char* HTML_TEMPLATE =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html\r\n"
"\r\n"
"<!DOCTYPE html>\n"
"<html lang='en'>\n"
"<head>\n"
"    <meta charset='UTF-8'>\n"
"    <meta name='viewport' content='width=device-width, initial-scale=1.0'>\n"
"    <title>Word Game</title>\n"
"    <style>\n"
"        body {\n"
"            background-color: #add8e6;\n" 
"            display: flex;\n"
"            justify-content: center;\n"
"            align-items: center;\n"
"            height: 100vh;\n"
"            margin: 0;\n"
"            font-family: Arial, sans-serif;\n"
"            text-align: center;\n"
"        }\n"
"        .content {\n"
"            padding: 20px;\n"
"            background: white;\n"
"            border-radius: 8px;\n"
"            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);\n"
"        }\n"
"    </style>\n"
"</head>\n"
"<body>\n"
"    <div class='content'>\n"
"        %s\n"
"    </div>\n"
"</body>\n"
"</html>\n";

int displayWord(int client_socket) {
    // Buffer for the generated HTML content
    char html[4096];
    memset(html, 0, sizeof(html));
    char* ptr = html;

    // Checking the current game state and generating content
    if (!gameState.gameInitialized) {
        ptr += sprintf(ptr, "<h2>No game in progress. Please start a new game.</h2>\n"
                            "<a href='/new-game'>Start New Game</a>\n");
    } else {
        if (gameState.masterWord) {
            // Show master word letters in sorted order
            int len = strlen(gameState.masterWord->data);
            char word[len + 1];
            strcpy(word, gameState.masterWord->data);

            // Sort the master word letters alphabetically
            for (int i = 0; i < len - 1; i++) {
                for (int j = i + 1; j < len; j++) {
                    if (word[i] > word[j]) {
                        char temp = word[i];
                        word[i] = word[j];
                        word[j] = temp;
                    }
                }
            }

            ptr += sprintf(ptr, "<div class='word'>Letters available: <strong>%s</strong></div>\n", word);

            // Display words found/remaining counters
            int foundCount = 0, totalCount = 0;
            struct gameListNode* countNode = gameListRoot;
            while (countNode != NULL) {
                if (countNode->found) foundCount++;
                totalCount++;
                countNode = countNode->next;
            }
            ptr += sprintf(ptr, "<div>Words found: %d/%d</div>\n", foundCount, totalCount);
        }

        // Display input form if the game isn't completed
        if (!gameState.gameCompleted) {
            ptr += sprintf(ptr,
                           "<form method='POST' action='/guess'>\n"
                           "    <input type='text' name='guess' placeholder='Enter your guess'>\n"
                           "    <input type='submit' value='Submit Guess'>\n"
                           "</form>\n");
        }

        // Found words
        ptr += sprintf(ptr, "<div style='margin-bottom: 20px;'><strong>Found words:</strong><br>\n");
        struct gameListNode* current = gameListRoot;
        while (current != NULL) {
            if (current->found) {
                ptr += sprintf(ptr, "<span style='margin-right: 10px;'>%s</span>\n", current->data);
            }
            current = current->next;
        }
        ptr += sprintf(ptr, "</div>\n");

        // Remaining words placeholders
        ptr += sprintf(ptr, "<div><strong>Remaining words:</strong><br>\n");
        current = gameListRoot;
        while (current != NULL) {
            if (!current->found) {
                ptr += sprintf(ptr, "<span style='margin-right: 10px;'>");
                for (int i = 0; i < strlen(current->data); i++) {
                    ptr += sprintf(ptr, "_ ");
                }
                ptr += sprintf(ptr, "</span>\n");
            }
            current = current->next;
        }
        ptr += sprintf(ptr, "</div>\n");

        ptr += sprintf(ptr, "</div>\n");

        // Congratulate the player if the game is completed
        if (gameState.gameCompleted) {
            ptr += sprintf(ptr,
                           "<!DOCTYPE html>\n"
                           "<html lang='en'>\n"
                           "<head>\n"
                           "<meta charset='UTF-8'>\n"
                           "<meta name='viewport' content='width=device-width, initial-scale=1.0'>\n"
                           "<title>Game Completed</title>\n"
                           "<style>\n"
                           "  body {\n"
                           "    background-color: #4CAF50;\n"
                           "    color: black;\n"
                           "    font-family: Arial, sans-serif;\n"
                           "    text-align: center;\n"
                           "    padding: 50px;\n"
                           "    margin: 0;\n"
                           "  }\n"
                           "  h2 {\n"
                           "    font-size: 2.5em;\n"
                           "    margin-bottom: 20px;\n"
                           "  }\n"
                           "</style>\n"
                           "</head>\n"
                           "<body>\n"
                           "<h2>Congratulations! 🎉</h2>\n"
                           "<p>Ready for another challenge?</p>\n"
                            "<a href='/new-game'>Start New Game</a>\n"
                            "</body>\n"
                            "</html>\n");
        }
    }

    //The full HTTP response
    char response[8192];
    snprintf(response, sizeof(response), HTML_TEMPLATE, html);

    // Sending the response to the client
    send(client_socket, response, strlen(response), 0);

    return 0;
}

//This function returns true to check the conditions for the loop
bool isDone()
{
    //The loops checks if any of the bool found values for the gameList Nodes are false then it returns false, it only returns true if all the bool found values are true.
    struct gameListNode* current = gameListRoot;
    while(current != NULL)
    {
        if(current->found == false)
        {
            return false;
        }
        else{
            current = current->next;
        }
    }
    return true;
}

// Main game loop with socket communication
int gameloop(int client_socket) {
    while (!gameState.gameCompleted) {
        // Display the current game state
        displayWord(client_socket);

        char buffer[1024] = {0};  // Create a buffer
        recv(client_socket, buffer, sizeof(buffer), 0);  // Populate buffer with client data

        if (acceptInput(client_socket, buffer) < 0) {
            fprintf(stderr, "Error handling user input.\n");
            break;
        }
    }
    return 0;
}

int cleanupGameListNodes()
{
    //freeing the nodes of gameListNode
    gameListNode *start = gameListRoot;
    while (start != NULL) {
        gameListNode *nextNode = start->next;
        free(start);
        start = nextNode;
    }
    return 0;
}

int cleanupWordListNodes()
{
    //freeing the nodes of wordListNode
    wordListNode *current = root;
    while (current != NULL) {
        wordListNode *nextNode = current->next;
        free(current);
        current = nextNode;
    }
    return 0;
}

int acceptInput(int client_socket, const char* buffer) {
    char guess[30] = {0};
    bool valid_input = false;

    // Extracting the guess from POST request body
    const char* content_start = strstr(buffer, "\r\n\r\n");
    if (content_start) {
        content_start += 4; // Move past \r\n\r\n
        char* param_start = strstr(content_start, "guess=");
        
        if (param_start) {
            param_start += 6; // Move past "guess="
            
            // Find end of parameter (next & or end of string)
            char* param_end = strstr(param_start, "&");
            if (param_end == NULL) {
                param_end = param_start + strlen(param_start);
            }

            // Copy guess, ensuring we don't overflow
            size_t guess_length = param_end - param_start;
            if (guess_length > 0 && guess_length < sizeof(guess)) {
                strncpy(guess, param_start, guess_length);
                guess[guess_length] = '\0';
                
                // Remove any URL encoding
                for (char* p = guess; *p; p++) {
                    if (*p == '+') *p = ' ';
                }

                // Trim whitespace
                char* start = guess;
                char* end = guess + strlen(guess) - 1;
                while (start <= end && isspace(*start)) start++;
                while (end > start && isspace(*end)) end--;
                *(end + 1) = '\0';
                memmove(guess, start, end - start + 2);

                valid_input = true;
            }
        }
    }

    // If no guess found in body, trying URL parameters
    if (!valid_input) {
        const char* guessStart = strstr(buffer, "guess=");
        if (guessStart) {
            sscanf(guessStart + 6, "%29[^&\n ]", guess);
            valid_input = true;
        }
    }

    // Input validation
    if (!valid_input || strlen(guess) == 0) {
        fprintf(stderr, "Invalid or empty guess\n");
        return -1;
    }
    
    // to show the guess
    fprintf(stderr, "Processed guess: '%s'\n", guess);

    // Update game state based on the guess
    pthread_mutex_lock(&game_mutex);

    struct gameListNode* current = gameListRoot;
    bool wordFound = false;

    while (current != NULL) {
        char word[30];
        strncpy(word, current->data, sizeof(word));
        word[sizeof(word) - 1] = '\0';

        // Convert word to lowercase for case-insensitive matching
        for (int i = 0; word[i]; i++) {
            word[i] = tolower(word[i]);
        }

        // Sanitize trailing whitespace/newlines
        size_t len = strlen(word);
        while (len > 0 && (word[len - 1] == '\n' || word[len - 1] == '\r' || word[len - 1] == ' ')) {
            word[--len] = '\0';
        }

        // Single letter or full word match
        if ((strlen(guess) == 1 && len == 1 && word[0] == guess[0] && !current->found) ||
            (strcmp(word, guess) == 0 && !current->found)) {
            
            current->found = true;
            fprintf(stderr, "Matched: %s\n", current->data);
            wordFound = true;
            break;
        }

        current = current->next;
    }

    if (!wordFound) {
        fprintf(stderr, "No matching word found for guess: %s\n", guess);
    }

    // Check if the game is completed
    gameState.gameCompleted = isDone();
    
    pthread_mutex_unlock(&game_mutex);
    
    // Display the updated game state
    displayWord(client_socket);

    return 0;
}

//this function takes in a string and letterCounts and updates the lettercounts for each distribution
int getLetterDistribution(char *str, int *LetterCounts)
{
    //a loop that sets all letter counts to 0
    for(int j = 0; j < 26; j++)
    {
        LetterCounts[j] = 0;
    }
    //a loop that finds the correct index for each character.
    for(int i = 0; i < strlen(str) ; i++)
    {
        //str[i]-'A' subtracts the ASCII value of the char from the ASCII of A and that value is set as the
        //index where letterCount will incremented
        //Making sure that the str[i] is between a and z
        if(str[i] >= 'a' && str[i] <= 'z'){
            int index = toupper(str[i]) - 'A';
            // Incrementing the count of the letter
            LetterCounts[index]++;
        }
    }
    return 0;
}

//this function takes 2 letterCounts and compares them to check if they are equal or not
int compareCounts(int *counts1, int *counts2)
{
    int is_equal = 1;
    //this loops 26 times and compares the individual letter counts for both distributions and breaks if any individual letter count is not equal
    //the goal of this loop is to ensure that each count in letter counts is equal if not then it will break
    for(int i = 0; i < 26; i++)
    {
        if(counts1[i] < counts2[i])
        {
            is_equal = 0;
            break;
        }
    }
    return is_equal;
}

char *base_dir = NULL;

// Modify handle_client to support both game and file serving
void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);

    char buffer[1024] = {0};
    ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received <= 0) {
        fprintf(stderr, "Error receiving client request\n");
        close(client_socket);
        return NULL;
    }

    char method[10], path[256];
    sscanf(buffer, "%s %s", method, path);

    // Game initialization and handling
    if (strcmp(path, "/new-game") == 0 || strcmp(path, "/words") == 0) {
        pthread_mutex_lock(&game_mutex);
        teardown();
        displayWord(client_socket);
        pthread_mutex_unlock(&game_mutex);
    }
    // Guess handling
    else if (strncmp(path, "/guess", 6) == 0 && strcmp(method, "POST") == 0) {
        acceptInput(client_socket, buffer);
    }
    // File serving logic
    else if (strcmp(method, "GET") == 0) {
        char fullPath[1000];
        //adding the base dir to the filename from GET to get the full path
        strcpy(fullPath,base_dir);
        strcat(fullPath, path);
        printf("Path: %s\n", fullPath);
        struct stat fileStat;
        int length = stat(fullPath, &fileStat);
        
        int fd;
        fd = open(fullPath, O_RDONLY);
        
        //opening the file if not found then I send a 404 error
        if(fd == -1)
        {
            char response[] = "HTTP/1.1 404 Not Found\r\n";
            send(client_socket, response, strlen(response), 0);
        }else{
        //if the file is found then I send a 200 Ok with the length of the contents and the contents of the file.
            char response_header[1000];
            snprintf(response_header, sizeof(response_header),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Length: %lld\r\n\r\n", (long long)fileStat.st_size);
            send(client_socket, response_header, strlen(response_header), 0);
            
            // sending the file contents
            char buffer[1024];
            ssize_t bytes_read;
            while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
                send(client_socket, buffer, bytes_read, 0);
            }
        }
        close (fd);
    }
    else {
        // Invalid method response
        const char *response =
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n\r\n"
            "Invalid request.\n";
        send(client_socket, response, strlen(response), 0);
    }

    close(client_socket);
    return NULL;
}

void teardown() {
    // Clean up existing game list and word list
    cleanupGameListNodes();
    gameListRoot = NULL;
    end = NULL;

    // Reset word list
    cleanupWordListNodes();
    root = NULL;
    tail = NULL;

    // Reset game state
    gameState.gameInitialized = false;
    gameState.gameCompleted = false;
    gameState.masterWord = NULL;

    // Reinitialize word list and game
    int totalCount = initialization(); // Load words from dictionary
    gameState.masterWord = getRandomWord(totalCount);
    
    if (gameState.masterWord != NULL) {
        // Find valid words that can be made from master word
        findWords(gameState.masterWord->data);
        gameState.gameInitialized = true;
        
        // Debug print
        fprintf(stderr, "New game started. Master word: %s\n", gameState.masterWord->data);
    } else {
        fprintf(stderr, "Failed to select a master word\n");
    }
}

int main(int argc, char *argv[]) {
    //checking if the base directory is passed in as an argument
    if (argc != 2) {
        fprintf(stderr, "Usage: %s\n", argv[0]);
        exit(1);
    }
    
    base_dir = argv[1];
    
    printf("Base directory: %s\n", base_dir);

    char port[10];
    char address[100];
    struct addrinfo hints, *res, *p;
    int sockfd;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;  // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP only
    hints.ai_flags = AI_PASSIVE; // Fill in my IP for me
    
    //setting server port to 8000
    strcpy(port, "8000");

    // Get address info for server binding
    if (getaddrinfo(NULL, port, &hints, &res) != 0) {
        perror("getaddrinfo");
        return 1;
    }
    
    //creating a socket
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(sockfd == -1)
    {
        perror("socket");
        freeaddrinfo(res);
        return 1;
    };
    
    // Binding the socket
    if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("bind");
        close(sockfd);
        freeaddrinfo(res);
        return 1;
    }
    
    //getting the IP address and the port from the server
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    getsockname(sockfd, (struct sockaddr *)&addr, &addr_len);
    
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str));
    printf("Server listening on IP: %s, Port: %s\n", ip_str, port);
    
    //listening for connections
    if(listen(sockfd, 5) < 0)
    {
        perror("listen");
        close(sockfd);
        freeaddrinfo(res);
        return 1;
    }
    
    //loop for accepting client connections
    char buffer[100];
    
    while (1) {
        struct sockaddr_storage client_addr;
        socklen_t addr_size = sizeof(client_addr);
        int *new_sock = malloc(sizeof(int));
        
        *new_sock = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size);
        if (*new_sock < 0) {
            perror("accept failed");
            free(new_sock);
            continue;
        }
        pthread_t tid;
        int thread_create_result = pthread_create(&tid, NULL, handle_client, new_sock);
        if (thread_create_result != 0) {
            perror("pthread_create failed");
            close(*new_sock);
            free(new_sock);
        } else {
            // Detach thread to prevent resource leaks
            pthread_detach(tid);
        }
    }
    //freeing addrinfo and closing the socket
    freeaddrinfo(res);
    close(sockfd);
    return 0;
}
