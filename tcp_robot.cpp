#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <iostream>
#include <vector>
#include <thread>


#define MOD (65536)

#define SERVER_KEY 54621
#define CLIENT_KEY 45328


#define SERVER_MOVE             "102 MOVE\a\b"
#define SERVER_TURN_LEFT        "103 TURN LEFT\a\b"
#define SERVER_TURN_RIGHT       "104 TURN RIGHT\a\b"
#define SERVER_PICK_UP          "105 GET MESSAGE\a\b"
#define SERVER_LOGOUT           "106 LOGOUT\a\b"
#define SERVER_OK               "200 OK\a\b"
#define SERVER_LOGIN_FAILED     "300 LOGIN FAILED\a\b"
#define SERVER_SYNTAX_ERROR     "301 SYNTAX ERROR\a\b"
#define SERVER_LOGIC_ERROR      "302 LOGIC ERROR\a\b"



#define BUFFER_SIZE 200

#define PORT 8080


typedef enum: int {
    MODE_CONFIRM_SERVER = 0,
    MODE_CONFIRM_CLIENT,
    MODE_FIND_POS,
    MODE_GO_TO_TARGET_POS,
    MODE_PICK_UP,
    MODE_TERMINATE,
    MODE_RECHARGING,
    MODE_NONE = -1
} MODE;

typedef int DIR;


struct Position {
    int _x, _y;
};

struct Robot {
    Position _p;
    DIR _d;
};

using namespace std;


int findMessage(string& message, string& buffer, int& bufferPos, int bufferSize) {
    if (message.size() != 0 && message[message.size() - 1] == '\a' && buffer[bufferPos] == '\b') {
        message.resize(message.size() - 1); // delete "\a"
        bufferPos++;
        return 1;
    }
    size_t pos = buffer.find("\a\b", bufferPos);
    if (pos == string::npos || pos + 1 >= (size_t)bufferSize) {
        message += buffer.substr(bufferPos, bufferSize - bufferPos);
        bufferPos = bufferSize;
        return 0;
    }
    message += buffer.substr(bufferPos, pos - bufferPos);
    bufferPos = pos + 2;

    return 1;
}

uint confirm(string& message, int cliSocket) {
    uint hash = 0;
    for (size_t i = 0; i < message.size(); ++i) {
        hash += message[i];
        hash %= MOD;
    }

    hash *= 1000;
    hash %= MOD;
    string hashStr = to_string((hash + SERVER_KEY) % MOD) + "\a\b";

    send(cliSocket, hashStr.data(), hashStr.size() * sizeof(char), 0);

    return (hash + CLIENT_KEY) % MOD;
}

int parseOK(string& message, Position& dst) {
    if (sscanf(message.c_str(), "OK %d %d", &dst._x, &dst._y) != 2 || message.size() > 10) {
        return 0;
    }
    size_t i = 2;
    while (message[i] == ' ') { i++; }
    if (message[i] == '-') i++;
    while (message[i] >= '0' && message[i] <= '9') { i++; }
    while (message[i] == ' ') { i++; }
    if (message[i] == '-') i++;
    while (i < message.size() && message[i] >= '0' && message[i] <= '9') { i++; }
    if (i != message.size()) {

        return 0;
    }

    return 1;
}

int countLeftTurns(DIR a, DIR b) {
    return (a - b + 4) % 4;
}

int countRightTurns(DIR a, DIR b) {
    return (b - a + 4) % 4;
}

int turn(DIR targetDir, DIR& currentDir, int cliSocket) {
    if (targetDir == currentDir) {
        return 0;
    }

    if (countLeftTurns(currentDir, targetDir) < countRightTurns(currentDir, targetDir)) {
        send(cliSocket, SERVER_TURN_LEFT, sizeof(SERVER_TURN_LEFT) / sizeof(char) - 1, 0);
        currentDir += 3;
        currentDir %= 4;
    }
    else {
        send(cliSocket, SERVER_TURN_RIGHT, sizeof(SERVER_TURN_RIGHT) / sizeof(char) - 1, 0);
        currentDir += 1;
        currentDir %= 4;
    }

    return 1;
}

int goToTargetPos(Position& targetPos, Robot& r, int cliSocket) {
    if (r._p._x == targetPos._x && r._p._y == targetPos._y) {
        return 0;
    }

    if (r._p._x != targetPos._x) {
        if (!turn(2 * (int)(r._p._x > targetPos._x), r._d, cliSocket)) {
            send(cliSocket, SERVER_MOVE, sizeof(SERVER_MOVE) / sizeof(char) - 1, 0);
        }
    }
    else {
        if (!turn(2 * (int)(r._p._y < targetPos._y) + 1, r._d, cliSocket)) {
            send(cliSocket, SERVER_MOVE, sizeof(SERVER_MOVE) / sizeof(char) - 1, 0);
        }
    }

    return 1;
}

