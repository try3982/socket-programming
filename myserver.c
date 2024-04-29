#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024
#define MAX_PATH_SIZE 2000 // 파일 경로의 최대 길이

void handle_request(int client_socket);
void send_response(int client_socket, const char *file_path);

void send_file(int client_socket, FILE *file) {
    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    // 파일을 읽어서 클라이언트에게 전송
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        if (send(client_socket, buffer, bytes_read, 0) < 0) {
            perror("Error sending file data");
            fclose(file);
            exit(EXIT_FAILURE);
        }
    }

    // 파일을 모두 전송한 후 파일을 닫음
    fclose(file);
}

long get_file_size(FILE *file) {
    long size;
    fseek(file, 0, SEEK_END); // 파일 끝으로 이동
    size = ftell(file);       // 현재 파일 위치 가져오기
    fseek(file, 0, SEEK_SET); // 파일 처음으로 이동
    return size;
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);

    if (port <= 1024 || port >= 65536) {
        fprintf(stderr, "Invalid port number. Please choose a port between 1025 and 65535.\n");
        exit(EXIT_FAILURE);
    }

    // 소켓 생성
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error opening socket");
        exit(EXIT_FAILURE);
    }

    // 소켓 구조체 초기화
    struct sockaddr_in server_addr, client_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // 소켓에 주소 바인딩
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error in binding");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // 연결 대기
    if (listen(server_socket, 5) < 0) {
        perror("Error in listen");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", port);

    while (1) {
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Error in accepting connection");
            close(server_socket);
            exit(EXIT_FAILURE);
        }

        handle_request(client_socket);
        

        close(client_socket);
    }

    close(server_socket);
    return 0;
}
void handle_request(int client_socket) {
    pid_t pid = fork(); // 자식 프로세스 생성

    if (pid < 0) {
        perror("Error forking");
        exit(EXIT_FAILURE);
    } else if (pid == 0) { // 자식 프로세스
        // 클라이언트로부터 요청 메시지 읽기
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = read(client_socket, buffer, BUFFER_SIZE - 1);
        if (bytes_received < 0) {
            perror("Error reading from socket");
            exit(EXIT_FAILURE);
        }

        // 요청 메시지 콘솔에 출력
        printf("Received HTTP request from client:\n%s\n", buffer);

        // 요청 메시지 파싱
        char method[10], url[100];
        sscanf(buffer, "%s /%s HTTP/1.1", method, url);
        printf("요청 메세지 파싱 완료, %s\n", url);
        url[strlen(url)] = '\0';

        if (strcmp(method, "GET") == 0) {
            printf("send resp 전 \n");
            send_response(client_socket, url);
            printf("send resp 후 \n");
        }

        // 자식 프로세스 종료
        close(client_socket);
        exit(EXIT_SUCCESS);
    } else { // 부모 프로세스
        // 클라이언트 소켓 닫기
        close(client_socket);
    }
}
void send_response(int client_socket, const char *file_path) {
    FILE *file = fopen(file_path, "rb"); // 파일을 바이너리 모드로 읽기 전용으로 열기
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // 파일 크기 구하기
    long size = get_file_size(file);

    // HTTP 응답 헤더 작성
    char response[BUFFER_SIZE];
    if (strstr(file_path, ".mp3")) {
        sprintf(response, "HTTP/1.1 200 OK\nContent-Type: audio/mpeg\nContent-Length: %ld\n\n", size);
    } else if (strstr(file_path, ".jpg")) {
        sprintf(response, "HTTP/1.1 200 OK\nContent-Type: image/jpeg\nContent-Length: %ld\n\n", size);
    } else if (strstr(file_path, ".gif")) {
        sprintf(response, "HTTP/1.1 200 OK\nContent-Type: image/gif\nContent-Length: %ld\n\n", size);
    } else {
        sprintf(response, "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: %ld\n\n", size);
    }

    // HTTP 응답 헤더 전송
    if (send(client_socket, response, strlen(response), 0) < 0) {
        perror("Error sending HTTP response header");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // 파일을 클라이언트에게 전송
    send_file(client_socket, file);

    // 클라이언트 소켓 닫기
    close(client_socket);
}
