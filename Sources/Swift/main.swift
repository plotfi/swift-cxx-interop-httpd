import Foundation
import CxxStdlib
import CXX

var LOAD_DIR = "./"
print("* Beginning Server, Root Directory: \(LOAD_DIR)\n")
FileManager.default.changeCurrentDirectoryPath(LOAD_DIR)

// Sets up networking sockets used by Producer thread to AcceptConnection
let ServerSocket = ContructTCPSocket(1337)
defer { close(ServerSocket); }

var requestId: Int32 = 0

// Grab the client socket requests and process them async
while true {
    if #available(macOS 10.15, *) {
      // let ClientSocketOpt: CxxOptional<Int32> = AcceptConnection(ServerSocket)
      let ClientSocketOpt = AcceptConnection(ServerSocket)

      print("await request \(requestId)")
      requestId += 1

      // if let sock = ClientSocketOpt.value {
      if ClientSocketOpt > 0 {
        let ClientSocket = ClientSocketOpt

        Task { await HandleRequest(ClientSocket, requestId) }
      } else {
        print("Failed to get client socket...")
        print("Goodbye...")
        break
      }
    } else {
      // Fallback on earlier versions
      print("swift async/await and requires macOS 10.15 or later.")
      print("Goodbye...")
      break
    } 
}

@available(macOS 10.15.0, *)
func HandleRequest(_ ClientSocket : CInt, _ ID: CInt) async {
  var request: cxx_std_vector_of_int = cxx_std_vector_of_int()
  request.push_back(ClientSocket)
  request.push_back(ID)

  let status = HttpProto(request)
  print("Handled request \(ID) with status \(status)")
  close(ClientSocket)
}


public func getMimeTypeSwift(Name: std.string) -> std.string {
  let NameStr = String(Name)

  var dot = ""
  if let ExtensionIndex = NameStr.lastIndex(of: ".") {
    dot = String(NameStr[ExtensionIndex...])
  }

  switch dot {
  case ".html":
    return "text/html; charset=iso-8859-1"
  case ".midi":
    return "audio/midi"
  case ".jpg":
    return "image/jpeg"
  case ".jpeg":
    return "image/jpeg"
  case ".mpeg":
    return "video/mpeg"
  case ".gif":
    return "image/gif"
  case ".png":
    return "image/png"
  case ".css":
    return "text/css"
  case ".au":
    return "audio/basic"
  case ".wav":
    return "audio/wav"
  case ".avi":
    return "video/x-msvideo"
  case ".mov":
    return "video/quicktime"
  case ".mp3":
    return "audio/mpeg"
  case ".m4a":
    return "audio/mp4"
  case ".pdf":
    return "application/pdf"
  case ".ogg":
    return "application/ogg"
  default:
    return "text/plain; charset=iso-8859-1"
  }
}

