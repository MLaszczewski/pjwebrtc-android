package io.experty.pjwebrtctest;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.UUID;

import io.experty.pjwebrtc.PeerConnection;
import io.experty.pjwebrtc.PjWebRTC;
import io.experty.pjwebrtc.ResultCallback;
import io.experty.pjwebrtc.UserMedia;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;
import okhttp3.WebSocket;
import okhttp3.WebSocketListener;
import okio.ByteString;

public class MainActivity extends AppCompatActivity{

  static final String TAG = "UI";

  private final class AppWebSocketListener extends WebSocketListener {
    static final String TAG = "WS";

    private static final int NORMAL_CLOSURE_STATUS = 1000;

    private MainActivity activity;

    AppWebSocketListener(MainActivity activityp) {
      this.activity = activityp;
    }

    @Override
    public void onOpen(WebSocket webSocket, Response response) {
      Log.d(TAG, "WebSocket connected");
      //webSocket.close(NORMAL_CLOSURE_STATUS, "Goodbye !");
    }

    @Override
    public void onMessage(WebSocket webSocket, String text) {
      Log.d(TAG, "Receiving : " + text);
      try {
        JSONObject message = new JSONObject(text);
        if(message.get("uuid").equals(activity.clientUUID)) return;
        if(message.has("sdp")) {
          final JSONObject sdp = message.getJSONObject("sdp");
          activity.peerConnection.setRemoteDescription(sdp, new ResultCallback<Boolean>() {
            @Override
            public void onResult(Boolean result) {
              try {
                Log.d(TAG,"REMOTE DESCRIPTION SET!!!! SDPTYPE: "+sdp.getString("type"));
                if(sdp.getString("type").equals("offer")) {
                  Log.d(TAG,"CREATING SDP ANSWER");
                  activity.peerConnection.createAnswer(new ResultCallback<JSONObject>() {
                    @Override
                    public void onResult(JSONObject result) {
                      try {
                        final JSONObject answer = result;
                        Log.d(TAG,"got description:\n" + result.toString(2));
                        peerConnection.setLocalDescription(result,
                          new ResultCallback<Boolean>() {
                            @Override
                            public void onResult(Boolean result) {
                              try {
                                JSONObject msg = new JSONObject();
                                msg.put("sdp", answer);
                                msg.put("uuid", activity.clientUUID);
                                activity.webSocket.send(msg.toString(2));
                              } catch (JSONException e) {
                                e.printStackTrace();
                              }
                            }
                          });
                      } catch (JSONException e) {
                        e.printStackTrace();
                      }
                    }
                  });
                }
              } catch (JSONException e) {
                e.printStackTrace();
              }
            }
          });
        } else if(message.has("ice")) {
          JSONObject ice = message.isNull("ice") ? null : message.getJSONObject("ice");
          peerConnection.addIceCandidate(ice, new ResultCallback<Boolean>() {
            @Override
            public void onResult(Boolean result) {
            }
          });

        }
      } catch (JSONException e) {
        e.printStackTrace();
      }
    }

    @Override
    public void onMessage(WebSocket webSocket, ByteString bytes) {
      Log.d(TAG,"Receiving bytes : " + bytes.hex());
    }

    @Override
    public void onClosing(WebSocket webSocket, int code, String reason) {
      webSocket.close(NORMAL_CLOSURE_STATUS, null);
      Log.d(TAG,"Closing : " + code + " / " + reason);
    }

    @Override
    public void onFailure(WebSocket webSocket, Throwable t, Response response) {
      Log.d(TAG,"Error : " + t.getMessage());
    }
  }

  public UserMedia userMedia;
  public PeerConnection peerConnection;

  public TextView statusText;
  public Button joinButton;

  public OkHttpClient httpClient;
  public WebSocket webSocket;

  public boolean iAmCaller;

  public static final String clientUUID = UUID.randomUUID().toString();

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);


    statusText = findViewById(R.id.status_text);
    joinButton = findViewById(R.id.join_button);
    joinButton.setEnabled(false);
    joinButton.setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View view) {

      }
    });

    PjWebRTC.getUserMedia(new JSONObject(), new ResultCallback<UserMedia>() {
      @Override
      public void onResult(UserMedia result) {
        userMedia = result;
        tryAddStream();
      }
    });
    PjWebRTC.createPeerConnection(new JSONObject(), new ResultCallback<PeerConnection>() {
      @Override
      public void onResult(PeerConnection result) {
        peerConnection = result;
        peerConnection.onIceCandidate = new ResultCallback<JSONObject>() {
          @Override
          public void onResult(JSONObject result) {
            try {
              JSONObject msg = new JSONObject();
              msg.put("ice", result);
              webSocket.send(msg.toString(2));
            } catch (JSONException e) {
              e.printStackTrace();
            }
          }
        };
        tryAddStream();
      }
    });

    httpClient = new OkHttpClient();
    Request request = new Request.Builder().url("ws://10.0.2.2:8338/").build();
    AppWebSocketListener listener = new AppWebSocketListener(this);
    webSocket = httpClient.newWebSocket(request, listener);
    httpClient.dispatcher().executorService().shutdown();
  }

  protected void tryAddStream() {
    Log.d(TAG, "TRY ADD STREAM " + userMedia + " " + peerConnection);
    if(userMedia != null && peerConnection != null) {
      peerConnection.addStream(userMedia);
      joinButton.setEnabled(true);
      statusText.setText("waiting for connection...");
    }
  }

}
