#include <stdio.h>
#include <string.h>
#include <winsock2.h>

#include "./lib/sqlite3.h"

typedef struct
{
    char username[32];
    unsigned int score;
} Entry;

typedef struct
{
    const char *response_500;
    const char *response_404;
    const char *response_400;
    const char *response_201;
} Responses;

typedef struct
{
    const char *create_db;
    const char *insert_db;
    const char *select_db;
    const char *selectn_db;
} Queries;

Responses responses = {"HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\n"
                       "Content-Length: 21\r\n\r\nInternal Server Error",
                       "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n"
                       "Content-Length: 9\r\n\r\nNot Found",
                       "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n"
                       "Content-Length: 17\r\n\r\nMalformed Request",
                       "HTTP/1.1 201 Created\r\nContent-Type: text/plain\r\n"
                       "Content-Length: 7\r\n\r\nCreated"};

Queries queries = {"CREATE TABLE IF NOT EXISTS scores("
                   "username TEXT PRIMARY KEY NOT NULL, "
                   "score INTEGER NOT NULL)",
                   "INSERT OR REPLACE INTO scores("
                   "username, score) VALUES (?, ?)",
                   "SELECT score FROM scores WHERE username = ?",
                   "SELECT username, score, DENSE_RANK() OVER ("
                   "ORDER BY score DESC) AS rank FROM scores LIMIT ?"};

int main(void)
{
    // init struct
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        return 1;
    }
    // create socket
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET)
    {
        WSACleanup();
        return 1;
    }
    // set to: IPV4, use any address, use Big-Endian (network byte order)
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);
    // (sockaddr *) cast because bind accepts ipv4 or ipv6 sockaddr_in structs
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
    {
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }
    // probably won't fail here if bind succeeded but safety
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR)
    {
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    sqlite3 *db = NULL;
    char *errmsg = NULL;

    if (sqlite3_open("scores.db", &db) != SQLITE_OK)
    {
        sqlite3_close(db);
        WSACleanup();
        return 1;
    }

    if (sqlite3_exec(db, queries.create_db, NULL, NULL, &errmsg) != SQLITE_OK)
    {
        sqlite3_free(errmsg);
        sqlite3_close(db);
        WSACleanup();
        return 1;
    }

    while (1)
    {

        Entry entry = {0};
        struct sockaddr_in client_addr = {0};
        int client_addr_size = sizeof(client_addr);

        SOCKET client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_size);
        if (client_socket == INVALID_SOCKET)
        {
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
            send(client_socket, responses.response_400, strlen(responses.response_400), 0);
            closesocket(client_socket);
            continue;
        }

        if (strcmp(method, "POST") == 0 && strcmp(path, "/scores") == 0)
        {

            char *body = strstr(req_buf, "\r\n\r\n");

            if (body == NULL)
            {
                send(client_socket, responses.response_500, strlen(responses.response_500), 0);
                closesocket(client_socket);
                continue;
            }
            // skip the \r\n\r\n
            body += 4;

            char test[33] = {0};
            sscanf(body, "%32s", test);
            if (strlen(test) == 32)
            {
                send(client_socket, responses.response_400, strlen(responses.response_400), 0);
                closesocket(client_socket);
                continue;
            } // check for proper length name

            if (sscanf(body, "%31s %u", entry.username, &entry.score) != 2)
            {
                send(client_socket, responses.response_500, strlen(responses.response_500), 0);
                closesocket(client_socket);
                continue;
            }

            sqlite3_stmt *stmt; // statement handle (self reminder, still learning sqlite3)
            if (sqlite3_prepare_v2(db, queries.insert_db, -1, &stmt, NULL) != SQLITE_OK)
            {
                send(client_socket, responses.response_500, strlen(responses.response_500), 0);
                closesocket(client_socket);
                continue;
            }

            sqlite3_bind_text(stmt, 1, entry.username, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, entry.score);

            if (sqlite3_step(stmt) != SQLITE_DONE)
            {
                sqlite3_finalize(stmt);
                send(client_socket, responses.response_500, strlen(responses.response_500), 0);
                closesocket(client_socket);
                continue;
            }

            sqlite3_finalize(stmt);
            send(client_socket, responses.response_201, strlen(responses.response_201), 0);
            closesocket(client_socket);
            continue;
        }
        else if (strcmp(method, "GET") == 0 && strncmp(path, "/scores/", 8) == 0)
        {

            if (sscanf(path, "/scores/%31s", entry.username) != 1)
            {
                send(client_socket, responses.response_400, strlen(responses.response_400), 0);
                closesocket(client_socket);
                continue;
            }

            sqlite3_stmt *stmt;
            if (sqlite3_prepare_v2(db, queries.select_db, -1, &stmt, NULL) != SQLITE_OK)
            {
                send(client_socket, responses.response_500, strlen(responses.response_500), 0);
                closesocket(client_socket);
                continue;
            } // if sqlite3_prepare_v2 fails we don't need to call finalize

            sqlite3_bind_text(stmt, 1, entry.username, -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) == SQLITE_ROW)
            {
                entry.score = sqlite3_column_int(stmt, 0);
                char body[32] = {0};
                snprintf(body, sizeof(body), "%u", entry.score);
                char res_buf[128] = {0};
                snprintf(res_buf, sizeof(res_buf),
                         "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n"
                         "Content-Length: %zu\r\n\r\n%s",
                         strlen(body), body);
                send(client_socket, res_buf, strlen(res_buf), 0);
                printf("Method: %s\nPath: %s\nUsername: %s\nScore: %u", method, path, entry.username, entry.score);
            }
            else
            {
                send(client_socket, responses.response_404, strlen(responses.response_404), 0);
            }
            sqlite3_finalize(stmt);
            closesocket(client_socket);
            continue;
        }
        else
        { // kind of ugly having this block just send the same response as the get's else but w/e
            send(client_socket, responses.response_404, strlen(responses.response_404), 0);
            closesocket(client_socket);
            continue;
        }
    }
    sqlite3_close(db);
    return 0;
}