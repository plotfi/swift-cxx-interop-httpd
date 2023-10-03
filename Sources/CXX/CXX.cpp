#include <string>

#if !(BOOTSTRAPINTEROP)
// Real Code
#include "swift-header.h"
#else
// Dummy Code
#endif

#define REUSE_PORT 1
#define MAXPENDING 5  // Max Requests
#define BUFFERLEN 100 // recv size per iter
#define HTTPD "attohttpd"
#define URL "http://www.puyan.org"

#include <arpa/inet.h>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctype.h>
#include <dirent.h>
#include <fstream>
#include <functional>
#include <inttypes.h>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <queue>
#include <sstream>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <optional>

#define CHECK(check, message)                                                  \
  do {                                                                         \
    if ((check) < 0) {                                                         \
      fprintf(stderr, "%s failed: Error on line %d.\n", (message), __LINE__);  \
      perror(message);                                                         \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (false)

namespace {

std::string get_mime_type(const char *name) {
#if !(BOOTSTRAPINTEROP)
  return main::getMimeTypeSwift(std::string(name));
#else
// Dummy Code
  return "";
#endif
}

void send_headers(int status, std::string title, std::string mime, FILE *socket,
                  off_t len = -1) {
  time_t now = time(nullptr);
  char timebuf[100];
  strftime(timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
  fprintf(socket, "%s %d %s\r\n", "HTTP/1.1", status, title.c_str());
  fprintf(socket, "Server: %s\r\n", HTTPD);
  if (mime != "")
    fprintf(socket, "Content-Type: %s\r\n", mime.c_str());
  if (len >= 0)
    fprintf(socket, "Content-Length: %lld\r\n", (long long)len);
  fprintf(socket, "Connection: close\r\n\r\n");
}

void HttpStart(int status, FILE *socket, std::string color, std::string title) {
  send_headers(status, (status == 200 ? "Ok" : title), "text/html", socket);
  fprintf(socket,
          "<html><head><title>%s</title></head><body bgcolor=%s>"
          "<h4>%s</h4><pre>",
          title.c_str(), color.c_str(), title.c_str());
}

int HttpEnd(int status, FILE *socket) {
  fprintf(socket, "</pre><a href=\"%s\">%s</a></body></html>\n", URL, HTTPD);
  return status;
}

int send_error(int stat, FILE *sock, std::string title, std::string text = "") {
  send_headers(stat, title, "text/html", sock);
  std::stringstream sstr;
  sstr << stat << " ";
  HttpStart(stat, sock, "#cc9999", sstr.str() + title);
  fprintf(sock, "%s", (text == "" ? title.c_str() : text.c_str()));
  return HttpEnd(stat, sock);
}

int doFile(const char *filename, FILE *socket) {
  std::ifstream ifs(filename, std::ios::in | std::ios::binary | std::ios::ate);
  if (!ifs.is_open())
    return send_error(403, socket, "Forbidden: Protected.");
  std::streampos pos = ifs.tellg();
  const size_t len = static_cast<std::string::size_type>(pos);
  char *buf = new char[len];
  bzero(buf, len);
  ifs.seekg(0, std::ios::beg);
  ifs.read(buf, pos);
  ifs.close();
  send_headers(200, "Ok", get_mime_type(filename), socket, len);
  fwrite(buf, sizeof(char), len, socket);
  fflush(socket);
  delete[] buf;
  return 200;
}

int http_proto(FILE *socket, const char *request) {
  char method[100], path[1000], protocol[100];

  if (!request || strlen(request) < strlen("GET / HTTP/1.1"))
    return send_error(403, socket, "Bad Request", "No request found.");
  if (sscanf(request, "%[^ ] %[^ ] %[^ ]", method, path, protocol) != 3)
    return send_error(400, socket, "Bad Request", "Can't parse request.");
  if (strncasecmp(method, "get", 3))
    return send_error(501, socket, "Not Impl");
  if (path[0] != '/')
    return send_error(400, socket, "Bad Filename");

  std::string fileStr = &(path[1]);
  if (fileStr.c_str()[0] == '\0')
    fileStr = "./";
  const char *file = fileStr.c_str();
  size_t len = strlen(file);
  struct stat sb;

  if (file[0] == '/' || !strncmp(file, "..", 2) || !strncmp(file, "../", 3) ||
      strstr(file, "/../") != (char *)0 || !strncmp(&(file[len - 3]), "/..", 3))
    return send_error(400, socket, "Bad Request", "Illegal filename.");
  if (stat(file, &sb) < 0)
    return send_error(404, socket, "File Not Found");

  std::string dir = std::string(file) + ((file[len - 1] != '/') ? "/" : "");
  fileStr = S_ISDIR(sb.st_mode) ? (dir + "index.html") : file;

  if (!S_ISDIR(sb.st_mode) || (stat(fileStr.c_str(), &sb) >= 0))
    return doFile(fileStr.c_str(), socket);

  HttpStart(200, socket, "lightblue", std::string("Index of ") + dir);
  struct dirent **dl;
  for (int i = 0, n = scandir(dir.c_str(), &dl, NULL, alphasort); i != n; ++i) {
    fprintf(socket, "<a href=\"%s\">%s</a>\n", dl[i]->d_name, dl[i]->d_name);
    free(dl[i]);
  }
  free(dl);
  return HttpEnd(200, socket);
}
}

int HttpProto(int socket) {
  std::string requestStr("");
  for (char buffer[BUFFERLEN];;) {
    bzero(buffer, BUFFERLEN);
    int newBytes = read(socket, buffer, BUFFERLEN);
    requestStr += (newBytes > 0) ? std::string(buffer) : "";
    if (newBytes < BUFFERLEN)
      break;
  }

  FILE *socketFile = fdopen(socket, "w");
  http_proto(socketFile, requestStr.c_str());
  fclose(socketFile);
  return socket;
}

int ContructTCPSocket(uint16_t portNumber) {
  int ssock;                                 // Server Socket
  int O = REUSE_PORT;                        // reuse port sockopt
  struct sockaddr_in saddr;                  // Server Socket addr
  bzero(&saddr, sizeof(saddr));              // Zero serve  struct
  saddr.sin_family = AF_INET;                // Inet addr family
  saddr.sin_addr.s_addr = htonl(INADDR_ANY); // Any incoming iface
  saddr.sin_port = htons(portNumber);        // Local (server) port
  CHECK(ssock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP), "socket()");
  CHECK(setsockopt(ssock, SOL_SOCKET, SO_REUSEADDR, &O, sizeof(O)), "opt");
  CHECK(bind(ssock, (struct sockaddr *)&saddr, sizeof(saddr)), "bind()");
  CHECK(listen(ssock, MAXPENDING), "listen()");
  return ssock;
}

// auto AcceptConnection(int ssock) -> std::optional<int> {
auto AcceptConnection(int ssock) -> int {
  struct sockaddr_in addr;
  unsigned len = sizeof(addr);
  int clientSocket = accept(ssock, (struct sockaddr *)&addr, &len);
  printf("Handling client %s\n", inet_ntoa(addr.sin_addr));

  if (clientSocket < 0) {
    // return {};
    return -1;
  }

  return clientSocket;
}
