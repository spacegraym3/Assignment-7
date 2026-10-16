# a7 - Connect Four

In this week’s assignment you will build a networked “Connect4” application. 

Your networked application will mean two users in different locations can play a Connect4 game together! Users will be able to make moves on the board by sending a request to the server.

Our game server will also be able to crash and recover games by using persistent storage (aka the disk & filesystem). 

You will have to use the POSIX networking API to create a server and client program using sockets. You will also leverage the POSIX file API to open, read, and write files on disk for the purpose of saving and loading an in-progress game. 

As this is the last C project of cx651, you will get to make more design choices yourself! Carefully plan how you will implement each feature and the data structures you will need.

# Step 1: Implementing the Game Logic

The game of connect four is simple. Players take turns placing pieces in a chosen column. When a column is chosen, the piece drops down to the bottom of that column, possibly stacking onto earlier placed pieces.

The goal of the game is to "connect four" of these pieces either vertically, horizontally, or diagonally.

In this next section you will implement the functionality of three key helper methods.

```c
/*
Takes as input a pointer to a connect4 game board, a player (0/1), and a int column.
Places a new piece in `col` on the game board.
*/
int make_move(struct game* g, int player, int col) {
    
}
 
/*
Takes as input a pointer to a connect4 game board.
Returns 0 or 1 if player 0 or 1 has won respectively.
Returns -1 if there is no winner.
*/
int check_win(struct game* g) {
   
}
 
/*
Initializes an existing game with an empty board.
*/
void new_game(struct game* g, int gameId) {
   
}
```


## Step 1a: Make the Board

It is up to you how you want to represent your game board. For inspiration, consider how we represent images in a3. 
A standard board for connect4 has 7 columns and 6 rows. 

> [!IMPORTANT]
> - Task: Define a C struct for how a board should be represented called `struct board`. Implement this in `common.h`

> [!IMPORTANT]
> - Task: Implement the logic that creates a new game in `game.c`.

## Step 1b: Placing Pieces

We now will implement the logic of making a move on our game board.

Consider an example of how the board changes when a `O` is placed in column 3.

```
 0 1 2 3 4 5 6
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|X|.|.|.|.|
|.|X|O|O|X|.|.|
```
This move results in the `O` landing "on top" of the existing `O` in column 3.

```
 0 1 2 3 4 5 6
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|X|O|.|.|.|
|.|X|O|O|X|.|.|
```

Making a move should also update bookkeeping on whose turn it currently is.

> [!IMPORTANT]
> - Task: Implement the logic that allows a player to place a piece in a given chosen column in `game.c`.


## Step 1c: Win Detection

A player wins a game of connect4 by connecting four pieces horizontally, vertically, or diagonally.

> [!IMPORTANT]
> - Task: Implement the logic that checks if either player has won the game.

# Step 2: Adding Networking

This project will involve at least three processes running simultaneously:
- A server
- Two clients

As you start to structure your server, eventually you will want to support more games in progress. For example, four clients should be able to use your server to play 2 separate games that do not interfere with each other.

You will use the functions `recvfrom` to receive messages and `sendto` to send messages. The messages can send any bytes you wish to implement the correct functionality.

The client and server share the UDP packet definitions in `common.h`. Requests use
`struct client_request`, with each integer field in network byte order. Responses
use `struct server_response`; its integer fields and flattened board cells are
also in network byte order. Board cells are ordered row by row. The server should
send `SERVER_WAITING` while a game has only one player, `SERVER_ACTIVE` when
both players can play, `SERVER_FINISHED` with the winner set when a player wins,
`SERVER_TIE` when the board fills, or `SERVER_ERROR` with an error message for
an invalid request. Send an updated game state to both clients after each valid
move.

## Step 2a: Server Setup

In the provided `server.c` code there is a skeleton which sets up the socket and port.

You will need to implement the server's logic loop that continually:
- `recvfrom` incoming clients on the open port.
- `sendto` any clients who should hear about this update.
  - for example, when your opponent makes a move, both players should receive the resulting move and print the board out.


## Step 2b: Client Setup

On starting a game, the client is prompted:
```
Connect 4
  1) Create a new game
  2) Join a game
```

**To set up the game:**
1. The first client will input `1` indicating they are creating a new game.
  - The server will respond with the `gameId` which starts at 0 and increases more games are created.
