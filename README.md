# TCP-Server

Task
Create a server for automatic control of remote robots. Robots authenticate to the server and server directs them to target location in coordinate plane. For testing purposes each robot starts at the random coordinates and its target location is always located in the area inclusively between coordinates -2 and 2 at X- and Y-axes. Somewhere in the target area a secret message is placed, which robot should find, thus all positions of the target area should be searched. Server is able to navigate several robots at once and implements communication protocol without errors.

Detailed specification
Communication between server and robots is implemented via a pure textual protocol. Each command ends with a pair of special symbols "\a\b". Server must follow the communication protocol strictly, but should take into account imperfect robots firmware (see section Special situations).

Server messages:

Name	Message	Description
SERVER_CONFIRMATION	<16-bit number in decimal notation>\a\b	Message with confirmation code. Can contain maximally 5 digits and the termination sequence \a\b.
SERVER_MOVE	102 MOVE\a\b	Command to move one position forward
SERVER_TURN_LEFT	103 TURN LEFT\a\b	Command to turn left
SERVER_TURN_RIGHT	104 TURN RIGHT\a\b	Command to turn right
SERVER_PICK_UP	105 GET MESSAGE\a\b	Command to pick up the message
SERVER_LOGOUT	106 LOGOUT\a\b	Command to terminate the connection after successfull message discovery
SERVER_OK	200 OK\a\b	Positive acknowledgement
SERVER_LOGIN_FAILED	300 LOGIN FAILED\a\b	Autentication failed
SERVER_SYNTAX_ERROR	301 SYNTAX ERROR\a\b	Incorrect syntax of the message
SERVER_LOGIC_ERROR	302 LOGIC ERROR\a\b	Message sent in wrong situation


Client messages:

Name	Message	Description	Example	Maximal length
CLIENT_USERNAME	<user name>\a\b	Message with username. Username can be any sequence of characters except for the pair \a\b.	Oompa_Loompa\a\b	20
CLIENT_CONFIRMATION	<16-bit number in decimal notation>\a\b	Message with confirmation code. Can contain maximally 5 digits and the termination sequence \a\b.	1009\a\b	7
CLIENT_OK	OK <x> <y>\a\b	Confirmation of performed movement, where x and y are the robot coordinates after execution of move command.	OK -3 -1\a\b	12
CLIENT_RECHARGING	RECHARGING\a\b	Robot starts charging and stops to respond to messages.		12
CLIENT_FULL_POWER	FULL POWER\a\b	Robot has recharged and accepts commands again.		12
CLIENT_MESSAGE	<text>\a\b	Text of discovered secret message. Can contain any characters except for the termination sequence \a\b.	Haf!\a\b	150


Authentication
Server and client both know pair of authentication keys (these are not public and private keys):

Server key: 54621
Client key: 45328
Each robot starts communictation with sending its username. Username can be any sequence of characters not containing the termination sequence „\a\b“. Server computes hash code from the username:

Username: Meow!

ASCII representation: 77 101 111 119 33

Resulting hash: ((77 + 101 + 111 + 119 + 33) * 1000) % 65536 = 47784
Resulting hash is 16-bit number in decimal notation. The server then adds a server key to the hash, so that if 16-bit capacity is exceeded, the value simply "wraps around" (due to modulo operation):

(47784 + 54621) % 65536 = 36869
Resulting confirmation code of server is sent to client as text in SERVER_CONFIRM message. Client takes the received code and calculates hash back from it, then compares it with the expected hash value, which he has calculated from the username. If they are equal, client computes the confirmation code of client and sends it back to the server. Calculation of the client confirmation code is simular to the server one, only the client key is used:

(47784 + 45328) % 65536 = 27576

Client confirmation key is sent to the server in CLIENT_CONFIRMATION message, server calculates hash back from it and compares it with the original hash of the username. If values are equal, server sends message SERVER_OK, otherwise answers with message SERVER_LOGIN_FAILED and terminates the connection. The whole sequence of steps is represented at the following picture:

Client                  Server
​------------------------------------------
CLIENT_USERNAME     --->
                    <---    SERVER_CONFIRMATION
CLIENT_CONFIRMATION --->
                    <---    SERVER_OK
                              or
                            SERVER_LOGIN_FAILED
                      .
                      .
                      .
Server does not know usernames in advance. Robots can choose any name, but they have to know client and server keys. The key pair ensures two-sided autentication and prevents the autentication process from being compromised by simple eavesdropping of communication.

Movement of robot to the target area
Robot can move only straight (SERVER_MOVE), but is able to turn right (SERVER_TURN_RIGHT) or left (SERVER_TURN_LEFT). After each move command robot sends confirmation (CLIENT_OK), part of which is actual coordinates of robot. Be careful - robots are running already for some time, so they start to make errors. Sometimes it happens that they do not move forward. This situation needs to be detected and addressed properly. At the beginning of communication robot position is not known to server. Server must find out robot position and orientation (direction) only from robot answers. In order to prevent infinite wandering of robot in space, each robot has a limited number of movements (move forward and turn). The number of moves should be sufficient for a reasonable robot transfer to the target. Following is a demonstration of communication. The server first moves the robot twice to detect its current state and then guides it towards the target coordinates.

