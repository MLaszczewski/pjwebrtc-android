package io.experty.pjwebrtctest;

import org.json.JSONArray;

public interface Observer {
  void handle(String signal, JSONArray args);
}
