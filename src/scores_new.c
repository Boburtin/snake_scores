#include "Fwrd.h"

int score_posts(SOCKET client_socket, sqlite3 *db, char *path, char *req_buf, Res *res)
{
    char *body = strstr(req_buf, "\r\n\r\n");
    if (body == NULL)
    {
        send(client_socket, res->res_400, strlen(res->res_400), 0);
        closesocket(client_socket);
        return 1;
    }
    for (char *ptr = req_buf; ptr < body; ptr++)
    {
        if (*ptr >= 65 && *ptr <= 90)
            *ptr += 32;
    }
    const char *res_401_part = "HTTP/1.1 401 Unauthorized\r\nContent-Type: text/plain\r\nContent-Length: 29\r\n\r\n";
    char res_buf[256] = {0};
    char *auth_ptr = strstr(req_buf, "authorization: bearer ");
    if (auth_ptr == NULL)
    {
        snprintf(res_buf, sizeof(res_buf), "%sMissing/Invalid Authorization", res_401_part);
        send(client_socket, res_buf, strlen(res_buf), 0);
        closesocket(client_socket);
        return 1;
    }
    auth_ptr += 22;
    if (auth_ptr[0] == '\r' || auth_ptr[0] == '\n' || auth_ptr[0] == ' ' || auth_ptr[0] == '\0')
    {
        send(client_socket, res->res_400, strlen(res->res_400), 0);
        closesocket(client_socket);
        return 1;
    }
    char auth_token[65] = {0};
    sscanf(auth_ptr, "%64s", auth_token);
    char usertest[33] = {0}, scrtest[33] = {0};
    body += 4;
    int parsed = sscanf(body, "%32s %32s", usertest, scrtest);
    if (parsed != 2)
    {
        send(client_socket, res->res_400, strlen(res->res_400), 0);
        closesocket(client_socket);
        return 1;
    }
    char *endptr = NULL;
    errno = 0;
    long scr = strtol(scrtest, &endptr, 10);
    if (*endptr != '\0' || errno == ERANGE)
    {
        send(client_socket, res->res_400, strlen(res->res_400), 0);
        closesocket(client_socket);
        return 1;
    }
    sqlite3_int64 user_id = validate(db, usertest, auth_token);
    if (user_id == -1)
    {
        snprintf(res_buf, sizeof(res_buf), "%sExpired Auth/Invalid Username", res_401_part);
        send(client_socket, res_buf, strlen(res_buf), 0);
        closesocket(client_socket);
        return 1;
    }
    sqlite3_stmt *statement = NULL;
    if (strcmp(path, "/score/snake") == 0)
    {
        const char *insert_str = "INSERT OR REPLACE INTO snake_scores(user_id, score) VALUES(?, ?)";
        if (sqlite3_prepare_v2(db, insert_str, -1, &statement, NULL) != SQLITE_OK)
        {
            send(client_socket, res->res_500, strlen(res->res_500), 0);
            closesocket(client_socket);
            return 1;
        }
        sqlite3_bind_int64(statement, 1, user_id);
        sqlite3_bind_int64(statement, 2, (sqlite_int64)scr);
        if (sqlite3_step(statement) != SQLITE_DONE)
        {
            sqlite3_finalize(statement);
            send(client_socket, res->res_500, strlen(res->res_500), 0);
            closesocket(client_socket);
            return 1;
        }
        sqlite3_finalize(statement);
        snprintf(res_buf, sizeof(res_buf), "%s7\r\n\r\nSuccess", res->res_201_part);
        send(client_socket, res_buf, strlen(res_buf), 0);
        closesocket(client_socket);
        return 0;
    }
    if (strcmp(path, "/score/pong") == 0)
    {
        const char *insert_str = "INSERT OR REPLACE INTO pong_scores(user_id, score) VALUES(?, ?)";
        if (sqlite3_prepare_v2(db, insert_str, -1, &statement, NULL) != SQLITE_OK)
        {
            send(client_socket, res->res_500, strlen(res->res_500), 0);
            closesocket(client_socket);
            return 1;
        }
        sqlite3_bind_int64(statement, 1, user_id);
        sqlite3_bind_int64(statement, 2, (sqlite_int64)scr);
        if (sqlite3_step(statement) != SQLITE_DONE)
        {
            sqlite3_finalize(statement);
            send(client_socket, res->res_500, strlen(res->res_500), 0);
            closesocket(client_socket);
            return 1;
        }
        sqlite3_finalize(statement);
        snprintf(res_buf, sizeof(res_buf), "%s7\r\n\r\nSuccess", res->res_201_part);
        send(client_socket, res_buf, strlen(res_buf), 0);
        closesocket(client_socket);
        return 0;
    }
    return 1;
}

sqlite3_int64 validate(sqlite3 *db, char *username, char *auth_token)
{
    sqlite3_stmt *statement = NULL;
    const char *check_for_expired =
        "SELECT sessions.user_id FROM sessions JOIN users ON users.id = sessions.user_id WHERE users.username = ? AND "
        "sessions.token = ? AND sessions.expires_at > strftime('%s', 'now')";
    if (sqlite3_prepare_v2(db, check_for_expired, -1, &statement, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_text(statement, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, auth_token, -1, SQLITE_STATIC);
    if (sqlite3_step(statement) != SQLITE_ROW)
    {
        sqlite3_finalize(statement);
        return -1;
    }
    sqlite3_int64 user_id = sqlite3_column_int64(statement, 0);
    sqlite3_finalize(statement);
    return user_id;
}