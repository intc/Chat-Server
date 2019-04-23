Chat Server
=

*Note: This is an experimental fork of https://github.com/yorickdewid/Chat-Server.*

* Thread safe library functions are used
* Compiled as C11

Simple chatroom in C. This project demonstrates the basic use of sockets. There is no client available but any telnet client will do. Just connect to the server on the specified port and address. By default port 5000 is used. The project was intended to run on Linux and Unix based systems. However with minor changes you'd be able to run it on Windows.

## Build

Run GNU make in the repository

`make`

Then start

`./chat_server`

Or build an executable with debug symbols

`make debug`

## Chat commands

| Command       | Parameter               |                                     |
| ------------- | ----------------------- | ----------------------------------- |
| \exit         |                         | Leave the chatroom                  |
| \ping         |                         | Test connection, responds with PONG |
| \nick         | [nickname]              | Change nickname                     |
| \msg          | [receiver_id] [message] | Send private message                |
| \who          |                         | Show active clients                 |
| \help         |                         | Show this help                      |
