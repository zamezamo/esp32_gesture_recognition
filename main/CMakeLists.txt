idf_component_register(
    SRCS 
        "main.cpp"
        "camera.cpp"
        "hand_gesture_model.cpp"
        "http_handlers.cpp"
        "http_server.cpp"
        "wifi.cpp"

    INCLUDE_DIRS
        "."
        "../model"
    )
