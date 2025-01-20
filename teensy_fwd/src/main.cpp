#include "commands.cpp"

void setup() {
}

void loop() {
    // before doing anything, address any waiting
    // commands
    // (since these can be time sensitive, such as
    // poll priority commands)
    apply_pending_commands();
}
