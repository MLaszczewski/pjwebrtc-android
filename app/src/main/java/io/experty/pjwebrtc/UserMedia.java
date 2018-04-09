package io.experty.pjwebrtc;

import org.json.JSONObject;

/**
 * Created by m8 on 09/04/2018.
 */

public class UserMedia {

  public final int instanceId;

  public UserMedia(int instanceId) {
    this.instanceId = instanceId;
  }

  public void handleMessage(JSONObject message) { /* no messages for user media */ }



}