Client                  Server
​------------------------------------------
                  .
                  .
                  .
                <---    SERVER_MOVE
CLIENT_OK       --->
                <---    SERVER_MOVE
CLIENT_OK       --->
                <---    SERVER_MOVE
                          or
                        SERVER_TURN_LEFT
                          or
                        SERVER_TURN_RIGHT
                  .
                  .
                  .
This part of communication cannot be skipped, robot waits at least one of the movement commands - SERVER_MOVE, SERVER_TURN_LEFT or SERVER_TURN_RIGHT.

Caution! Robots sometimes make errors and are not able to move forward. Situation, that robot has not moved, needs to be detected and command to move should be sent again. During the rotation (turn commands) robots do not make errors.


Secret message discovery
After the robot reaches the target area (square with corner coordinates [2,2], [2,-2], [-2,2] and [-2,-2]), it starts to search the whole area, which means that robot attemps to pick up the secret message at all 25 positions of the target area (SERVER_PICK_UP). If robot receives command to pick up the message, but robot is not in the target area, an autodestruction of robot is initiated and communication with server is abrupted. If there is no secret message at the particular position, robot responds with empty message CLIENT_MESSAGE - „\a\b“. (It is guaranteed, that the secret message always contains non-empty text string.) Otherwise, robot sends server the text of the discovered secret message and server ends the connection with the message SERVER_LOGOUT. (It is guaranteed, that secret message never matches the message CLIENT_RECHARGING, so if the recharge message is obtained by the server after the pick up command, it always means that robot started to charge.) After that, client and server close the connection. Demo of the target area search:

Client                  Server
​------------------------------------------
                  .
                  .
                  .
                <---    SERVER_PICK_UP
CLIENT_MESSAGE  --->
                <---    SERVER_MOVE
CLIENT_OK       --->
                <---    SERVER_PICK_UP
CLIENT_MESSAGE  --->
                <---    SERVER_TURN_RIGHT
CLIENT_OK       --->
                <---    SERVER_MOVE
CLIENT_OK       --->
                <---    SERVER_PICK_UP
CLIENT_MESSAGE  --->
                <---    SERVER_LOGOUT


Recharging
Each robot has a limited power source. If it starts to run out of battery, he notifies the server and then recharges itself from the solar panel. It does not respond to any messages during the charging. When it finishes, it informs the server and continues there, where it left before recharging. If the robot does not stop charging in the time interval TIMEOUT_RECHARGING, the server terminates the connection.

Client                    Server
​------------------------------------------
CLIENT_USERNAME   --->
                  <---    SERVER_CONFIRMATION
CLIENT_RECHARGING --->

      ...

CLIENT_FULL_POWER --->
CLIENT_CONFIRMATION   --->
                  <---    SERVER_OK
                            or
                          SERVER_LOGIN_FAILED
                    .
                    .
                    .
Another example:

Client                  Server
​------------------------------------------
                    .
                    .
                    .
                  <---    SERVER_MOVE
CLIENT_CONFIRM    --->
CLIENT_RECHARGING --->

      ...

CLIENT_FULL_POWER --->
                <---    SERVER_MOVE
CLIENT_CONFIRM  --->
                  .
                  .
                  .


Error situations
Some robots can have corrupted firmware and thus communicate wrongly. Server should detect misbehavior and react correctly.

Error during authentication
Server responds to the wrong authentication with SERVER_LOGIN_FAILED message. This message is sent only after the server receives valid message CLIENT_USERNAME and CLIENT_CONFIRMATION and the received hash is not equal to the hash of username. (Valid == syntactically correct) In other situation server cannot send message SERVER_LOGIN_FAILED.

Syntax error
The server always reacts to the syntax error immediately after receiving the message in which it detected the error. The server sends the SERVER_SYNTAX_ERROR message to the robot and then terminates the connection as soon as possible. Syntactically incorrect messages:

Imcomming message is longer than number of characters defined for each message (including the termination sequence \a\b). Message length is defined in client messages table.
Imcomming message syntax does not correspond to any of messages CLIENT_USERNAME, CLIENT_CONFIRMATION, CLIENT_OK, CLIENT_RECHARGING and CLIENT_FULL_POWER.
Each incommimg message is tested for the maximal size and only messages CLIENT_CONFIRMATION, CLIENT_OK, CLIENT_RECHARGING and CLIENT_FULL_POWER are tested for their content (messages CLIENT_USERNAME and CLIENT_MESSAGE can contain anything).

Logic error
Logic error happens just during recharging, in two cases - when robot sends information about charging (CLIENT_RECHARGING) and then sends anything other than CLIENT_FULL_POWER. Or, if the robot sends CLIENT_FULL_POWER without previous CLIENT_RECHARGING message. Server reacts to these errors with SERVER_LOGIC_ERROR message and immediate termination of connection.

