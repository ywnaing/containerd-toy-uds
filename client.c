#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "grpc_frame.h"

#define SOCKET_PATH "/tmp/containerd_toy.sock"

static void call_rpc(int sock, const char *cmd) {
  printf("[Client] --> Sending gRPC Frame: \"%s\"\n", cmd);

  // Send length-prefixed 5-byte header + payload
  if (!grpc_send_frame(sock, cmd, (uint32_t)strlen(cmd))) {
    perror("grpc_send_frame failed");
    return;
  }

  // Receive length-prefixed response
  uint8_t response[1024] = {0};
  uint32_t resp_len = 0;
  if (!grpc_recv_frame(sock, response, sizeof(response) - 1, &resp_len)) {
    perror("grpc_recv_frame failed");
    return;
  }

  response[resp_len] = '\0';
  printf("[Client] <-- Received gRPC Response (%u bytes):\n         %s\n\n",
         resp_len, (char *)response);
}

int main(void) {
  int sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_un addr = {0};
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

  if (connect(sock, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("connect");
    close(sock);
    exit(EXIT_FAILURE);
  }

  printf("========================================================\n");
  printf(" 🚀 Connected to containerd_toy.sock over gRPC/UDS\n");
  printf("========================================================\n\n");

  // Perform multiple RPC calls over the SAME persistent connection stream
  call_rpc(sock, "RunContainer redis:7.0-alpine");
  call_rpc(sock, "RunContainer nginx:latest");
  call_rpc(sock, "ListContainers");
  call_rpc(sock, "StopContainer c-1");
  call_rpc(sock, "ListContainers");

  close(sock);
  printf("[Client] Finished all RPC calls. Disconnected cleanly.\n");
  return 0;
}
