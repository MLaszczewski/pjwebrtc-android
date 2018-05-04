#include <jni.h>
#include <string>

#include <concurrentqueue.h>
#include "global.h"
#include "PeerConnection.h"
#include <json.hpp>
#include <chrono>

#include <android/log.h>

#define TAG "NATIVE"

#include <pthread.h>
#include <unistd.h>

static int pfd[2];
static pthread_t loggingThread;

void registerThisThread(const char* name) {
  pj_thread_t* wsThread = nullptr;
  pj_thread_desc wsThreadDesc;
  if(!pj_thread_is_registered()) pj_thread_register(name, wsThreadDesc, &wsThread);
}

static void *loggingFunction(void*) {
  ssize_t readSize;
  char buf[128];

  while((readSize = read(pfd[0], buf, sizeof buf - 1)) > 0) {
    if(buf[readSize - 1] == '\n') {
      --readSize;
    }

    buf[readSize] = 0;  // add null-terminator

    __android_log_write(ANDROID_LOG_DEBUG, TAG, buf); // Set any log level you want
  }

  return 0;
}
static int runLoggingThread() { // run this function to redirect your output to android log
  setvbuf(stdout, 0, _IOLBF, 0); // make stdout line-buffered
  setvbuf(stderr, 0, _IONBF, 0); // make stderr unbuffered

  /* create the pipe and redirect stdout and stderr */
  pipe(pfd);
  dup2(pfd[1], 1);
  dup2(pfd[1], 2);

  /* spawn the logging thread */
  if(pthread_create(&loggingThread, 0, loggingFunction, 0) == -1) {
    return -1;
  }

  pthread_detach(loggingThread);

  return 0;
}


static pthread_t pollingThread;
static pthread_t timerThread;

static void* pollingFunction(void*) {
  registerThisThread("ioqueue_poll");
  while(true) {
    const pj_time_val delay = {.sec = 0, .msec = 10};
    pj_ioqueue_poll(webrtc::ioqueue, &delay);
  }
  return 0;
}
static void* timerFunction(void*) {
  registerThisThread("timer_poll");
  while(true) {
    pj_timer_heap_poll(webrtc::timerHeap, nullptr);
    std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
  }
  return 0;
}
static int runPollingThreads() {
  if(pthread_create(&pollingThread, 0, pollingFunction, 0) == -1) {
    return -1;
  }
  pthread_detach(pollingThread);

  if(pthread_create(&timerThread, 0, timerFunction, 0) == -1) {
    return -1;
  }
  pthread_detach(timerThread);

  return 0;
}

moodycamel::ConcurrentQueue<std::string> messages;

std::map<int, std::shared_ptr<webrtc::PeerConnection>> peerConnections;
int lastPeerConnectionId = 0;

std::map<int, std::shared_ptr<webrtc::UserMedia>> userMedias;
int lastUserMediaId = 0;

extern "C"
JNIEXPORT void JNICALL
Java_io_experty_pjwebrtc_PjWebRTC_init(JNIEnv *env, jclass type) {
  webrtc::init();
  registerThisThread("jni_init");
  runLoggingThread();
  runPollingThreads();
}

