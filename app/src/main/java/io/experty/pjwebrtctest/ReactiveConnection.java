package io.experty.pjwebrtctest;

import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.HashMap;

import io.experty.pjwebrtc.ResultCallback;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;
import okhttp3.WebSocket;
import okhttp3.WebSocketListener;
import okio.ByteString;

public class ReactiveConnection extends WebSocketListener {
  static final String TAG = "REACTIVE";

  private static final int NORMAL_CLOSURE_STATUS = 1000;

  public Object sessionId;
  public WebSocket webSocket;

  public int lastRequestId;
  public HashMap<Integer, ResultCallback<Object>> requests = new HashMap<Integer, ResultCallback<Object>>();
  public HashMap<Object, Observable> observables = new HashMap<Object, Observable>();

  public String connectionState;
  public ResultCallback<String> onConnectionStateChange;

  public ReactiveConnection(String url, Object sessionId) {
    connectionState = "connecting";
    this.sessionId = sessionId;
    OkHttpClient httpClient = new OkHttpClient();
    Request request = new Request.Builder()
        .url(url)
        .header("Sec-WebSocket-Protocol", "reactive-observer")
        .build();
    webSocket = httpClient.newWebSocket(request, this);
    httpClient.dispatcher().executorService().shutdown();
  }

  public void send(String msg) {
    Log.d(TAG, "OUTGOING MESSAGE: "+msg);
    webSocket.send(msg);
  }

  public void removeObservable(Object what) {
    observables.remove(what);
    try {
      JSONObject msg = new JSONObject();
      msg.put("type", "unobserve");
      msg.put("what", what);
      send(msg.toString(2));
    } catch (JSONException e) {
      e.printStackTrace();
    }
  }
  public ObservableList observableList(Object what) {
    if(observables.containsKey(what)) {
      return (ObservableList) observables.get(what);
    }
    ObservableList list = new ObservableList(this, what);
    observables.put(what, list);

    try {
      JSONObject msg = new JSONObject();
      msg.put("type", "observe");
      msg.put("what", what);
      send(msg.toString(2));
    } catch (JSONException e) {
      e.printStackTrace();
    }

    return list;
  }
  public ObservableValue observableValue(Object what) {
    if(observables.containsKey(what)) {
      return (ObservableValue) observables.get(what);
    }
    ObservableValue value = new ObservableValue(this, what);

    observables.put(what, value);

    try {
      JSONObject msg = new JSONObject();
      msg.put("type", "observe");
      msg.put("what", what);
      send(msg.toString(2));
    } catch (JSONException e) {
      e.printStackTrace();
    }

    return value;
  }
  public void request(Object method, JSONArray args, ResultCallback<Object> resultCb) {
    int requestId = ++lastRequestId;
    requests.put(requestId, resultCb);
    try {
      JSONObject msg = new JSONObject();
      msg.put("type", "request");
      msg.put("requestId", requestId);
      msg.put("method", method);
      msg.put("args", args);
      send(msg.toString(2));
    } catch (JSONException e) {
      e.printStackTrace();
    }
  }

  public void onOpen(WebSocket webSocket, Response response) {
    Log.d(TAG, "WebSocket connected");
    try {
      JSONObject msg = new JSONObject();
      msg.put("type", "initializeSession");
      msg.put("sessionId", sessionId);
      send(msg.toString(2));
    } catch (JSONException e) {
      e.printStackTrace();
    }
    onConnectionStateChange.onResult("connected");
  }

  @Override
  public void onMessage(WebSocket webSocket, String text) {
    Log.d(TAG, "INCOMING MESSAGE : " + text);
    try {
      JSONObject msg = new JSONObject(text);
      if(msg.has("responseId")) {
        if(msg.getString("type").equals("error")) {
          throw new RuntimeException("received error: "+ msg.getString("error"));
        } else {
          int responseId = msg.getInt("responseId");
          requests.get(responseId).onResult(msg.get("response"));
          requests.remove(responseId);
        }
      } else if(msg.get("type").equals("notify")) {
        Observable obs = observables.get(msg.get("what"));
        if(obs != null) obs.handle(msg.getString("signal"), msg.getJSONArray("args"));
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
    onConnectionStateChange.onResult("disconnected");
  }

  @Override
  public void onFailure(WebSocket webSocket, Throwable t, Response response) {
    Log.d(TAG,"Error : " + t.getMessage());
    onConnectionStateChange.onResult("failed");
  }

}
