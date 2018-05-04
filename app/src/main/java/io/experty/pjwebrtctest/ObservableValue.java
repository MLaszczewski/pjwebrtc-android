package io.experty.pjwebrtctest;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;

public class ObservableValue extends Observable {

  Object value = JSONObject.NULL;

  ObservableValue(ReactiveConnection connection, Object what) {
    super(connection, what);
  }

  public void addObserver(Observer obs) {
    observers.add(obs);
    JSONArray arr = new JSONArray();
    arr.put(value);
    obs.handle("set", arr);
  }

  public void handle(String signal, JSONArray args) {
    try {
      if(signal.equals("set")) {
        value = args.get(0);
        notifyObservers(signal, args);
      }
    } catch (JSONException e) {
      e.printStackTrace();
    }
  }

}
