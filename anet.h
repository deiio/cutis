/*
 * Copyright (c) 2023 ByteDance, All rights reserved.
 * Author: mn, mn@furzoom.com
 */

#ifndef ANET_H_
#define ANET_H_

#define ANET_OK 0
#define ANET_ERR -1
#define ANET_ERR_LEN 256

int anetTcpConnect(char *err, char *addr, int port);
int anetTcpNonBlockConnect(char *err, char *addr, int port);
int anetRead(int fd, void *buf, int count);
int anetWrite(int fd, void *buf, int count);
int anetResolve(char *err, char *host, char *ip_buf);
int anetTcpServer(char *err, int port, char *bind_addr);
int anetAccept(char *err, int sock, char *ip, int *port);
int anetNonBlock(char *err, int fd);
int anetTcpNoDelay(char *err, int fd);
int anetTcpKeepAlive(char *err, int fd);

#endif  // ANET_H_
