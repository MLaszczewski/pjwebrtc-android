package io.experty.pjwebrtc;

import android.text.TextUtils;
import android.util.ArrayMap;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.concurrent.BlockingDeque;
import java.util.function.Consumer;

/**
 * Created by m8 on 05/04/2018.
 */

public class PjWebRTC {

  public static native void init();
  public static native void pushMessage(String json);
  public static native String getNextMessage(long wait);

  public static Thread readerThread;
  public static Thread writerThread;

  public static boolean initialized;
  public static boolean finished;

  public static int lastRequestId;

  public static ArrayMap<Integer, ResultCallback<JSONObject>> waitingRequests;

  public static ArrayMap<Integer, UserMedia> userMedia;
  public static ArrayMap<Integer, PeerConnection> peerConnection;

  public static BlockingDeque<String> sendQueue;

  static {
    initialized = false;
    System.loadLibrary("pjwebrtc");
    readerThread = new Thread() {
      public void run() {
        PjWebRTC.init();
        initialized = true;
        while(!finished) {
          String strMsg = PjWebRTC.getNextMessage(100);
          if(TextUtils.isEmpty(strMsg)) continue;
          Log.d("PJWEBRTC", "MESSAGE:\n"+strMsg);
          try {
            JSONObject msg = new JSONObject(strMsg);
            if(msg.has("responseId")) {
              int id = msg.getInt("responseId");
              waitingRequests.get(id).onResult(msg);
              waitingRequests.remove(id);
            } else if(msg.has("peerConnectionId")){
              int id = msg.getInt("peerConnectionId");
              peerConnection.get(id).handleMessage(msg);
            } else if(msg.has("userMediaId")){
              int id = msg.getInt("userMediaId");
              userMedia.get(id).handleMessage(msg);
            }
          } catch (JSONException e) {
            e.printStackTrace();
          }
        }
      }
    };
    readerThread.start();

    writerThread = new Thread() {
      public void run() {
        while(!initialized) try {
          Thread.sleep(10);
        } catch (InterruptedException e) {
          e.printStackTrace();
        }
        while(!finished) {
          try {
            PjWebRTC.pushMessage(sendQueue.takeFirst());
          } catch (InterruptedException e) {
            e.printStackTrace();
          }
        }
      }
    };
  }


  public static void request(JSONObject req, ResultCallback<JSONObject> cb) {
    int requestId = ++lastRequestId;
    waitingRequests.put(requestId, cb);
    try {
      req.put("requestId", requestId);
      String strMsg = req.toString(2);
      Log.d("PJWEBRTC", "REQUEST:\n"+strMsg);
      sendQueue.addLast(strMsg);
    } catch (JSONException e) {
      waitingRequests.remove(requestId);
      e.printStackTrace();
    }
  }

  public static void getUserMedia(JSONObject constraints, final ResultCallback<UserMedia> cb) {
    try {
      JSONObject req = new JSONObject();
      req.put("type", "getUserMedia");
      req.put("constraints", constraints);
      request(req, new ResultCallback<JSONObject>() {
        @Override
        public void onResult(JSONObject result) {
          try {
            int id = result.getInt("userMediaId");
            UserMedia um = new UserMedia(id);
            userMedia.put(id, um);
            cb.onResult(um);
          } catch (JSONException e) {
            e.printStackTrace();
          }
        }
      });
    } catch (JSONException e) {
      e.printStackTrace();
    }
  }

  public static void createPeerConnection(JSONObject config, final ResultCallback<PeerConnection> cb) {
    try {
      JSONObject req = new JSONObject();
      req.put("type", "createPeerConnection");
      req.put("config", config);
      request(req, new ResultCallback<JSONObject>() {
        @Override
        public void onResult(JSONObject result) {
          try {
            int id = result.getInt("userMediaId");
            PeerConnection pc = new PeerConnection(id);
            peerConnection.put(id, pc);
            cb.onResult(pc);
          } catch (JSONException e) {
            e.printStackTrace();
          }
        }
      });
    } catch (JSONException e) {
      e.printStackTrace();
    }
  }

}
