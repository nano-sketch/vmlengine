#include "MyApp.h"
#ifdef _WIN32
#include <windows.h>
#endif

#define WINDOW_WIDTH  600
#define WINDOW_HEIGHT 400

MyApp::MyApp() {
  ///
  /// Create our main App instance.
  ///
  app_ = App::Create();

  ///
  /// Create a resizable window by passing by OR'ing our window flags with
  /// kWindowFlags_Resizable.
  ///
  window_ = Window::Create(app_->main_monitor(), WINDOW_WIDTH, WINDOW_HEIGHT,
    false, kWindowFlags_Titled | kWindowFlags_Resizable);

  ///
  /// Create our HTML overlay-- we don't care about its initial size and
  /// position because it'll be calculated when we call OnResize() below.
  ///
  overlay_ = Overlay::Create(window_, 1, 1, 0, 0);

  ///
  /// Force a call to OnResize to perform size/layout of our overlay.
  ///
  OnResize(window_.get(), window_->width(), window_->height());

  ///
  /// Load a page into our overlay's View
  ///
  overlay_->view()->LoadURL("file:///app.html");

  ///
  /// Register our MyApp instance as an AppListener so we can handle the
  /// App's OnUpdate event below.
  ///
  app_->set_listener(this);

  ///
  /// Register our MyApp instance as a WindowListener so we can handle the
  /// Window's OnResize event below.
  ///
  window_->set_listener(this);

  ///
  /// Register our MyApp instance as a LoadListener so we can handle the
  /// View's OnFinishLoading and OnDOMReady events below.
  ///
  overlay_->view()->set_load_listener(this);

  ///
  /// Register our MyApp instance as a ViewListener so we can handle the
  /// View's OnChangeCursor and OnChangeTitle events below.
  ///
  overlay_->view()->set_view_listener(this);

  // Setup simple live-reload watcher for assets/app.html (if present)
  try {
    namespace fs = std::filesystem;
#include <algorithm>

    std::vector<fs::path> candidates;

    // Check common locations relative to current working directory
    candidates.push_back(fs::current_path() / "assets" / "app.html");
    candidates.push_back(fs::current_path() / "app.html");

    // Also check parent directories of the current path for an `assets/app.html` (walk up a few levels)
    {
      fs::path p = fs::current_path();
      for (int i = 0; i < 6; ++i) {
        candidates.push_back(p / "assets" / "app.html");
        p = p.parent_path();
        if (p.empty()) break;
      }
    }

    // Also check the executable directory's assets (useful when running installed in build/out)
#ifdef _WIN32
    {
      wchar_t exe_path_w[MAX_PATH];
      if (GetModuleFileNameW(NULL, exe_path_w, MAX_PATH) > 0) {
        fs::path exe_path = fs::path(exe_path_w);
        // add several ancestor-relative checks from the exe location
        fs::path p = exe_path.parent_path();
        for (int i = 0; i < 6 && !p.empty(); ++i) {
          candidates.push_back(p / "assets" / "app.html");
          candidates.push_back(p / "app.html");
          p = p.parent_path();
        }
      }
    }
#else
    // Try reading /proc/self/exe on unix-like systems
    try {
      fs::path exe_link = "/proc/self/exe";
      if (fs::exists(exe_link)) {
        fs::path exe_path = fs::read_symlink(exe_link);
        candidates.push_back(exe_path.parent_path() / "assets" / "app.html");
        candidates.push_back(exe_path.parent_path() / ".." / "assets" / "app.html");
      }
    } catch (...) {}
#endif

    for (auto &p : candidates) {
      if (fs::exists(p)) {
        watched_file_path_ = p;
        watched_last_write_ = fs::last_write_time(p);
        watched_last_check_ = std::chrono::steady_clock::now();
        break;
      }
    }
  } catch (...) {
    // ignore filesystem errors; watcher will remain disabled
  }
}

MyApp::~MyApp() {
}

void MyApp::Run() {
  app_->Run();
}

void MyApp::OnUpdate() {
  ///
  /// This is called repeatedly from the application's update loop.
  ///
  /// You should update any app logic here.
  ///
  // Check watched file every 500ms and reload view when modified.
  if (!watched_file_path_.empty()) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - watched_last_check_).count();
    if (elapsed > 500) {
      watched_last_check_ = now;
      try {
        auto current_write = std::filesystem::last_write_time(watched_file_path_);
        if (current_write != watched_last_write_) {
          watched_last_write_ = current_write;
          overlay_->view()->Reload();
        }
      } catch (...) {
        // ignore errors
      }
    }
  }
}

void MyApp::OnClose(ultralight::Window* window) {
  app_->Quit();
}

void MyApp::OnResize(ultralight::Window* window, uint32_t width, uint32_t height) {
  ///
  /// This is called whenever the window changes size (values in pixels).
  ///
  /// We resize our overlay here to take up the entire window.
  ///
  overlay_->Resize(width, height);
}

void MyApp::OnFinishLoading(ultralight::View* caller,
                            uint64_t frame_id,
                            bool is_main_frame,
                            const String& url) {
  ///
  /// This is called when a frame finishes loading on the page.
  ///
}

void MyApp::OnDOMReady(ultralight::View* caller,
                       uint64_t frame_id,
                       bool is_main_frame,
                       const String& url) {
  ///
  /// This is called when a frame's DOM has finished loading on the page.
  ///
  /// This is the best time to setup any JavaScript bindings.
  ///
}

void MyApp::OnChangeCursor(ultralight::View* caller,
                           Cursor cursor) {
  ///
  /// This is called whenever the page requests to change the cursor.
  ///
  /// We update the main window's cursor here.
  ///
  window_->SetCursor(cursor);
}

void MyApp::OnChangeTitle(ultralight::View* caller,
                          const String& title) {
  ///
  /// This is called whenever the page requests to change the title.
  ///
  /// We update the main window's title here.
  ///
  window_->SetTitle(title.utf8().data());
}
