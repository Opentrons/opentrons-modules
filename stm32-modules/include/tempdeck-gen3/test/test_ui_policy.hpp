#pragma once

struct TestUIPolicy {

    void set_heartbeat_led(bool set) {
        _heartbeat_set = set;
        ++_heartbeat_set_count;
    }

    bool _heartbeat_set = false;
    size_t _heartbeat_set_count = 0;
};
