#ifndef FWRD_H
#define FWRD_H

#include <errno.h>
#include <sodium.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>

typedef struct
{
    const char *res_500;
    const char *res_409;
    const char *res_404;
    const char *res_401;
    const char *res_400;
    const char *res_201_part;
    const char *res_200_part;
} Res;

int auth_posts(SOCKET client_socket, sqlite3 *db, char *path, char *req_buf, Res *res);

int score_gets(SOCKET client_socket, sqlite3 *db, char *path, Res *res);

int score_posts(SOCKET client_socket, sqlite3 *db, char *path, char *req_buf, Res *res);

sqlite3_int64 validate(sqlite3 *db, char *username, char *auth_token);

#endif