Timeout
Protokol for communication with robots contains two timeout types:

TIMEOUT - timeout for communication. If robot or server does not receive message from the other side for this time interval, they consider the connection to be lost and immediately terminate it.
TIMEOUT_RECHARGING - timeout for robot charging. After the server receives message CLIENT_RECHARGING, robot should at latest till this interval send message CLIENT_FULL_POWER. If robot does not manage it, server has to immediately terminate the connection.
Special situations
During the communication through some complicated network infrastructure two situations can take place:

Message can arrive divided into several parts, which are read from the socket one at a time. (This happens due to segmentation and possible delay of some segments on the way through the network.)
Message, sent shortly after another one, may arrive almost simultaneously with it. They could be read together with one reading from the socket. (This happens, when the server does not manage to read the first message from the buffer before the second message arrives.)
Using a direct connection between the server and the robots, combined with powerful hardware, these situations cannot occur naturally, so they are artificially created by the tester. In some tests, both situations are combined.

Every properly implemented server should be able to cope with this situation. Firmware of robots counts on this fact and even exploits it. If there are situations in the protocol where messages from the robot have a predetermined order, they are sent in that order at once. This allows robots to reduce their power consumption and simplifies protocol implementation (from their point of view).



Server optimization
Server optimize the protokol so it does not wait for the end of the message, which is obviously bad. For example, only a part of the username message is sent for authentication. Server for example receives 14 characters of the username, but still does not receive the termination sequence \a\b. Since the maximum username message length is 12 characters, it is clear that the message received cannot be valid. Therefore, the server does not wait for the rest of the message, but sends a message SERVER_SYNTAX_ERROR and terminates the connection. In principle, server should react in the same way when receiving a secret message.

In the part of communication where robot is navigated to the target coordinates, server expects three possible messages: CLIENT_OK, CLIENT_RECHARGING, or CLIENT_FULL_POWER. If server reads a part of the incomplete message and this part is longer than the maximum length of these messages, it sends SERVER_SYNTAX_ERROR and terminates the connection. For the help with optimization, the maximum length for each message is listed in the table.

Demo communication
C: "Oompa_Loompa\a\b"
S: "15045\a\b"
C: "5752\a\b"
S: "200 OK\a\b"
S: "102 MOVE\a\b"
C: "OK 0 1\a\b"
S: "102 MOVE\a\b"
C: "OK 0 2\a\b"
S: "103 TURN LEFT\a\b"
C: "OK 0 2\a\b"
S: "102 MOVE\a\b"
C: "OK -1 2\a\b"
S: "102 MOVE\a\b"
C: "OK -2 2\a\b"
S: "104 TURN RIGHT\a\b"
C: "OK -2 2\a\b"
S: "104 TURN RIGHT\a\b"
C: "OK -2 2\a\b"
S: "105 GET MESSAGE\a\b"
C: "Secret message.\a\b"
S: "106 LOGOUT\a\b"

Testing
Image of OS Tiny Core Linux is prepared for your server testing. It containts tester of the homework. Image is compatible with VirtualBox application.

Tester
Download and unzip the image. Then run the image in VirtualBox. After starting and booting shell is immediately ready to use. Tester is run by command tester:

tester <port number> <remote address> [test number(s)]
The first parameter is the port number on which your server will listen. The following is a parameter with the server remote address. If your server is running on the same computer as VirtualBox, use the default gateway address. The procedure is shown in the following figure:

testing image example
The output is quite long, so it is good to redirect it to the less command, in which it is easier to navigate.

If no test number is entered, all tests are run sequentially. Tests can also be run individually. The following example runs tests 2, 3, and 8:

tester 3999 10.0.2.2 2 3 8 | less




Overview of tests
Ideal situation
Test 1 sends valid data for authentication and its robot is located at the target coordinates after the first move and awaits picking up a secret message.

Check of authentication
Tests 2 to 4 verify that the server correctly checks for authentication errors. (Invalid confirmation code, special characters in username…​)

Checking the treatment of special situations
Tests 5 to 7 control the correct server response to special situations (segmentation and message merging).

Checking detection of syntax errors
Tests 8 to 14 verify the detection of syntax errors.

Checking communication timeout detection
Tests 15 and 16 verify that the server timeout works correctly and server terminates the connection properly.

Checking server optimization
Tests 17 through 21 check that the server is optimized correctly.

Check of robot navigation
Tests 20 to 24 check that the server can guide the robot to its destination. Attention! Robots can make mistakes and sometimes do not move forward.

Check of reaction to robot’s charging
Tests 25 to 28 check that the server is responding properly to the robot’s charging.

Check of parallel processing
Test 29 runs three test instances in parallel.

Testing by randomly generated situations
Test 30 generates valid but randomly generated communication. This test is used in the final test.

Final check
This test runs automatically after all previous tests have completed successfully. 3 instances of the test 30 run in parallel.




