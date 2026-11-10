#pragma once

class Rail;
class Client;

// For the automated Windows mode test (docs/windows-mode-plan.md,
// Hardening): driven by environment variables, it types into and clicks
// the first app window through the same input path a person's keyboard
// and mouse take, and saves each window's pixels as Windows sent them.
//
//   WILDOS_RDP_TEST_TYPE   text to type once the first window has drawn;
//                          <ctrl+v> is a chord, <wait> half a second
//   WILDOS_RDP_TEST_CLICK  x,y in the window to click before typing
//   WILDOS_RDP_TEST_DUMP   folder for <window id>.png, written after typing
//   WILDOS_RDP_TEST_QUIT   seconds after typing to end the session
void startSelfTest(Rail *rail, Client *client);
