#ifndef CXX_H
#define CXX_H

#include <vector>
#include <optional>

using cxx_std_vector_of_int = std::vector<int>;

int ContructTCPSocket(uint16_t portNumber);
int HttpProto(int socket);
// auto AcceptConnection(int ssock) -> std::optional<int>;
auto AcceptConnection(int ssock) -> int;

#endif
