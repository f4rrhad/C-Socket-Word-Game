# C-Socket-Word-Game
C-Socket Word Game is a networked word game built in C using socket programming. It features real-time gameplay, concurrent client support via threads, and robust communication between server and clients.


Features:
	•	Multiplayer Support: Allows multiple clients to connect to the server and participate simultaneously.
	•	Dynamic Gameplay: A game loop that manages word display, client input, and game progression.
	•	Real-Time Communication: Uses TCP sockets for reliable communication between server and clients.
	•	Threaded Architecture: Utilizes POSIX threads (pthreads) to handle multiple client connections concurrently.
	•	Input Validation: Ensures correct and efficient processing of user inputs.
	•	Robust Design:
	•	Initialization and teardown routines for seamless setup and cleanup.
	•	File and state management for game data persistence and logging.

Technologies Used:
	•	C Programming Language
	•	POSIX Threads (pthreads)
	•	Socket Programming (<sys/socket.h>, <netdb.h>)
	•	File Handling (<fcntl.h>, <sys/stat.h>)
	•	Networking Utilities (<arpa/inet.h>)

How to Run:
1.	Clone this repository:

git clone <repository-url>
cd <repository-name>


	2.	Compile the program:

gcc -o main wwf4.c -lpthread

	3.	Start the server:

./main



Future Improvements:
	•	Enhanced gameplay mechanics with additional word challenges.
	•	User authentication and leaderboard integration.
	•	Improved error handling and input sanitization.

