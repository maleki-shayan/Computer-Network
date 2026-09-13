# CA1 — TCP Chat and File Sharing

A terminal-based client/server application for group chat, private messaging, and file sharing over TCP. The server uses `select()` to handle multiple client connections. Separate classes manage users, commands, messages, and file metadata.

[Assignment](ca1.pdf) · [Report](Report.pdf)

## Build and run

Requirements: Linux or Ubuntu/WSL, GNU Make, and a C++17-capable compiler.

From this directory:

```bash
make CXXFLAGS='-std=c++17 -Wall -Wextra'
./server.out
```

In a second terminal, from the same directory:

```bash
./client.out
```

The client connects to `127.0.0.1:8080`. Open additional clients in separate terminals to try group and private messages.

## Commands

| Command | Action |
| --- | --- |
| `REGISTER alice demo-password` | Create an account |
| `LOGIN alice demo-password` | Log in |
| `MSG hello everyone` | Send a group message |
| `PM bob hello` | Send a private message |
| `USERS` | List online users |
| `PUT example.txt` | Upload a local file |
| `LIST` | List uploaded files |
| `GET example.txt` | Download a file |
| `QUIT` | Disconnect |

Uploads are stored in `uploaded files/` and downloads in `downloaded files/`. Accounts and the uploaded-file index are kept in memory for the server session. Use simple filenames without spaces for the examples above.
