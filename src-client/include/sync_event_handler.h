#ifndef SYNC_EVENT_HANDLER_H
#define SYNC_EVENT_HANDLER_H

#include <string>


// Main event handler loop. Processes events and pending events.
void handle_events();

void handle_all_pending_events(); // helper function to handle all pending events, used on startup to update client state before processing any new events from the tracker

#endif