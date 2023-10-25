#pragma once

#if __APPLE__

#include "modules/desktop_capture/mouse_cursor_monitor.h"

namespace bridge {

// Captures mouse shape and position.
// Wraps `webrtc::MouseCursorMonitor` wrapping `Capture` calls with @autorelease
// block to prevent memory leaking.
class MouseCursorMonitorMac : public webrtc::MouseCursorMonitor {
 public:
  // Creates a new `MouseCursorMonitorMac`.
  MouseCursorMonitorMac(
      std::unique_ptr<webrtc::MouseCursorMonitor> mouse_monitor);

  // Initializes the monitor with the `callback`, which must remain valid until
  // capturer is destroyed.
  virtual void Init(Callback* callback, Mode mode) override;

  // Captures current cursor shape and position
  void Capture() override;

 private:
  // Inner `MouseCursorMonitor` that this `MouseCursorMonitorMac` delegates
  // calls to.
  std::unique_ptr<webrtc::MouseCursorMonitor> mouse_monitor_;
};

}  // namespace bridge

#endif
