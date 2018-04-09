#include "../include/WebSocket.h"

#ifdef EMSCRIPTEN
  #include <emscripten.h>
  #define wsxx_log(...) emscripten_log(EM_LOG_ERROR, __VA_ARGS__)
#endif

#ifdef EMSCRIPTEN

namespace wsxx {

unsigned int WebSocket::lastSocketId = 0;

WebSocket::WebSocket(std::string url, openCallback onOpenp, messageCallback onMessagep, closeCallback onClosep)
  : onMessage(onMessagep), onOpen(onOpenp), onClose(onClosep) {
   socketId = ++lastSocketId;
  EM_ASM_ARGS({
    var cxx = $2;
    var urlPointer = $0;
    var url = Module.Pointer_stringify( urlPointer );
    var ws = new WebSocket( url );
    ws.binaryType='arraybuffer';

    ws.onopen = function() {
      Module._wsxx_handle_open(cxx);
    };
    ws.onclose = function(ev) {
      var reason = Module.allocate(Module.intArrayFromString(ev.reason), 'i8', Module.ALLOC_NORMAL);
      Module._wsxx_handle_close(cxx, ev.code, reason, ev.wasClean);
      Module._free(reason);
    };
    ws.onerror = function() {
      Module._wsxx_handle_error(cxx);
    };
    ws.onmessage = function(ev) {
      if(ev.data instanceof ArrayBuffer) {
        /// TODO: persistent buffer optimization
        var size = ev.data.byteLength;
        var src = new Uint8Array(ev.data);
        var destPtr = Module._malloc(size);
        var dest = new Uint8Array(Module.HEAPU8.buffer, destPtr, size);
        dest.set(src);
        try {
          Module._wsxx_handle_binary_message(cxx, destPtr, size);
        } catch(e) {
          console.error(e);
          console.error(e.stack);
        }
        Module._free(destPtr);
      } else {
        var text = Module.allocate(Module.intArrayFromString(ev.data), 'i8', Module.ALLOC_NORMAL);
        try {
          Module._wsxx_handle_text_message(cxx, text);
        } catch(e) {
          console.error(e);
          console.error(e.stack);
        }
        Module._free(text);
      }
    };

    window.WSXX = window.WSXX || [];
    window.WSXX[$1] = ws; //*/
  }, url.c_str(), socketId, this);
}
WebSocket::WebSocket(std::string url) {
  WebSocket(url,nullptr,nullptr,nullptr);
}
WebSocket::~WebSocket() {
  closeConnection();
  EM_ASM_ARGS({
    delete window.WSXX[$0]
  }, socketId);
}
void WebSocket::closeConnection(unsigned short code, std::string reason) {
  EM_ASM_ARGS({
    window.WSXX[$0].close($1,Pointer_stringify($2))
  }, socketId, code, reason.c_str());
}
void WebSocket::closeConnection() {
  closeConnection(1000, "");
}
void WebSocket::send(std::string data, PacketType type) {
  if(type == PacketType::Text) {
    EM_ASM_ARGS({
      window.WSXX[$0].send(Pointer_stringify($1));
    }, socketId, data.c_str());
  }
  if(type == PacketType::Binary) {
    EM_ASM_ARGS({
      var buff = new Uint8Array($1);
      var heapData = new Uint8Array(Module.HEAPU8.buffer, $2, $1);
      buff.set(heapData);
      window.WSXX[$0].send(buff.buffer);
    }, socketId, data.size(), data.c_str());
  }
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
  return (WebSocket::State)EM_ASM_INT({
    return window.WSXX[$0].readyState
  }, socketId);
}
unsigned int WebSocket::bufferedAmount() {
  return (unsigned int)EM_ASM_INT({
    return window.WSXX[$0].bufferedAmount
  }, socketId);
}

}

extern "C" {

EMSCRIPTEN_KEEPALIVE
void wsxx_handle_open(wsxx::WebSocket* ws) {
  if(ws->onOpen) ws->onOpen();
}

EMSCRIPTEN_KEEPALIVE
void wsxx_handle_close(wsxx::WebSocket* ws, int code, const char* reason, bool wasClean) {
  if(ws->onClose) ws->onClose(code,std::string(reason),wasClean);
}

EMSCRIPTEN_KEEPALIVE
void wsxx_handle_error(wsxx::WebSocket* ws) {
  if(ws->onError) ws->onError();
}

EMSCRIPTEN_KEEPALIVE
void wsxx_handle_binary_message(wsxx::WebSocket* ws, char* buffer, int length) {
  if(ws->onMessage) ws->onMessage(std::string(buffer,length), wsxx::WebSocket::PacketType::Binary);
}

EMSCRIPTEN_KEEPALIVE
void wsxx_handle_text_message(wsxx::WebSocket* ws, char* text) {
  if(ws->onMessage) ws->onMessage(std::string(text), wsxx::WebSocket::PacketType::Text);
}

}


#endif