void nextTargetPos(Position& targetPos) {
    if (targetPos._y % 2 == 0) {
        if (targetPos._x != 2) {
            targetPos._x++;
        }
        else {
            targetPos._y--;
        }
    }
    else {
        if (targetPos._x != -2) {
            targetPos._x--;
        }
        else {
            targetPos._y--;
        }
    }
}

int serverOptimization(string& message, MODE mode) {

    switch (mode) {
    case MODE_CONFIRM_SERVER: {
        if (message.size() <= 18
            || (message.size() == 19 && message[message.size() - 1] == '\a'))
            \


            return 1;
        else
            return 0;

        break;
    }
    case MODE_CONFIRM_CLIENT: {
        if (string("RECHARGING\a").find(message) == 0) {
            return 1;
        }

        size_t countDec = 0;
        while (countDec < 5 && countDec < message.size()
            && message[countDec] >= '0' && message[countDec] <= '9') {
            countDec++;
        }

        if ((countDec + 1 == message.size() && message[countDec] == '\a')
            || countDec == message.size()) {
            return 1;
        }

        return 0;

        break;
    }
    case MODE_FIND_POS: {
        if (string("RECHARGING\a").find(message) == 0) {
            return 1;
        }

        if (message.size() > 11
            || (message.size() == 11 && message[10] != '\a')) {
            return 0;
        }
        return 1;

        break;
    }
    case MODE_GO_TO_TARGET_POS: {
        if (string("RECHARGING\a").find(message) == 0) {
            return 1;
        }
        if (message.size() > 11
            || (message.size() == 11 && message[10] != '\a')) {
            return 0;
        }
        return 1;

        break;
    }
    case MODE_PICK_UP: {
        if (message.size() > 149
            || (message.size() == 149 && message[148] != '\a')) {
            return 0;
        }
        return 1;
        break;
    }

    case MODE_RECHARGING: {
        if (string("FULL POWER\a").find(message) == 0) {
            return 1;
        }
        return -1;

        break;
    }

    default:
        return 0;
        break;
    }
}

