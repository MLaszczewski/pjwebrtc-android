package io.experty.pjwebrtc;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Created by m8 on 09/04/2018.
 */

public class PeerConnection {

  private final int instanceId;

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

  public void handleMessage(JSONObject message) {

  }
}
