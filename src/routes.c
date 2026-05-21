#include "Fwrd.h"

#define MAX_RESPONSE_BODY 64935
#define MAX_RESPONSE_FULL 65065

const char *select_sessions_expiry = "SELECT id, expires_at FROM sessions WHERE token = ?";
const char *select_user_snakescore = "SELECT score FROM snake_scores WHERE user_id = ?";
const char *select_user_pongscore = "SELECT score FROM pong_scores WHERE user_id = ?";

int auth_posts(SOCKET client_socket, sqlite3 *db, char *path, char *req_buf, Res *res)
{
    char *body = strstr(req_buf, "\r\n\r\n");
    if (body == NULL)
    {
        send(client_socket, res->res_400, strlen(res->res_400), 0);
        closesocket(client_socket);
        return 1;
    }
    body += 4;
    char usertest[33] = {0}, passtest[33] = {0};
    sscanf(body, "%32s %32s", usertest, passtest);
    if (strlen(usertest) == 32 || strlen(passtest) == 32)
    {
        send(client_socket, res->res_400, strlen(res->res_400), 0);
        closesocket(client_socket);
        return 1;
    }
    sqlite3_stmt *statement = NULL;
    const char *insert_sessions = "INSERT INTO sessions(token, user_id, expires_at) VALUES (?, ?, ?)";
    if (strcmp(path, "/register") == 0)
    {
        char hash[crypto_pwhash_STRBYTES];
        if (crypto_pwhash_str(hash, passtest, strlen(passtest), crypto_pwhash_OPSLIMIT_INTERACTIVE,
                              crypto_pwhash_MEMLIMIT_INTERACTIVE) == -1)
        {
            send(client_socket, res->res_500, strlen(res->res_500), 0);
            closesocket(client_socket);
            return 1;
        }
        const char *insert_users = "INSERT INTO USERS(username, hash) VALUES(?, ?)";
        if (sqlite3_prepare_v2(db, insert_users, -1, &statement, NULL) != SQLITE_OK)
        {
            send(client_socket, res->res_500, strlen(res->res_500), 0);
            closesocket(client_socket);
            return 1;
        }
        sqlite3_bind_text(statement, 1, usertest, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 2, hash, -1, SQLITE_STATIC);
        int step_code = sqlite3_step(statement);
        if (step_code == SQLITE_CONSTRAINT)
        {
            sqlite3_finalize(statement);
            send(client_socket, res->res_409, strlen(res->res_409), 0);
            closesocket(client_socket);
            return 1;
        }
        if (step_code != SQLITE_DONE)
        {
            sqlite3_finalize(statement);
            send(client_socket, res->res_500, strlen(res->res_500), 0);
            closesocket(client_socket);
            return 1;
        }
        sqlite3_finalize(statement);
        sqlite3_int64 user_id = sqlite3_last_insert_rowid(db);
        unsigned char token_bytes[32] = {0};
        randombytes_buf(token_bytes, sizeof(token_bytes));
        char token[65] = {0};
        sodium_bin2hex(token, sizeof(token), token_bytes, sizeof(token_bytes));
        if (sqlite3_prepare_v2(db, insert_sessions, -1, &statement, NULL) != SQLITE_OK)
        {
            send(client_socket, res->res_500, strlen(res->res_500), 0);
            closesocket(client_socket);
            return 1;
        }
        sqlite3_bind_text(statement, 1, token, -1, SQLITE_STATIC);
        sqlite3_bind_int64(statement, 2, user_id);
        sqlite3_bind_int64(statement, 3, (sqlite3_int64)(time(NULL) + 2592000));
        if (sqlite3_step(statement) != SQLITE_DONE)
        {
            sqlite3_finalize(statement);
            send(client_socket, res->res_500, strlen(res->res_500), 0);
            closesocket(client_socket);
            return 1;
        }
        sqlite3_finalize(statement);
        char res_buf[256] = {0};
        snprintf(res_buf, sizeof(res_buf), "%s%zu\r\n\r\n%s", res->res_201_part, strlen(token), token);
        send(client_socket, res_buf, strlen(res_buf), 0);
        closesocket(client_socket);
        return 0;
    }
    if (strcmp(path, "/login") == 0)
    {
        const char *select_id_and_hash = "SELECT id, hash FROM users WHERE username = ?";
        if (sqlite3_prepare_v2(db, select_id_and_hash, -1, &statement, NULL) != SQLITE_OK)
        {
            send(client_socket, res->res_500, strlen(res->res_500), 0);
            closesocket(client_socket);
            return 1;
        }
        sqlite3_bind_text(statement, 1, usertest, -1, SQLITE_STATIC);
        if (sqlite3_step(statement) != SQLITE_ROW)
        {
            sqlite3_finalize(statement);
            send(client_socket, res->res_401, strlen(res->res_401), 0);
            closesocket(client_socket);
            return 1;
        }
        sqlite3_int64 user_id = sqlite3_column_int64(statement, 0);
        const char *hash_ptr_temp = (const char *)sqlite3_column_text(statement, 1);
        if (crypto_pwhash_str_verify(hash_ptr_temp, passtest, strlen(passtest)) == -1)
        {
            sqlite3_finalize(statement);
            send(client_socket, res->res_401, strlen(res->res_401), 0);
            closesocket(client_socket);
            return 1;
        }
        sqlite3_finalize(statement);
        unsigned char token_bytes[32] = {0};
        randombytes_buf(token_bytes, sizeof(token_bytes));
        char token[65] = {0};
        sodium_bin2hex(token, sizeof(token), token_bytes, sizeof(token_bytes));
        const char *select_session_for_id = "SELECT token FROM sessions WHERE user_id = ?";
        if (sqlite3_prepare_v2(db, select_session_for_id, -1, &statement, NULL) != SQLITE_OK)
        {
            send(client_socket, res->res_500, strlen(res->res_500), 0);
            closesocket(client_socket);
            return 1;
        }
        sqlite3_bind_int64(statement, 1, user_id);
        if (sqlite3_step(statement) == SQLITE_ROW)
        {
            sqlite3_finalize(statement);
            const char *updt_tkn = "UPDATE sessions SET token = ?, expires_at = ? WHERE user_id = ?";
            if (sqlite3_prepare_v2(db, updt_tkn, -1, &statement, NULL) != SQLITE_OK)
            {
                send(client_socket, res->res_500, strlen(res->res_500), 0);
                closesocket(client_socket);
                return 1;
            }
            sqlite3_bind_text(statement, 1, token, -1, SQLITE_STATIC);
            sqlite3_bind_int64(statement, 2, (sqlite3_int64)(time(NULL) + 2592000));
            sqlite3_bind_int64(statement, 3, user_id);
            if (sqlite3_step(statement) != SQLITE_DONE)
            {
                sqlite3_finalize(statement);
                send(client_socket, res->res_500, strlen(res->res_500), 0);
                closesocket(client_socket);
                return 1;
            }
            sqlite3_finalize(statement);
            char res_buf[256] = {0};
            snprintf(res_buf, sizeof(res_buf), "%s%zu\r\n\r\n%s", res->res_200_part, strlen(token), token);
            send(client_socket, res_buf, strlen(res_buf), 0);
            closesocket(client_socket);
            return 0;
        }
        else
        {
            sqlite3_finalize(statement);
        }
        if (sqlite3_prepare_v2(db, insert_sessions, -1, &statement, NULL) != SQLITE_OK)
        {
            send(client_socket, res->res_500, strlen(res->res_500), 0);
            closesocket(client_socket);
            return 1;
        }
        sqlite3_bind_text(statement, 1, token, -1, SQLITE_STATIC);
        sqlite3_bind_int64(statement, 2, user_id);
        sqlite3_bind_int64(statement, 3, (sqlite3_int64)(time(NULL) + 2592000));
        if (sqlite3_step(statement) != SQLITE_DONE)
        {
            sqlite3_finalize(statement);
            send(client_socket, res->res_500, strlen(res->res_500), 0);
            closesocket(client_socket);
            return 1;
        }
        sqlite3_finalize(statement);
        char res_buf[256] = {0};
        snprintf(res_buf, sizeof(res_buf), "%s%zu\r\n\r\n%s", res->res_200_part, strlen(token), token);
        send(client_socket, res_buf, strlen(res_buf), 0);
        closesocket(client_socket);
        return 0;
    }
    return 1;
}

