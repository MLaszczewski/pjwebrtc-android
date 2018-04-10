package io.experty.pjwebrtc;

import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Created by m8 on 09/04/2018.
 */

public class PeerConnection {

  private final int instanceId;

  public ResultCallback<JSONObject> onIceCandidate;

  PeerConnection(int instanceId) {
    this.instanceId = instanceId;
  }

  public void addStream(UserMedia media) {
    try {
      JSONObject req = new JSONObject();
      req.put("type", "addStream");
      req.put("peerConnectionId", instanceId);
      req.put("userMediaId", media.instanceId);
      PjWebRTC.request(req, new ResultCallback<JSONObject>() {
        @Override
        public void onResult(JSONObject result) {
        }
      });
    } catch (JSONException e) {
      e.printStackTrace();
    }
  }

  public void setLocalDescription(JSONObject sdp, final ResultCallback<Boolean> doneCb) {
    try {
      JSONObject req = new JSONObject();
      req.put("type", "setLocalDescription");
      req.put("peerConnectionId", instanceId);
      req.put("sdp", sdp);
      PjWebRTC.request(req, new ResultCallback<JSONObject>() {
        @Override
        public void onResult(JSONObject result) {
          doneCb.onResult(true);
        }
      });
    } catch (JSONException e) {
      e.printStackTrace();
    }
  }

  public void setRemoteDescription(JSONObject sdp, final ResultCallback<Boolean> doneCb) {
    try {
      JSONObject req = new JSONObject();
      req.put("type", "setRemoteDescription");
      req.put("peerConnectionId", instanceId);
      req.put("sdp", sdp);
      PjWebRTC.request(req, new ResultCallback<JSONObject>() {
        @Override
        public void onResult(JSONObject result) {
          doneCb.onResult(true);
        }
      });
    } catch (JSONException e) {
      e.printStackTrace();
    }
  }

  public void addIceCandidate(JSONObject ice, final ResultCallback<Boolean> doneCb) {
    try {
      JSONObject req = new JSONObject();
      req.put("type", "addIceCandidate");
      req.put("peerConnectionId", instanceId);
      req.put("ice", ice != null ? ice : JSONObject.NULL);
      PjWebRTC.request(req, new ResultCallback<JSONObject>() {
        @Override
        public void onResult(JSONObject result) {
          doneCb.onResult(true);
        }
      });
    } catch (JSONException e) {
      e.printStackTrace();
    }
  }

  public void createOffer(final ResultCallback<JSONObject> offerCb) {
    try {
      JSONObject req = new JSONObject();
      req.put("type", "createOffer");
      req.put("peerConnectionId", instanceId);
      PjWebRTC.request(req, new ResultCallback<JSONObject>() {
        @Override
        public void onResult(JSONObject result) {
          try {
            offerCb.onResult(result.getJSONObject("sdp"));
          } catch (JSONException e) {
            e.printStackTrace();
          }
        }
      });
    } catch (JSONException e) {
      e.printStackTrace();
    }
  }

  public void createAnswer(final ResultCallback<JSONObject> answerCb) {
    try {
      JSONObject req = new JSONObject();
      req.put("type", "createAnswer");
      req.put("peerConnectionId", instanceId);
      PjWebRTC.request(req, new ResultCallback<JSONObject>() {
        @Override
        public void onResult(JSONObject result) {
          try {
            answerCb.onResult(result.getJSONObject("sdp"));
          } catch (JSONException e) {
            e.printStackTrace();
          }
        }
      });
    } catch (JSONException e) {
      e.printStackTrace();
    }
  }

  public void handleMessage(JSONObject message) {
    try {
      String messageType = message.getString("type");
      if(messageType.equals("iceCandidate")) {
        onIceCandidate.onResult(message.getJSONObject("candidate"));
      }
    } catch (JSONException e) {
      e.printStackTrace();
    }

  }
}
