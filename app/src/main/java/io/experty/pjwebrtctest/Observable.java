package io.experty.pjwebrtctest;

import org.json.JSONArray;
import org.json.JSONException;

import java.util.ArrayList;

public class Observable {

  public ArrayList<Observer> observers = new ArrayList<Observer>();

  Object value;

  ReactiveConnection connection;
  Object what;

  Observable(ReactiveConnection connection, Object what) {
    this.connection = connection;
    this.what = what;
  }

  public void addObserver(Observer obs) {
    observers.add(obs);
    JSONArray arr = new JSONArray();
    arr.put(value);
    obs.handle("set", arr);
  }

  public void removeObserver(Observer obs) {
    observers.remove(obs);
    if(observers.size() == 0) connection.removeObservable(what);
  }

  public void notifyObservers(String signal, JSONArray args) {
    for(Observer obs : observers) {
      obs.handle(signal, args);
    }
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