int score_gets(SOCKET client_socket, sqlite3 *db, char *path, Res *res)
{
    sqlite3_stmt *statement = NULL;
    char *query_param = strchr(path, '?');
    if (query_param == NULL)
    {
        send(client_socket, res->res_400, strlen(res->res_400), 0);
        closesocket(client_socket);
        return 1;
    }
    char strcat_buf[MAX_RESPONSE_BODY] = {0};
    if (strncmp(query_param, "?limit=", 7) == 0)
    {
        query_param += 7;
        char tuple[65] = {0};
        char res_buf[MAX_RESPONSE_FULL] = {0};
        if (strncmp(path, "/score/snake", 12) == 0)
        {
            const char *select_n_snakescores =
                "SELECT users.username, snake_scores.score FROM users JOIN snake_scores ON users.id "
                "= snake_scores.user_id ORDER BY snake_scores.score DESC LIMIT ?";
            if (sqlite3_prepare_v2(db, select_n_snakescores, -1, &statement, NULL) != SQLITE_OK)
            {
                send(client_socket, res->res_500, strlen(res->res_500), 0);
                closesocket(client_socket);
                return 1;
            }
            char *endptr = NULL;
            errno = 0;
            long scores_limit = strtol(query_param, &endptr, 10);
            if (*endptr != '\0' || errno == ERANGE)
            {
                send(client_socket, res->res_400, strlen(res->res_400), 0);
                closesocket(client_socket);
                return 1;
            }
            scores_limit = scores_limit <= 0 ? 50 : scores_limit > 250 ? 250 : scores_limit;
            sqlite3_bind_int64(statement, 1, (sqlite_int64)scores_limit);
            while (sqlite3_step(statement) == SQLITE_ROW)
            {
                const char *usrname = (const char *)sqlite3_column_text(statement, 0);
                int scr = sqlite3_column_int(statement, 1);
                snprintf(tuple, sizeof(tuple), "%s %d\n", usrname, scr);
                strcat(strcat_buf, tuple);
            }
            sqlite3_finalize(statement);
            if (strlen(strcat_buf) == 0)
            {
                send(client_socket, res->res_404, strlen(res->res_404), 0);
                closesocket(client_socket);
                return 1;
            }
            snprintf(res_buf, sizeof(res_buf), "%s%zu\r\n\r\n%s", res->res_200_part, strlen(strcat_buf), strcat_buf);
            send(client_socket, res_buf, strlen(res_buf), 0);
            closesocket(client_socket);
            return 0;
        }
        if (strncmp(path, "/score/pong", 11) == 0)
        {
            const char *select_n_pongscores =
                "SELECT users.username, pong_scores.score FROM users JOIN pong_scores ON users.id = "
                "pong_scores.user_id ORDER BY pong_scores.score DESC LIMIT ?";
            if (sqlite3_prepare_v2(db, select_n_pongscores, -1, &statement, NULL) != SQLITE_OK)
            {
                send(client_socket, res->res_500, strlen(res->res_500), 0);
                closesocket(client_socket);
                return 1;
            }
            char *endptr = NULL;
            errno = 0;
            long scores_limit = strtol(query_param, &endptr, 10);
            if (*endptr != '\0' || errno == ERANGE)
            {
                send(client_socket, res->res_400, strlen(res->res_400), 0);
                closesocket(client_socket);
                return 1;
            }
            scores_limit = scores_limit <= 0 ? 50 : scores_limit > 250 ? 250 : scores_limit;
            sqlite3_bind_int64(statement, 1, (sqlite_int64)scores_limit);
            while (sqlite3_step(statement) == SQLITE_ROW)
            {
                const char *usrname = (const char *)sqlite3_column_text(statement, 0);
                int scr = sqlite3_column_int(statement, 1);
                snprintf(tuple, sizeof(tuple), "%s %d\n", usrname, scr);
                strcat(strcat_buf, tuple);
            }
            sqlite3_finalize(statement);
            if (strlen(strcat_buf) == 0)
            {
                send(client_socket, res->res_404, strlen(res->res_404), 0);
                closesocket(client_socket);
                return 1;
            }
            snprintf(res_buf, sizeof(res_buf), "%s%zu\r\n\r\n%s", res->res_200_part, strlen(strcat_buf), strcat_buf);
            send(client_socket, res_buf, strlen(res_buf), 0);
            closesocket(client_socket);
            return 0;
        }
    }
    if (strncmp(query_param, "?username=", 10) == 0)
    {
        char res_string[65] = {0};
        char req_string[33] = {0};
        char res_buf[256] = {0};
        query_param += 10;
        if (strncmp(path, "/score/snake", 12) == 0)
        {
            const char *select_srch_snakescore =
                "SELECT users.username, snake_scores.score FROM users JOIN snake_scores ON "
                "users.id = snake_scores.user_id WHERE users.username = ?";
            if (sqlite3_prepare_v2(db, select_srch_snakescore, -1, &statement, NULL) != SQLITE_OK)
            {
                send(client_socket, res->res_500, strlen(res->res_500), 0);
                closesocket(client_socket);
                return 1;
            }
            if (strlen(query_param) >= 32)
            {
                send(client_socket, res->res_400, strlen(res->res_400), 0);
                closesocket(client_socket);
                return 1;
            }
            strcpy(req_string, query_param);
            sqlite3_bind_text(statement, 1, req_string, -1, SQLITE_STATIC);
            if (sqlite3_step(statement) != SQLITE_ROW)
            {
                sqlite3_finalize(statement);
                send(client_socket, res->res_404, strlen(res->res_404), 0);
                closesocket(client_socket);
                return 1;
            }
            const char *usrname = (const char *)sqlite3_column_text(statement, 0);
            int scr = sqlite3_column_int(statement, 1);
            snprintf(res_string, sizeof(res_string), "%s %d", usrname, scr);
            // add score overflow guard later
            sqlite3_finalize(statement);
            snprintf(res_buf, sizeof(res_buf), "%s%zu\r\n\r\n%s", res->res_200_part, strlen(res_string), res_string);
            send(client_socket, res_buf, strlen(res_buf), 0);
            closesocket(client_socket);
            return 0;
        }
        if (strncmp(path, "/score/pong", 11) == 0)
        {
            const char *select_srch_pongscore =
                "SELECT users.username, pong_scores.score FROM users JOIN pong_scores ON "
                "users.id = pong_scores.user_id WHERE users.username = ?";
            if (sqlite3_prepare_v2(db, select_srch_pongscore, -1, &statement, NULL) != SQLITE_OK)
            {
                send(client_socket, res->res_500, strlen(res->res_500), 0);
                closesocket(client_socket);
                return 1;
            }
            if (strlen(query_param) >= 32)
            {
                send(client_socket, res->res_400, strlen(res->res_400), 0);
                closesocket(client_socket);
                return 1;
            }
            strcpy(req_string, query_param);
            sqlite3_bind_text(statement, 1, req_string, -1, SQLITE_STATIC);
            if (sqlite3_step(statement) != SQLITE_ROW)
            {
                sqlite3_finalize(statement);
                send(client_socket, res->res_404, strlen(res->res_404), 0);
                closesocket(client_socket);
                return 1;
            }
            const char *usrname = (const char *)sqlite3_column_text(statement, 0);
            int scr = sqlite3_column_int(statement, 1);
            snprintf(res_string, sizeof(res_string), "%s %d", usrname, scr);
            // TODO: overflow guard
            sqlite3_finalize(statement);
            snprintf(res_buf, sizeof(res_buf), "%s%zu\r\n\r\n%s", res->res_200_part, strlen(res_string), res_string);
            send(client_socket, res_buf, strlen(res_buf), 0);
            closesocket(client_socket);
            return 0;
        }
    }
    return 1;
}