int runCommunication(int cliSocket) {
    string message;
    string buffer;
    buffer.resize(BUFFER_SIZE);
    int bufferPos = 0;
    int recvSize = 0;
    int messageFlag = 0;
    uint clientHash = -1;
    int findPosFlag = 0;

    struct timeval waitTime;
    waitTime.tv_sec = 1;
    waitTime.tv_usec = 0;

    fd_set fdSet;

    Robot r;
    r._d = -1;
    Position targetPos = { -2, 2 };

    MODE mode = MODE_CONFIRM_SERVER;
    MODE lastMode = MODE_NONE;

    while (1) {
        if (bufferPos >= recvSize) {
            FD_ZERO(&fdSet);
            FD_SET(cliSocket, &fdSet);
            if (!select(cliSocket + 1, &fdSet, NULL, NULL, &waitTime)) {
                close(cliSocket);
                return 0;
            }
            recvSize = recv(cliSocket, buffer.data(), BUFFER_SIZE, 0);

            bufferPos = 0;
            waitTime.tv_sec = 1;
            waitTime.tv_usec = 0;
        }

        messageFlag = findMessage(message, buffer, bufferPos, recvSize);

        int res = serverOptimization(message, mode);
        if (res == -1) {
            send(cliSocket, SERVER_LOGIC_ERROR, sizeof(SERVER_LOGIC_ERROR) / sizeof(char) - 1, 0);
            close(cliSocket);
            return 0;
        }
        if (!res) {
            send(cliSocket, SERVER_SYNTAX_ERROR, sizeof(SERVER_SYNTAX_ERROR) / sizeof(char) - 1, 0);
            close(cliSocket);
            return 0;
        }



        if (messageFlag) {
            if (message == "RECHARGING") {
                waitTime.tv_sec = 5;
                waitTime.tv_usec = 0;
                lastMode = mode;

                mode = MODE_RECHARGING;
                message.clear();
                continue;
            }

            switch (mode) {
            case MODE_CONFIRM_SERVER: {

                clientHash = confirm(message, cliSocket);
                mode = MODE_CONFIRM_CLIENT;
                break;
            }
            case MODE_CONFIRM_CLIENT: {
                uint recvHash;
                int res = sscanf(message.c_str(), "%u", &recvHash);
                if (message.size() > 5 || res == 0 || recvHash > 65536) {
                    send(cliSocket, SERVER_SYNTAX_ERROR, sizeof(SERVER_SYNTAX_ERROR) / sizeof(char) - 1, 0);
                    close(cliSocket);
                    return 0;
                }

                if (clientHash != recvHash) {
                    send(cliSocket, SERVER_LOGIN_FAILED, sizeof(SERVER_LOGIN_FAILED) / sizeof(char) - 1, 0);
                    close(cliSocket);
                    return -1;
                }
                else {
                    mode = MODE_FIND_POS;
                    findPosFlag = 0;
                    send(cliSocket, SERVER_OK, sizeof(SERVER_OK) / sizeof(char) - 1, 0);
                    send(cliSocket, SERVER_MOVE, sizeof(SERVER_MOVE) / sizeof(char) - 1, 0);
                }
                break;
            }
            case MODE_FIND_POS: {
                Position p;
                if (!parseOK(message, p)) {
                    send(cliSocket, SERVER_SYNTAX_ERROR, sizeof(SERVER_SYNTAX_ERROR) / sizeof(char) - 1, 0);
                    close(cliSocket);
                    return 0;
                }
                if (findPosFlag == 0) {
                    r._p = p;
                    findPosFlag++;
                    send(cliSocket, SERVER_MOVE, sizeof(SERVER_MOVE) / sizeof(char) - 1, 0);
                }
                else {
                    ///////////////////////////////////////////////////////////////////////
                    int dx = p._x - r._p._x;
                    int dy = p._y - r._p._y;
                    if (dx == 1) {
                        r._d = 0;//right
                    }
                    if (dy == -1) {
                        r._d = 1;//bottom
                    }
                    if (dx == -1) {
                        r._d = 2;//left
                    }
                    if (dy == 1) {
                        r._d = 3;//up
                    }
                    r._p = p;
                    mode = MODE_GO_TO_TARGET_POS;
                    if (!goToTargetPos(targetPos, r, cliSocket)) {
                        send(cliSocket, SERVER_PICK_UP, sizeof(SERVER_PICK_UP) / sizeof(char) - 1, 0);
                        nextTargetPos(targetPos);
                        mode = MODE_PICK_UP;
                    }
                }

                break;
            }
            case MODE_GO_TO_TARGET_POS: {
                if (!parseOK(message, r._p)) {


                    send(cliSocket, SERVER_SYNTAX_ERROR, sizeof(SERVER_SYNTAX_ERROR) / sizeof(char) - 1, 0);
                    close(cliSocket);
                    return 0;
                }

                if (!goToTargetPos(targetPos, r, cliSocket)) {
                    send(cliSocket, SERVER_PICK_UP, sizeof(SERVER_PICK_UP) / sizeof(char) - 1, 0);
                    nextTargetPos(targetPos);
                    mode = MODE_PICK_UP;
                }

                break;
            }
            case MODE_PICK_UP: {

                if (message.size() == 0) {
                    mode = MODE_GO_TO_TARGET_POS;
                    if (!goToTargetPos(targetPos, r, cliSocket)) {
                        send(cliSocket, SERVER_PICK_UP, sizeof(SERVER_PICK_UP) / sizeof(char) - 1, 0);
                        nextTargetPos(targetPos);
                        mode = MODE_PICK_UP;
                    }
                }
                else {
                    cout << "Message: \"" << message << "\"" << endl;
                    send(cliSocket, SERVER_LOGOUT, sizeof(SERVER_LOGOUT) / sizeof(char) - 1, 0);
                    close(cliSocket);
                    return 0;
                }

                break;
            }

            case MODE_RECHARGING: {
                if (message == "FULL POWER") {
                    mode = lastMode;

                }
                else {
                    send(cliSocket, SERVER_LOGIC_ERROR, sizeof(SERVER_LOGIC_ERROR) / sizeof(char) - 1, 0);
                    close(cliSocket);
                    return 0;
                }

                break;
            }

            default:
                break;
            }

            messageFlag = 0;
            message.clear();
        }
    }

    return 0;
}

int main(void) {

    int mainSocket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    // bind socket to ip and port
    bind(mainSocket, (struct sockaddr*)&addr, sizeof(addr));
    listen(mainSocket, 1);

    while (1) {
        int cliSocket = accept(mainSocket, NULL, NULL);

        std::thread(runCommunication, cliSocket).detach();
    }
    close(mainSocket);

    return 0;
}