extern "C"
JNIEXPORT void JNICALL
Java_io_experty_pjwebrtc_PjWebRTC_pushMessage(JNIEnv *env, jobject instance, jstring json_) {
  registerThisThread("jni_push");
  const char *json = env->GetStringUTFChars(json_, 0);

  __android_log_print(ANDROID_LOG_VERBOSE, TAG, "PM %s", json);

  auto message = nlohmann::json::parse(json);
  auto messageType = message["type"];

  if(messageType == "getUserMedia") {
    webrtc::UserMediaConstraints constraints;
    /// TODO: read constraints
    int id = ++lastUserMediaId;
    userMedias[id] = webrtc::UserMedia::getUserMedia(constraints);
    nlohmann::json msg = { {"type", "gotUserMedia"}, {"userMediaId", id},
                           {"responseId", message["requestId"]} };
    messages.enqueue(msg.dump(2));
  } else
  if(messageType == "createPeerConnection") {;
    int id = ++lastPeerConnectionId;
    webrtc::PeerConnectionConfiguration pcConfig;
    pcConfig.iceServers = message["config"]["iceServers"];
    auto peerConnection = std::make_shared<webrtc::PeerConnection>();

    peerConnection->onConnectionStateChange = [id](std::string state){
      nlohmann::json msg = { {"type", "connectionStateChange"}, {"peerConnectionId", id},
                             {"connectionState", state} };
      messages.enqueue(msg.dump(2));
    };
    peerConnection->onIceConnectionStateChange = [id](std::string state){
      nlohmann::json msg = { {"type", "iceConnectionStateChange"}, {"peerConnectionId", id},
                             {"iceConnectionState", state} };
      messages.enqueue(msg.dump(2));
    };
    peerConnection->onIceGatheringStateChange = [id](std::string state){
      nlohmann::json msg = { {"type", "iceGatheringStateChange"}, {"peerConnectionId", id},
                             {"iceGatheringState", state} };
      messages.enqueue(msg.dump(2));
    };
    peerConnection->onSignalingStateChange = [id](std::string state) {
      nlohmann::json msg = { {"type", "signalingStateChange"}, {"peerConnectionId", id},
                             {"signalingState", state} };
      messages.enqueue(msg.dump(2));
    };

    peerConnection->init(pcConfig);

    peerConnections[id] = peerConnection;
    nlohmann::json msg = { {"type", "createdPeerConnection"}, {"peerConnectionId", id},
                           {"responseId", message["requestId"]} };
    messages.enqueue(msg.dump(2));
  } else
  if(messageType == "addStream") {
    int peerConnectionId = message["peerConnectionId"];
    auto peerConnection = peerConnections[peerConnectionId];
    auto userMedia = userMedias[message["userMediaId"]];
    peerConnection->addStream(userMedia);
    nlohmann::json msg = { {"type", "streamAdded"}, {"peerConnectionId", peerConnectionId},
                           {"responseId", message["requestId"]} };
    messages.enqueue(msg.dump(2));
  } else
  if(messageType == "setLocalDescription") {
    int peerConnectionId = message["peerConnectionId"];
    auto peerConnection = peerConnections[peerConnectionId];
    peerConnection->setLocalDescription(message["sdp"]);
    nlohmann::json msg = { {"type", "localDescriptionSet"}, {"peerConnectionId", peerConnectionId},
                           {"responseId", message["requestId"]} };
    messages.enqueue(msg.dump(2));
    peerConnection->iceCompletePromise->onResolved([peerConnection, peerConnectionId](bool& v) {
      printf("ICE GATHERING COMPLETE! - OUTPUTING CANDIDATES!\n");
      for (auto &candidate : peerConnection->localCandidates) {
        nlohmann::json msg = {{"type",             "iceCandidate"},
                              {"candidate",        candidate},
                              {"peerConnectionId", peerConnectionId}};
        messages.enqueue(msg.dump(2));
      }
      nlohmann::json msg = {{"type",             "iceCandidate"},
                            {"candidate",        nullptr},
                            {"peerConnectionId", peerConnectionId}};
      messages.enqueue(msg.dump(2));
    });
  } else
  if(messageType == "setRemoteDescription") {
    int peerConnectionId = message["peerConnectionId"];
    auto peerConnection = peerConnections[peerConnectionId];
    peerConnection->setRemoteDescription(message["sdp"]);
    nlohmann::json msg = { {"type", "remoteDescriptionSet"}, {"peerConnectionId", peerConnectionId},
                           {"responseId", message["requestId"]} };
    messages.enqueue(msg.dump(2));
  } else
  if(messageType == "createOffer") {
    int peerConnectionId = message["peerConnectionId"];
    auto peerConnection = peerConnections[peerConnectionId];
    peerConnection->createOffer()->onResolved([=](nlohmann::json offer) {
      nlohmann::json msg = { {"type", "createdOffer"}, {"sdp", offer},
                             {"peerConnectionId", peerConnectionId},
                             {"responseId", message["requestId"]} };
      messages.enqueue(msg.dump(2));
    });
  } else
  if(messageType == "createAnswer") {
    printf("CREATE ANSWER!!!!!!\n");
    int peerConnectionId = message["peerConnectionId"];
    auto peerConnection = peerConnections[peerConnectionId];
    peerConnection->createAnswer()->onResolved([=](nlohmann::json answer) {
      nlohmann::json msg = { {"type", "createdAnswer"}, {"sdp", answer},
                             {"peerConnectionId", peerConnectionId},
                             {"responseId", message["requestId"]} };
      messages.enqueue(msg.dump(2));
    });
  } else if(messageType == "addIceCandidate") {
    int peerConnectionId = message["peerConnectionId"];
    auto peerConnection = peerConnections[peerConnectionId];
    peerConnection->addIceCandidate(message["ice"]);
    nlohmann::json msg = { {"type", "iceCandidateAdded"},
                           {"peerConnectionId", peerConnectionId},
                           {"responseId", message["requestId"]} };
    messages.enqueue(msg.dump(2));
  } else if(messageType == "close") {
    int peerConnectionId = message["peerConnectionId"];
    auto peerConnection = peerConnections[peerConnectionId];
    peerConnection->close();
    nlohmann::json msg = { {"type", "closed"}, {"peerConnectionId", peerConnectionId},
                           {"responseId", message["requestId"]} };
    messages.enqueue(msg.dump(2));
  } else if(messageType == "delete") {
    int peerConnectionId = message["peerConnectionId"];
    peerConnections.erase(peerConnectionId);
  }

  env->ReleaseStringUTFChars(json_, json);
}

extern "C"
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
    std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
    long now = std::chrono::duration_cast< std::chrono::milliseconds >(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    if(now > readEnd) break;
  }

  return nullptr;
}
