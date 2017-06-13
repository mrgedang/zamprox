//
//  main.c
//  zamprox
//
//  Created by ~GG~ on 4/2/17.
//  Copyright © 2017 ~GG~. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "injector.h"

#define BUF_SIZE 16384

int buat_server(int localport);
int konek_ke_proxy();
void usage();
void server_loop();
void sigchld_handler();
void sigterm_handler();
void handle_data(int sockClient);
void proses_data_client(int sockClient, int sockProxy);
void proses_data_proxy(int sockProxy, int sockClient);

int localport, remoteport;
int sockServer, sockClient, sockProxy;
const char *ipProxy;

int main(int argc, const char * argv[]) {
    pid_t pid;
    // Opsi yang dibutuhkan ada 3
    // <local port> <remote ip> <remote port>
    if(argc != 4){
        usage();
        exit(EXIT_FAILURE);
    }
    
    // Ambil argument local port
    localport = atoi(argv[1]);
    // Ambil argument remote ip
    ipProxy = argv[2];
    // Ambil argument remote port
    remoteport = atoi(argv[3]);
    
    // Listen server
    if((sockServer = buat_server(localport)) < 0){
        exit(EXIT_FAILURE);
    }
    
    printf("Listen pada port %d\n", localport);
    
    // Sampai di sini perlu dibuat penghandle client masuk dengan accept()
    // Kita akan menggunakan fork(), sehingga diperlukan signal handler
    signal(SIGCHLD, sigchld_handler);
    signal(SIGTERM, sigterm_handler);
    
    // Daemonize
    if((pid = fork()) == 0){
        // Panggil fungsi buat server
        server_loop();
        //printf("Debug %d %s %d %d %d\n", localport, ipProxy, remoteport, sockServer, sockClient);
    }else{
        close(sockServer);
    }

    return EXIT_SUCCESS;
}

void usage(){
    printf("***************************************\n");
    printf("*        zamprox ANSI C version       *\n");
    printf("*        created by : ~GG~            *\n");
    printf("***************************************\n");
    printf("Usage : ./zamprox 8080 192.168.1.1 8080\n\n");
}

// Membuat socket dan menentukan alamat di mana server akan listen port
int buat_server(int localport){
    int sockServer, optval = 1;
    struct sockaddr_in serverAddr;
    
    // Buat socket untuk server
    if((sockServer = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        return -1;
    }
    
    // Bagian penting!
    // Set socket option keep alive
    if(setsockopt(sockServer, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0){
        return -1;
    }
    
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(localport);
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    
    // Bind alamat dan socket
    if(bind(sockServer, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) != 0){
        return -1;
    }
    
    // Listen port
    if(listen(sockServer, 20) < 0){
        return -1;
    }
    
    return sockServer;
}

// Pemroses koneksi masuk
// Fork setiap ada koneksi masuk
void server_loop(){
    struct sockaddr_in clientAddr;
    socklen_t size = sizeof(clientAddr);
    while(1){
        sockClient = accept(sockServer, (struct sockaddr*)&clientAddr, &size);
        if(fork() == 0){
            // Kita berada pada child yang tidak perlu sockServer
            close(sockServer);
            // Panggil pemroses data
            handle_data(sockClient);
            exit(0);
        }
        close(sockClient);
    }
}


void handle_data(int sockClient){
    // Sebagai proxy forwarder pertama kita akan melakukan koneksi menuju parent proxy
    if((sockProxy = konek_ke_proxy()) < 0){
        goto end;
    }
    if(fork() == 0){
        proses_data_client(sockClient, sockProxy);
        exit(0);
    }
    if(fork() == 0){
        proses_data_proxy(sockProxy, sockClient);
        exit(0);
    }
end:
    close(sockClient);
    close(sockProxy);
}

// Buat koneksi ke proxy parent untuk setiap client
int konek_ke_proxy(){
    int sockProxy;
    struct sockaddr_in proxyAddr;
    if((sockProxy = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        return -1;
    }
    
    memset(&proxyAddr, 0, sizeof(proxyAddr));
    proxyAddr.sin_family = AF_INET;
    proxyAddr.sin_port = htons(remoteport);
    proxyAddr.sin_addr.s_addr = inet_addr(ipProxy);
    
    // Langsung konek saja
    if(connect(sockProxy, (struct sockaddr*)&proxyAddr, sizeof(proxyAddr)) < 0){
        return -1;
    }
    
    return sockProxy;
}

// Untuk manipulasi request dari client
void proses_data_client(int sockClient, int sockProxy){
    ssize_t n;
    char buffer[BUF_SIZE];
    
    while ((n = recv(sockClient, buffer, BUF_SIZE, 0)) > 0) {
        // Selalu gunakan proxifier
        if(!strncasecmp(buffer, "CONNECT ", 8)){
            int fl_ptr = get_first_line(buffer);
            if(fl_ptr > 0){
                char *tmpbuf = malloc(fl_ptr);
                memcpy(&tmpbuf[0], buffer, fl_ptr);
                char *encoded = malloc(512);
                en64((unsigned char *)tmpbuf, (unsigned char *)encoded, fl_ptr);
                tmpbuf = malloc(BUF_SIZE);
                int send_size = 0;
                char *inject_request = "GET http://somedomain.tld/ HTTP/1.1\r\n";
                char *inject_header = "Gedang-Goreng: ";
                memcpy(&tmpbuf[send_size], inject_request, strlen(inject_request));
                send_size += strlen(inject_request);
                memcpy(&tmpbuf[send_size], inject_header, strlen(inject_header));
                send_size += strlen(inject_header);
                memcpy(&tmpbuf[send_size], encoded, strlen(encoded));
                send_size += strlen(encoded);
                memcpy(&tmpbuf[send_size], "\r\n", 2);
                send_size += 2;
                memcpy(&tmpbuf[send_size], buffer + fl_ptr, strlen(buffer) - fl_ptr);
                send_size += strlen(buffer) - fl_ptr;
                send(sockProxy, tmpbuf, send_size, 0);
                free(tmpbuf);
                free(encoded);
            }else{
                send(sockProxy, buffer, n, 0);
            }
        }else{
            send(sockProxy, buffer, n, 0);
        }
    }
    shutdown(sockProxy, SHUT_RDWR);
    close(sockProxy);
    
    shutdown(sockClient, SHUT_RDWR);
    close(sockClient);
}

// Data dari remote tidak perlu dimodifikasi
void proses_data_proxy(int sockProxy, int sockClient){
    ssize_t n;
    char buffer[BUF_SIZE];
    
    while ((n = recv(sockProxy, buffer, BUF_SIZE, 0)) > 0) {
        send(sockClient, buffer, n, 0);
    }
    shutdown(sockClient, SHUT_RDWR);
    close(sockClient);
    
    shutdown(sockProxy, SHUT_RDWR);
    close(sockProxy);
}


// Handle untuk child yang selesai tugas
// Untuk mengantisipasi terjadinya zombie
void sigchld_handler(int signal) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Handle terminate signal
void sigterm_handler(int signal) {
    close(sockClient);
    close(sockServer);
    exit(0);
}
