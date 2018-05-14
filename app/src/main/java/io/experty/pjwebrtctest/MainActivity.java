package io.experty.pjwebrtctest;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import org.json.JSONArray;
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

  public UserMedia userMedia;
  public PeerConnection peerConnection;

  public TextView statusText;
  public Button joinButton;
  public EditText roomName;

  public ReactiveConnection connection;

  public static final String clientUUID = UUID.randomUUID().toString();

  public Observer amICallingObserver;
  public Observer myIpObserver;
  public Observer otherUserIceObserver;
  public Observer otherUserSdpObserver;
  public boolean calling;
  public boolean callingReceived = false;
  public String myIP = "";
  public Object localDescription = JSONObject.NULL;
  public boolean inRoom;

  public void resetPC() {
    if(peerConnection != null) {
      peerConnection.onConnectionStateChange = null;
      peerConnection.onIceGatheringStateChange = null;
      peerConnection.onIceConnectionStateChange = null;
      peerConnection.onIceCandidate = null;
      peerConnection.close(new ResultCallback<JSONObject>() {
        @Override
        public void onResult(JSONObject result) {
          peerConnection = null;
        }
      });
    }

    JSONObject pcConfig = new JSONObject();
    try {
      pcConfig.put("iceServers", new JSONArray(
          "[{\n" +
              "  \"urls\":\"turn:turn.xaos.ninja:4433\",\n" +
              "  \"username\":\"test\",\n" +
              "  \"credential\":\"12345\"\n" +
              "}]"
      ));
    } catch (JSONException e) {
      e.printStackTrace();
    }
    PjWebRTC.createPeerConnection(pcConfig, new ResultCallback<PeerConnection>() {
      @Override
      public void onResult(PeerConnection result) {
        peerConnection = result;
        peerConnection.onIceCandidate = new ResultCallback<Object>() {
          @Override
          public void onResult(Object result) {
            handleIceCandidate(result);
          }
        };
        peerConnection.onIceGatheringStateChange = new ResultCallback<String>() {
          @Override
          public void onResult(final String result) {
            runOnUiThread(new Runnable() {
              @Override
              public void run() {
                statusText.setText("ICE Gathering state: " + result);
              }
            });
          }
        };
        peerConnection.onIceConnectionStateChange = new ResultCallback<String>() {
          @Override
          public void onResult(final String result) {
            runOnUiThread(new Runnable() {
              @Override
              public void run() {
                statusText.setText("ICE Connection state: " + result);
              }
            });
          }
        };
        peerConnection.onConnectionStateChange = new ResultCallback<String>() {
          @Override
          public void onResult(final String result) {
            runOnUiThread(new Runnable() {
              @Override
              public void run() {
                statusText.setText("Connection state: " + result);
              }
            });
          }
        };
        tryAddStream();
      }
    });
  }

  public void closePC() {
    peerConnection.onConnectionStateChange = null;
    peerConnection.onIceGatheringStateChange = null;
    peerConnection.onIceConnectionStateChange = null;
    peerConnection.onIceCandidate = null;
    peerConnection.close(new ResultCallback<JSONObject>() {
      @Override
      public void onResult(JSONObject result) {
        peerConnection = null;
      }
    });
    calling = false;
    localDescription = JSONObject.NULL;
  }

  public void sendOffer() {
    peerConnection.createOffer(new ResultCallback<JSONObject>() {
      @Override
      public void onResult(JSONObject result) {
        final JSONObject sdp = result;
        localDescription = sdp;
        peerConnection.setLocalDescription(sdp, new ResultCallback<Boolean>() {
          @Override
          public void onResult(Boolean result) {
          }
        });
        try {
          connection.request(new JSONArray("[\"room\", \"setSdp\"]"), new JSONArray("[\"" + roomName.getText() + "\", " + localDescription.toString() + "]"), new ResultCallback<Object>() {
            @Override
            public void onResult(Object result) {
            }
          });
        } catch (JSONException e) {
          e.printStackTrace();
        }
      }
    });
  }

  public void sendAnswer() {
    peerConnection.createAnswer(new ResultCallback<JSONObject>() {
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
                  localDescription = answer;
                  connection.request(new JSONArray("[\"room\", \"setSdp\"]"), new JSONArray("[\"" + roomName.getText() + "\", " + localDescription.toString() + "]"), new ResultCallback<Object>() {
                    @Override
                    public void onResult(Object result) {
                    }
                  });
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

  private void handleIceCandidate(Object candidate) {
    try {
      connection.request(new JSONArray("[\"room\", \"addIce\"]"), new JSONArray("[\""+roomName.getText()+"\", "+candidate+"]"), new ResultCallback<Object>() {
        @Override
        public void onResult(Object result) {
        }
      });
    } catch (JSONException e) {
      e.printStackTrace();
    }
  }


  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);

    final MainActivity activity = this;
    statusText = findViewById(R.id.status_text);
    joinButton = findViewById(R.id.join_button);
    roomName = findViewById(R.id.roomName);
    joinButton.setEnabled(false);

    amICallingObserver = new Observer() {
      @Override
      public void handle(String signal, JSONArray args) {
        if(!signal.equals("set")) throw new RuntimeException("unknown signal "+signal);
        try {
          if(args.get(0) == JSONObject.NULL) return;
          boolean nc = args.getBoolean(0);
          if(!callingReceived) {
            calling = nc;
          } else if(calling != nc) {
            resetPC();
          }
        } catch (JSONException e) {
          e.printStackTrace();
        }
      }
    };

    myIpObserver = new Observer() {
      @Override
      public void handle(String signal, JSONArray args) {
        if(!signal.equals("set")) throw new RuntimeException("unknown signal "+signal);
        String ip = null;
        try {
          ip = args.getString(0);
          if(!myIP.equals(ip)) {
            myIP = ip;
            resetPC();
          } else { // Reaction to reconnect
            connection.request(new JSONArray("[\"room\", \"setSdp\"]"), new JSONArray("[\""+roomName.getText()+"\", "+localDescription.toString()+"]"), new ResultCallback<Object>() {
              @Override
              public void onResult(Object result) {
              }
            });
          }
        } catch (JSONException e) {
          e.printStackTrace();
        }
      }
    };

    otherUserIceObserver = new Observer() {
      @Override
      public void handle(String signal, JSONArray args) {
        Log.d(TAG, "Other user ICE changed! "+ signal + " : " + args.toString());
        try {
          if(signal.equals("set")) {
            JSONArray candidates = args.getJSONArray(0);
            for(int i = 0; i < candidates.length(); i++) {
              peerConnection.addIceCandidate(candidates.getJSONObject(i), new ResultCallback<Boolean>() {
                @Override
                public void onResult(Boolean result) { }
              });
            }
          } else if(signal.equals("push")) {
            peerConnection.addIceCandidate(args.get(0) == JSONObject.NULL ? null : args.getJSONObject(0), new ResultCallback<Boolean>() {
              @Override
              public void onResult(Boolean result) {
              }
            });
          } else {
            throw new RuntimeException("unknown signal "+signal);
          }
        } catch (JSONException e) {
          e.printStackTrace();
        }
      }
    };

    otherUserSdpObserver = new Observer() {
      @Override
      public void handle(String signal, JSONArray args) {
        try {
          if(!signal.equals("set")) throw new RuntimeException("unknown signal "+signal);
          if(args.get(0) == JSONObject.NULL) return;
          runOnUiThread(new Runnable() {
                          @Override
                          public void run() {
                            statusText.setText("Connecting to other user...");
                          }
                        });
          Log.d(TAG, "args: "+ args.toString());
          peerConnection.setRemoteDescription(args.getJSONObject(0), new ResultCallback<Boolean>() {
            @Override
            public void onResult(Boolean result) {
              if(!calling) sendAnswer();
            }
          });
        } catch (JSONException e) {
          e.printStackTrace();
        }
      }
    };

    joinButton.setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View view) {
        if(!inRoom) {
          inRoom = true;
          roomName.setEnabled(false);
          joinButton.setText("Exit Room");

          String rn = roomName.getText().toString();

          statusText.setText("Connecting to room " + rn);

          try {
            connection.observableValue(new JSONArray("[\"room\", \"amICalling\", \"" + rn + "\"]")).addObserver(amICallingObserver);
            connection.observableValue(new JSONArray("[\"room\", \"myIp\", \"" + rn + "\"]")).addObserver(myIpObserver);
            connection.observableList(new JSONArray("[\"room\", \"otherUserIce\", \"" + rn + "\"]")).addObserver(otherUserIceObserver);
            connection.observableValue(new JSONArray("[\"room\", \"otherUserSdp\", \"" + rn + "\"]")).addObserver(otherUserSdpObserver);
          } catch (JSONException e) {
            e.printStackTrace();
          }
        } else {
          inRoom = false;
          roomName.setEnabled(true);
          joinButton.setText("Enter Room");

          String rn = roomName.getText().toString();
          statusText.setText("Please enter room name");

          try {
            connection.observableValue(new JSONArray("[\"room\", \"amICalling\", \"" + rn + "\"]")).removeObserver(amICallingObserver);
            connection.observableValue(new JSONArray("[\"room\", \"myIp\", \"" + rn + "\"]")).removeObserver(myIpObserver);
            connection.observableList(new JSONArray("[\"room\", \"otherUserIce\", \"" + rn + "\"]")).removeObserver(otherUserIceObserver);
            connection.observableValue(new JSONArray("[\"room\", \"otherUserSdp\", \"" + rn + "\"]")).removeObserver(otherUserSdpObserver);
            closePC();
          } catch (JSONException e) {
            e.printStackTrace();
          }
        }
      }
    });

    PjWebRTC.getUserMedia(new JSONObject(), new ResultCallback<UserMedia>() {
      @Override
      public void onResult(UserMedia result) {
        userMedia = result;
        Log.d(TAG,"got user media");
        tryAddStream();
      }
    });

    statusText.setText("Connecting to server...");
    joinButton.setEnabled(false);
    connection = new ReactiveConnection("wss://comm.xaos.ninja/ws", clientUUID);
    connection.onConnectionStateChange = new ResultCallback<String>() {
      @Override
      public void onResult(final String state) {
        runOnUiThread(new Runnable() {
          @Override
          public void run() {
            if(state == "connected") {
              statusText.setText("Please enter room name");
              joinButton.setEnabled(true);
            } else {
              statusText.setText("Disconnected: "+state);
              joinButton.setEnabled(false);
            }
          }
        });
      }
    };

  }

  protected void tryAddStream() {
    Log.d(TAG, "TRY ADD STREAM " + userMedia + " " + peerConnection);
    if(userMedia != null && peerConnection != null) {
      peerConnection.addStream(userMedia);
      if(calling) {
        sendOffer();
      }
    }
  }

}
