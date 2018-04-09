#include "../include/WebSocket.h"

#ifndef EMSCRIPTEN

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sstream>
#include <algorithm>
#include <unistd.h>

#ifndef __ANDROID
#include <stdio.h>
#include <arpa/inet.h>

#define wsxx_log(...) printf(__VA_ARGS__)
#endif

#ifdef __ANDROID
#define htonll(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#define ntohll(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))

#include <android/log.h>
#define wsxx_log(...) __android_log_print(ANDROID_LOG_INFO, "WSXX", __VA_ARGS__);
#endif

namespace wsxx {

  class ConnectionError : public std::exception {
  };

  void WebSocket::handleMessage(WebSocket::PacketType opcode, bool fin, const char* data, int dataSize) {
    if(opcode == PacketType::Ping) {
      Packet pong;
      pong.data = std::string(data, dataSize);
      pong.type = PacketType::Pong;
      queue.enqueue(pong);
      return;
    }
    if(opcode == PacketType::Pong) {
      /// ignore
      return;
    }
    if(opcode == PacketType::Close) {
      wsxx_log("Disconnected by server close request\n");
      state = State::Closed;
      if(onClose) onClose(0, "closed", true);
      close(sock);
      return;
    }
    if(waitingForMore) {
      if(opcode == PacketType::Continuation || opcode == PacketType::Binary || opcode == PacketType::Text) {
        concatBuffer.write(data, dataSize);
        if(fin) {
          waitingForMore = false;
          concatBuffer.write(data, dataSize);
          if(onMessage) onMessage(concatBuffer.str(), opcode);
          concatBuffer.str("");
        }
        return;
      }
      wsxx_log("Wrong opcode %d", (int)opcode);
      throw ConnectionError();
    } else {
      if(opcode == PacketType::Binary || opcode == PacketType::Text) {
        if(fin) {
          if(onMessage) onMessage(std::string(data, dataSize), opcode);
        } else {
          concatBuffer.write(data, dataSize);
          waitingForMore = true;
        }
        return;
      }
      wsxx_log("Wrong opcode %d", (int)opcode);
      throw ConnectionError();
    }
  }

  void WebSocket::sendData(const char* buffer, int length) {
    int position = 0;
    while(position < length) {
      int written = write(sock, buffer+position, length-position);
      if(written <= 0) {
        throw ConnectionError();
      }
      position+=written;
    }
  }

  std::string WebSocket::receiveHttpHeader() {
    std::ostringstream os;
    int rnCnt = 0;
    while(rnCnt<4) {
      char c;
      int r = read(sock, &c, 1);
      if(r <= 0) {
        throw ConnectionError();
      }
      if(rnCnt%2==0 && c=='\r') {
        rnCnt++;
      } else if(rnCnt%2==1 && c=='\n') {
        rnCnt++;
      } else {
        rnCnt = 0;
      }
      os.write(&c,1);
    }
    return os.str();
  }