2. The second client will input `2` and then be prompted for which gameId they are joining.
  - If they type `0` for example, they will join the first created game on the server as the second player.
  - This should fail, if they try to join a game that does not exist yet.

After both players have joined, they can now send moves and play the game!

## Step 2c: Client Moves

The client interface will be a CLI interface where clients are allowed to type in their moves. These moves will be transmitted to the server. Then, the client will display the resulting board and wait for a message from the server about their opponents move.

The flow of a turn is:
1. The client is prompted for an integer move.
2. They type in the move.
3. A message is sent to the server with the move.
4. The server responds back with the board post-move.
  - If the player has won, print a `YOU WIN!` message and exit.
5. The client waits for a message about their opponent's move.
6. After the opponent moves, the server responds back with a message about their opponents move.
    - If the opponent has won, print a `YOU LOSE!` message and exit.

The above 6 steps loop until a player has won, or the board is full. If the board fills, exit with `TIE` status.

You can optionally implement the logic to handle illegal moves, however, this is not tested in the testing suite.

For example, consider the game between two players:

Client 1 will see:
```
Connect 4
  1) Create a new game
  2) Join a game
> 1
Current Game
---------------------
GameID: 0

 0 1 2 3 4 5 6
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|

waiting for other player...

Current Game
---------------------
GameID: 0

 0 1 2 3 4 5 6
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|


Input Move Row: > 2
Current Game
---------------------
GameID: 0

 0 1 2 3 4 5 6
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|X|.|.|.|.|

waiting for other player...

Current Game
---------------------
GameID: 0

 0 1 2 3 4 5 6
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|X|O|.|.|.|


Input Move Row: > 2
Current Game
---------------------
GameID: 0

 0 1 2 3 4 5 6
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|X|.|.|.|.|
|.|.|X|O|.|.|.|

waiting for other player...

Current Game
---------------------
GameID: 0

 0 1 2 3 4 5 6
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|X|O|.|.|.|
|.|.|X|O|.|.|.|


Input Move Row: > 2
Current Game
---------------------
GameID: 0

 0 1 2 3 4 5 6
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|X|.|.|.|.|
|.|.|X|O|.|.|.|
|.|.|X|O|.|.|.|

waiting for other player...

Current Game
---------------------
GameID: 0

 0 1 2 3 4 5 6
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|X|O|.|.|.|
|.|.|X|O|.|.|.|
|.|.|X|O|.|.|.|


Input Move Row: > 2
Current Game
---------------------
GameID: 0

 0 1 2 3 4 5 6
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|X|.|.|.|.|
|.|.|X|O|.|.|.|
|.|.|X|O|.|.|.|
|.|.|X|O|.|.|.|

YOU WIN!

```

Client 2 will see:
```
Connect 4
  1) Create a new game
  2) Join a game
> 2
Game ID? >0
Current Game
---------------------
GameID: 0

 0 1 2 3 4 5 6
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|

waiting for other player...

Current Game
---------------------
GameID: 0

 0 1 2 3 4 5 6
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|X|.|.|.|.|


Input Move Row: > 3
Current Game
---------------------
GameID: 0

 0 1 2 3 4 5 6
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|X|O|.|.|.|

waiting for other player...

Current Game
---------------------
GameID: 0

 0 1 2 3 4 5 6
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|X|.|.|.|.|
|.|.|X|O|.|.|.|


Input Move Row: > 3
Current Game
---------------------
GameID: 0

 0 1 2 3 4 5 6
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|X|O|.|.|.|
|.|.|X|O|.|.|.|

waiting for other player...

Current Game
---------------------
GameID: 0

 0 1 2 3 4 5 6
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|X|.|.|.|.|
|.|.|X|O|.|.|.|
|.|.|X|O|.|.|.|


Input Move Row: > 3
Current Game
---------------------
GameID: 0

 0 1 2 3 4 5 6
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|X|O|.|.|.|
|.|.|X|O|.|.|.|
|.|.|X|O|.|.|.|

waiting for other player...

Current Game
---------------------
GameID: 0

 0 1 2 3 4 5 6
|.|.|.|.|.|.|.|
|.|.|.|.|.|.|.|
|.|.|X|.|.|.|.|
|.|.|X|O|.|.|.|
|.|.|X|O|.|.|.|
|.|.|X|O|.|.|.|

YOU LOSE!

```

