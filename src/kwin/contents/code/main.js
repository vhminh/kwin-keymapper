console.log("[DEBUG] kwin_app_keymapper KWin script starting")
workspace.windowActivated.connect(function(window) {
  if (window) {
    callDBus(
      "io.github.vhminh.kwin_keymapper",
      "/io/github/vhminh/kwin_keymapper",
      "io.github.vhminh.kwin_keymapper",
      "active_window_changed",
      window.resourceClass,
      window.resourceName,
      window.caption,
    );
  }
});
console.log("[DEBUG] kwin_app_keymapper KWin script ended")

