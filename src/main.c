#include "Fwrd.h"

const char *create_users = "CREATE TABLE IF NOT EXISTS users(id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           " username TEXT NOT NULL UNIQUE, hash TEXT NOT NULL)";
const char *update_users_username = "UPDATE users SET username = ? WHERE username = ? AND hash = ?";
const char *create_snake =
    "CREATE TABLE IF NOT EXISTS snake_scores(id INTEGER PRIMARY KEY AUTOINCREMENT,"
    " user_id INTEGER NOT NULL UNIQUE, score INTEGER NOT NULL, FOREIGN KEY(user_id) REFERENCES users(id))";
const char *insert_snake = "INSERT OR REPLACE INTO snake_scores(user_id, score) VALUES(?, ?)";
const char *select_snake_hiscore = "SELECT score FROM snake_scores WHERE user_id = ?";
const char *select_snake_Nscores = "SELECT users.username, snake_scores.score FROM users JOIN snake_scores"
                                   " ON users.id = snake_scores.user_id ORDER BY snake_scores.score DESC LIMIT ?";
const char *create_pong =
    "CREATE TABLE IF NOT EXISTS pong_scores(id INTEGER PRIMARY KEY AUTOINCREMENT,"
    " user_id INTEGER NOT NULL UNIQUE, score INTEGER NOT NULL, FOREIGN KEY(user_id) REFERENCES users(id))";
const char *insert_pong = "INSERT OR REPLACE INTO pong_scores(user_id, score) VALUES(?, ?)";
const char *select_pong_hiscore = "SELECT score FROM pong_scores WHERE user_id = ?";
const char *select_pong_Nscores = "SELECT users.username, pong_scores.score FROM users JOIN pong_scores"
                                  " ON users.id = pong_scores.user_id ORDER BY pong_scores.score DESC LIMIT ?";
const char *create_sessions =
    "CREATE TABLE IF NOT EXISTS sessions(id INTEGER PRIMARY KEY AUTOINCREMENT, token TEXT NOT NULL UNIQUE,"
    " user_id INTEGER NOT NULL UNIQUE, expires_at INTEGER NOT NULL, FOREIGN KEY(user_id) REFERENCES users(id))";
const char *select_sessions = "SELECT expires_at FROM sessions WHERE user_id = ?";

Res res = {"HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\nContent-Length: 21\r\n\r\n"
           "Internal Server Error",
           "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 9\r\n\r\nNot Found",
           "HTTP/1.1 401 Unauthorized\r\nContent-Type: text/plain\r\nContent-Length: 16\r\n\r\nUnauthorized",
           "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 17\r\n\r\nMalformed Request",
           "HTTP/1.1 201 Created\r\nContent-Type: text/plain\r\nContent-Length: ",
           "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "};

SOCKET socket_setup(void)
{
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
        return SOCKET_ERROR;
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET)
    {
        WSACleanup();
        return SOCKET_ERROR;
    }
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
    {
        closesocket(server_socket);
        WSACleanup();
        return SOCKET_ERROR;
    }
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR)
    {
        closesocket(server_socket);
        WSACleanup();
        return SOCKET_ERROR;
    }
    printf("Listening on port %d\n", ntohs(server_addr.sin_port));
    return server_socket;
}

sqlite3 *db_setup(void)
{
    sqlite3 *db = NULL;
    char *errmsg = NULL;

    if (sqlite3_open("server.db", &db) != SQLITE_OK)
    {
        sqlite3_close(db);
        return NULL;
    }
    if (sqlite3_exec(db, "PRAGMA foreign_keys = ON;", NULL, NULL, &errmsg) != SQLITE_OK)
    {
        sqlite3_free(errmsg);
        sqlite3_close(db);
        return NULL;
    }
    if (sqlite3_exec(db, create_users, NULL, NULL, &errmsg) != SQLITE_OK)
    {
        sqlite3_free(errmsg);
        sqlite3_close(db);
        return NULL;
    }
    if (sqlite3_exec(db, create_snake, NULL, NULL, &errmsg) != SQLITE_OK)
    {
        sqlite3_free(errmsg);
        sqlite3_close(db);
        return NULL;
    }
    if (sqlite3_exec(db, create_pong, NULL, NULL, &errmsg) != SQLITE_OK)
    {
        sqlite3_free(errmsg);
        sqlite3_close(db);
        return NULL;
    }
    if (sqlite3_exec(db, create_sessions, NULL, NULL, &errmsg) != SQLITE_OK)
    {
        sqlite3_free(errmsg);
        sqlite3_close(db);
        return NULL;
    }

    return db;
}

int main(void)
{
    SOCKET server_socket = socket_setup();
    if (server_socket == SOCKET_ERROR)
    {
        return 1;
    }
    sqlite3 *db = db_setup();
    if (db == NULL)
    {
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }
    if (sodium_init() == -1)
    {
        sqlite3_close(db);
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }
    while (1)
    {
        struct sockaddr_in client_addr = {0};
        int client_addr_size = sizeof(client_addr);
        SOCKET client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_size);
        if (client_socket == INVALID_SOCKET)
        {
            sqlite3_close(db);
            closesocket(server_socket);
            WSACleanup();
            return 1;
        }
        char req_buf[4096] = {0};
        int bytes_received = recv(client_socket, req_buf, sizeof(req_buf) - 1, 0);
        if (bytes_received == SOCKET_ERROR)
        {
            closesocket(client_socket);
            continue;
        }
        char method[16], path[256], version[16];
        int parsed = sscanf(req_buf, "%15s %255s %15s", method, path, version);
        if (parsed != 3)
        {
            send(client_socket, res.res_400, strlen(res.res_400), 0);
            closesocket(client_socket);
            continue;
        }
        if (strcmp(method, "POST") == 0 && (strcmp(path, "/register") == 0 || strcmp(path, "/login") == 0))
        {
            handle_auth(client_socket, db, path, req_buf, &res);
            continue;
        }
        if (strcmp(method, "GET") == 0 && strncmp(path, "/score/", 7) == 0)
        {
            
        }
        
    }
    return 0;
}

/*
/score/snake post
/score/snake 100 get
/score/snake name get

/score/pong  post
/score/pong 100 get
/score/pong name get

/score/ && post
if (score/pong) {name score token in body}
if (score/snake) {name score token in body}

/score/ && get
if (score/pong name) 
if (score/snake name)
if (score/pong num)
if (score/snake num)
*/