**NOTE: your code must implement this exact printing format!**


> [!IMPORTANT]
> - Task: Implement `client.c` and `server.c` such that two players can play a Connect4 game with GameID `0`.

You can test your code by opening three terminal panes. You may want to use `tmux` to do this.

In one window run `./server` and in the other two run `./client`.


## Step 2d: Unreliable Delivery

It is OK if your implementation can not handle dropped or lost messages. In fact, the networking protocol this code uses, UDP, is unreliable!

The `test.sh` test suite does not try to force message drops and since we are testing locally, it is unlikely to occur. 

> [!IMPORTANT]
> - Task: Based on your implementation, what happens if a message from client to server is dropped? What happens if a message from server to client is dropped? 
> - Describe what behavior you would expect to see in `questions.txt`. Label your answer `(1)`.

# Step 3: Testing

At this point running `./test.sh` should pass `t1,t2,t3`.

These are end-to-end integration tests.

The way the tests work is by piping input from `reference/t1/client1.in` and `reference/t1/client2.in` into `./client`. 

Then, the test verifies that the output EXACTLY matches `reference/t1/client1.out` and `reference/t1/client2.out`.

If you fail a test, check your produced `client1.out` and `client2.out` and see why they differ with the reference file.

You can use the shell `diff` command to find where they differ: 
```shell
diff reference/t1/client1.out client1.out
```

A summary of each test is below:
- `t1` tests if client1 wins by placing 4 Xs in column 2
- `t2` tests if client1 can win diagonally
- `t3` tests if client2 wins by placing 4 Os in column 3
- `t4` tests if four clients can play two games correctly
- `t5-crash` tests if two clients can play a game, have the server crash, and resume their game after the server comes back online.

> [!IMPORTANT]
> - Task: Verify your code passes the expected tests when running `./test.sh`

# Step 4: Multiple Ongoing Games

> [!IMPORTANT]
> - Task: Modify your code such that two ongoing games can occur simultaneously. Assigned GameIds should increases monotonically starting from 0. 
> - Task: Verify your code passes the `t4` when running `./test.sh`

# Step 5: Saving and Loading Games

We want to make our server **fault tolerant** by saving game data to disk. After a move is made, save the game to a file. 

When our server starts up, if the server is started in recovery mode, check for any the save game file and load them into memory.

To start the server in recovery mode, populate the `argv[1]` field as follows:
```shell
./server 1
```

Without argv[1], start the server without recovery mode.

## Step 5a: Saving a Game to File

After every move made, we should update the saved version of the game's file.
The server stores each game's save as `game_<id>.save` in its working directory.

> [!IMPORTANT]
> Task: Implement the logic in `server.c` and `save.c` that saves an in-memory game struct to a game file after every move.

## Step 5b: Loading a Game from a File

When your server starts up in `recovery mode`, in main, the first thing we should do is call `loadgame`, which loads a specified file path into the server's memory. 

You are free to name the disk file as you please and format the file as you wish.

> [!IMPORTANT]
> Task: Implement the logic in `server.c` and `save.c` that loads the save game file to an in-memory game struct.
> - Task: Verify your code passes the `t5` when running `./test.sh`

## Step 5c: Lack of Robustness

We have implemented a very bare-bones system for fault tolerance. Let's now consider where our savegame and loadgame functionality falls short.

> [!IMPORTANT]
> - Task: In what scenarios would our approach from savegame and loadgame not function as expected? Analyze and short-comings or limitations of the save/load functionality in `questions.txt` and label your answer `(2)`.


# Step 6: Submitting on Gradescope

To submit on Gradescope, submit all the files in this directory to the assignment upload.

You do not need to upload the `reference/` subdirectory.

**DO NOT upload a zip.** Use Shift to select all the files in your assignment directory instead.

Note: To download files from google colab, navigate to the `Assignment-7` directory that should be saved in your **Google Drive**. (Assuming you did all your work in `/content/drive/MyDrive/Assignment-7`). Clicking the three vertical dots shows a "download" option that will download all files to your local computer for upload to gradescope.

You should see the autograder run and report a score. Ensure that you are happy with this score! Feel free to resubmit as many times as you wish before the deadline.