  void WebSocket::writer(std::string url) {
    std::size_t protoSeparator = url.find("//");
    std::size_t pathSeparator = url.find("/",protoSeparator+2);
    std::string proto = url.substr(0, protoSeparator);
    std::string hostport = url.substr(protoSeparator+2, pathSeparator-protoSeparator-2);
    std::string path = url.substr(pathSeparator, url.size()-pathSeparator);

    signal(SIGPIPE, SIG_IGN);

    std::size_t portSeparator = hostport.find(":");
    std::string hostname;
    unsigned short port = 80;
    if(portSeparator == std::string::npos) {
      hostname = hostport;
    } else {
      hostname = hostport.substr(0,portSeparator);
      port = atoi(hostport.substr(portSeparator+1, hostport.size()-portSeparator-1).c_str());
    }

    if(proto != "ws:") {
      wsxx_log("Protocol %s is not supported\n", proto.c_str());
      state = State::Closed;
      if(onError) onError();
      if(onClose) onClose(0,"protocol not supported",false);
      return;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memset(&(server_addr.sin_zero), '\0', 8);

    in_addr& ipv4addr = server_addr.sin_addr;
    ipv4addr.s_addr = inet_addr(hostname.c_str());

    if(ipv4addr.s_addr == INADDR_NONE) { // search dns
      hostent *he = gethostbyname(hostname.c_str());
      if(he == NULL) {
        wsxx_log("Host %s not found\n", hostname.c_str());
        state = State::Closed;
        if(onError) onError();
        if(onClose) onClose(0, "host not found",false);
        return;
      }
      ipv4addr = *((struct in_addr *)he->h_addr);
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
      wsxx_log("Could not create socket!?\n");
      state = State::Closed;
      if(onError) onError();
      if(onClose) onClose(0, "could not create socket",false);
      return;
    }

    if(connect(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
      wsxx_log("Could not connect to %s : %d\n", hostname.c_str(), port);
      state = State::Closed;
      if(onError) onError();
      if(onClose) onClose(0,"could not connect",false);
      return;
    }
    wsxx_log("Connected\n");

    if(finished) return;

    std::string request =
        "GET "+path+" HTTP/1.1\r\n"+
        "Host: "+hostport+"\r\n"+
        "Upgrade: websocket\r\n"+
        "Connection: Upgrade\r\n"+
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"+
        "Sec-WebSocket-Version: 13\r\n\r\n";
    wsxx_log("SEND REQUEST:\n%s\n", request.c_str());

    try {
      sendData(request.c_str(), request.size());
      wsxx_log("WAITING FOR HTTP RESPONSE\n");
      if(finished) return;
      std::string reply = receiveHttpHeader();
      wsxx_log("Reply:\n%s\n", reply.c_str());
      std::istringstream ss(reply);
      std::string header;
      std::getline(ss, header, '\n');
      if(header != "HTTP/1.1 101 Switching Protocols\r") {
        wsxx_log("Server does not support webosockets.\n");
        throw new ConnectionError();
      }
      bool isSec = false;
      while(std::getline(ss, header, '\n')) {
        if(header=="\r") break; // finished parsing header
        size_t headerSeparator = header.find(": ");
        std::string headerName = header.substr(0, headerSeparator);
        std::string headerValue = header.substr(headerSeparator+2, header.size()-headerSeparator-3);
        std::transform(headerName.begin(), headerName.end(), headerName.begin(), ::tolower);
        //wsxx_log("H: '%s' V: '%s'\n",headerName.c_str(), headerValue.c_str());
        if(headerName == "sec-websocket-accept") {
          if(headerValue == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") isSec = true;
        }
      }
      if(!isSec) {
        wsxx_log("Server does not support webosockets\n");
        throw new ConnectionError();
      }

    } catch (ConnectionError e) {
      wsxx_log("Disconnected");
      state = State::Closed;
      if(onError) onError();
      if(onClose) onClose(0, "handshake failed", false);
      close(sock);
      return;
    }

    int flag = 1;
    int result = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));

    if(result < 0){
      wsxx_log("Can not set nodelay");
      state = State::Closed;
      if(onError) onError();
      if(onClose) onClose(0, "no nodelay", false);
      close(sock);
      return;
    }

    wsxx_log("Websocket handshake succeded!\n");
    state = State::Open;
    if(onOpen) onOpen();

    try {
      size_t bufferSize = 10240;
      unsigned char *buffer = new unsigned char[bufferSize];

      Packet packet;

      while (true) {
        if(finished) return;
        if (state != State::Open) {
          delete[] buffer;
          return;
        }

        if (!queue.try_dequeue(packet)) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
          continue;
        }

/*        wsxx_log("SEND PACKET LEN = %d, HDR: %x %x %x %x", (int)packet.data.size(),
                 packet.data.data()[0], packet.data.data()[1], packet.data.data()[2], packet.data.data()[3]);*/

        std::uint64_t dataSize = packet.data.size();
        int frameSize = 2; // header
        if (dataSize > 125) {
          frameSize += 2;
          if (dataSize > 65535) frameSize += 6;
        }
        frameSize += 4; // mask
        frameSize += dataSize;
        if (frameSize > bufferSize) {
          delete[] buffer;
          bufferSize = frameSize + 10240;
          buffer = new unsigned char[bufferSize];
        }
        unsigned char *p = buffer + 2;
        memset(buffer, 0, 2);
        buffer[0] |= 0x80; // fin
        buffer[0] |= (unsigned char)packet.type;
        buffer[1] |= 0x80; // mask
        if (dataSize <= 125) {
          buffer[1] |= dataSize;
        } else if (dataSize <= 65535) {
          buffer[1] |= 126;
          unsigned short size = htons(dataSize);
          memcpy(p, (char *) &size, 2);
          p += 2;
        } else {
          buffer[1] |= 127;
          std::uint64_t size = htonll(dataSize);
          memcpy(p, (char *) size, 8);
          p += 8;
        }

        //wsxx_log("FRAME HEADER %2x %2x\n",buffer[0], buffer[1]);

        std::uint32_t mask = rand();
        char maskBytes[4];
        memcpy(maskBytes, (char *) &mask, 4);
        memcpy(p, maskBytes, 4);
        p += 4;
        const char *data = packet.data.data();
        for (int i = 0; i < dataSize; i++) {
          *(p + i) = data[i] ^ maskBytes[i % 4];
        }

        sendData((char*)buffer, frameSize);

        bufferedBytes -= dataSize;

        //wsxx_log("FRAME SENT %d %d B0 %x\n", dataSize, frameSize, buffer[0]);
      }
    } catch (ConnectionError e) {
      wsxx_log("Disconnected\n");
      state = State::Closed;
      if(onClose) onClose(0, "closed", true);
      close(sock);
      return;
    }
  }
  void WebSocket::reader() {

    size_t bufferSize = 10240;
    char *buffer = new char[bufferSize];
    char readPhase = 0;
    char* p = buffer;
    int more = 2;

    unsigned char maskBytes[4];
    unsigned char opcode;
    bool fin;
    bool mask;

    unsigned int dataSize;

    while(true) {
      if(finished) return;
      if(state == State::Connecting) { // wait
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }
      if(state != State::Open) return;

      //wsxx_log("READER IN PHASE %d\n", readPhase);

      //wsxx_log("READ %d p %d\n", sock, more);
      int r = read(sock, p, more);

      //wsxx_log("READED %d BYTES\n", r);

      if(r <= 0) {
        wsxx_log("Disconnected %d\n", errno);
        state = State::Closed;
        if(onClose) onClose(0, "closed", true);
        close(sock);
        return;
      }

      more -= r;
      p += r;

      if(more == 0) {
        if(readPhase == 0) {
          int payloadLength = buffer[1]&0x7F;
          opcode = buffer[0]&0x0F;
          fin = buffer[0]&0x80;
          mask = buffer[1]&0x80;
          //wsxx_log("HDR Payload length %d\n", payloadLength);
          if(payloadLength == 126) {
            more = 2;
            readPhase = 1;
          } else
          if(payloadLength == 127) {
            more = 8;
            readPhase = 2;
          } else {
            dataSize = payloadLength;
            if(mask) {
              more = 4;
              readPhase = 3;
            } else {
              more = dataSize;
              readPhase = 4;
            }
          }
        } else if(readPhase == 1) {
          std::uint16_t size;
          memcpy((char*)&size, buffer, 2);
          dataSize = ntohs(size);
          if(mask) {
            more = 4;
            readPhase = 3;
          } else {
            more = dataSize;
            readPhase = 4;
          }
        } else if(readPhase == 2) {
          std::uint64_t size;
          memcpy((char*)&size, buffer, 8);
          size = ntohll(size);
          /*wsxx_log("LONG payload length %x %x %x %x %x %x %x %x = %d\n",
                   buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], (int)size);*/
          dataSize = size;
          if(mask) {
            more = 4;
            readPhase = 3;
          } else {
            more = dataSize;
            readPhase = 4;
          }
        } else if(readPhase == 3) {
          memcpy((char*)&maskBytes, buffer, 4);
          readPhase = 4;
          more = dataSize;
        } else if(readPhase == 4) {
          if(mask) {
            wsxx_log("server messages should be unmasked!");
            throw ConnectionError();
          }
          //wsxx_log("READED MESSAGE size = %d  opcode = %d  fin = %d\n", dataSize, opcode, fin);
          /*wsxx_log("DATA %x %x %x %x %x %x %x %x \n",
                   buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]);*/
          handleMessage((PacketType)opcode, fin, buffer, dataSize);
          readPhase = 0;
          more = 2;
        }
        if(more > bufferSize) {
          delete[] buffer;
          bufferSize = more + 10240;
          buffer = new char[bufferSize];
        }
        p = buffer;
      }

    }
  }
  WebSocket::WebSocket(std::string url, openCallback onOpenp, messageCallback onMessagep, closeCallback onClosep)
    : onMessage(onMessagep), onOpen(onOpenp), onClose(onClosep), state(State::Connecting),
      finished(false), waitingForMore(false),
      writerThread(&WebSocket::writer, this, url), readerThread(&WebSocket::reader, this) {
  }
  WebSocket::WebSocket(std::string url) {
    WebSocket(url,nullptr,nullptr,nullptr);
  }
  WebSocket::~WebSocket() {
    closeConnection();
    finished = true;
    readerThread.join();
    writerThread.join();
  }
  void WebSocket::closeConnection(unsigned short code, std::string reason) {
    close(sock);
    state = State::Closed;
    if(onClose) onClose(code, reason, true);
  }
  void WebSocket::closeConnection() {
    closeConnection(1000, "");
  }
  void WebSocket::send(std::string data, PacketType type) {
    Packet packet = { .data = std::move(data), .type = type };
    bufferedBytes += data.size();
    /*wsxx_log("QUEUE PACKET LEN = %d, HDR: %x %x %x %x", (int)packet.data.size(),
            packet.data.data()[0], packet.data.data()[1], packet.data.data()[2], packet.data.data()[3]);*/
    if(packet.data.data()[0] == 1 && packet.data.size()>1) throw ConnectionError();
    queue.enqueue(packet);
  }
  void WebSocket::setOnOpen(openCallback callback) {
    onOpen = callback;
  }
  void WebSocket::setOnClose(closeCallback callback) {
    onClose = callback;
  }
  void WebSocket::setOnMessage(messageCallback callback) {
    onMessage = callback;
  }
  void WebSocket::setOnError(errorCallback callback) {
    onError = callback;
  }
  WebSocket::State WebSocket::getState() {
    return state;
  }
  unsigned int WebSocket::bufferedAmount() {
    return bufferedBytes;
  }

}

#endif
