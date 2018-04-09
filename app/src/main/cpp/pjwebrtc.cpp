#include <jni.h>
#include <string>

#include <concurrentqueue.h>
#include "global.h"
#include "PeerConnection.h"
#include <json.hpp>
#include <chrono>

moodycamel::ConcurrentQueue<std::string> messages;
std::shared_ptr<webrtc::PeerConnection> peerConnection;

std::shared_ptr<webrtc::UserMedia> userMedia;

void registerThisThread(const char* name) {
  pj_thread_t* wsThread = nullptr;
  pj_thread_desc wsThreadDesc;
  if(!pj_thread_is_registered()) pj_thread_register(name, wsThreadDesc, &wsThread);
}

extern "C"
JNIEXPORT void JNICALL
Java_io_experty_pjwebrtc_PjWebRTC_init(JNIEnv *env, jclass type) {
  webrtc::init();
  registerThisThread("jni_init");
}
extern "C"
JNIEXPORT void JNICALL
Java_io_experty_pjwebrtc_PjWebRTC_pushMessage(JNIEnv *env, jobject instance, jstring json_) {
  registerThisThread("jni_push");
  const char *json = env->GetStringUTFChars(json_, 0);

  auto message = nlohmann::json::parse(json);
  auto messageType = message["type"];

  if(messageType == "getUserMedia") {
    webrtc::UserMediaConstraints constraints;
    /// TODO: read constraints
    userMedia = webrtc::UserMedia::getUserMedia(constraints);
    nlohmann::json msg = { {"type", "gotUserMedia"}, {"userMediaId", 0},
                           {"responseId", message["requestId"]} };
    messages.enqueue(msg.dump(2));
  } else
  if(messageType == "createPeerConnection") {
    webrtc::PeerConnectionConfiguration pcConfig;
    /// TODO: read pc config
    peerConnection = std::make_shared<webrtc::PeerConnection>(pcConfig);
    nlohmann::json msg = { {"type", "createdPeerConnection"}, {"peerConnectionId", 0},
                           {"responseId", message["requestId"]} };
    messages.enqueue(msg.dump(2));
  } else
  if(messageType == "addStream") {
    peerConnection->addStream(userMedia);
    nlohmann::json msg = { {"type", "streamAdded"}, {"peerConnectionId", 0},
                           {"responseId", message["requestId"]} };
    messages.enqueue(msg.dump(2));
  } else
  if(messageType == "setLocalDescription") {
    peerConnection->setLocalDescription(message["sdp"]);
    for (auto &candidate : peerConnection->localCandidates) {
      nlohmann::json msg = { {"type", "iceCandidate"}, {"candidate", candidate},
                             {"peerConnectionId", 0} };
      messages.enqueue(msg.dump(2));
    }
  } else
  if(messageType == "setRemoteDescription") {
    peerConnection->setRemoteDescription(message["sdp"]);
  } else
  if(messageType == "createOffer") {
    peerConnection->createOffer()->onResolved([=](nlohmann::json offer) {
      nlohmann::json msg = { {"type", "createdOffer"}, {"sdp", offer}, {"peerConnectionId", 0},
                             {"responseId", message["requestId"]} };
      messages.enqueue(msg.dump(2));
    });
  } else
  if(messageType == "createAnswer") {
    peerConnection->createAnswer(message["sdp"])->onResolved([=](nlohmann::json offer) {
      nlohmann::json msg = { {"type", "createdOffer"}, {"sdp", offer}, {"peerConnectionId", 0},
                             {"responseId", message["requestId"]} };
      messages.enqueue(msg.dump(2));
    });
  } else {
    if(messageType == "addIceCandidate") {
      peerConnection->addIceCandidate(message["ice"]);
    }
  }


env->ReleaseStringUTFChars(json_, json);
}extern "C"
JNIEXPORT jstring JNICALL
Java_io_experty_pjwebrtc_PjWebRTC_getNextMessage(JNIEnv *env, jobject instance, jlong wait) {
  registerThisThread("jni_read");
  long readEnd = std::chrono::duration_cast< std::chrono::milliseconds >(
      std::chrono::system_clock::now().time_since_epoch()
  ).count() + wait;

  while (true) {
    std::string message;
    if(messages.try_dequeue(message)) {
      return env->NewStringUTF(message.c_str());
    }
    if(peerConnection) {
      const pj_time_val delay = {0, 10};
      pj_timer_heap_poll(peerConnection->timerHeap, nullptr);
      pj_ioqueue_poll(peerConnection->ioqueue, &delay);
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    long now = std::chrono::duration_cast< std::chrono::milliseconds >(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    if(now > readEnd) break;
  }

  return nullptr;
}