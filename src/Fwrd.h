#ifndef FWRD_H
#define FWRD_H

#include <sodium.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>

typedef struct {
    const char *res_500;
    const char *res_404;
    const char *res_401;
    const char *res_400;
    const char *res_201_part;
    const char *res_200_part;
} Res;

int handle_auth(SOCKET client_socket, sqlite3 *db, char *path, char *req_buf, Res *res);

#endif