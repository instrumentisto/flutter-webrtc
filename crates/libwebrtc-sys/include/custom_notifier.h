#include <winsock2.h>
#include <windows.h>
#include <dbt.h>
#include <strsafe.h>
#include <functional>
#include <memory>
#include <vector>
#include <chrono>
#include <atomic>

class CustomNotifier {
public:
  // CustomNotifier(std::function<void()> cb) : cb_(cb) {
  CustomNotifier() {
    std::thread t([=]() {
      const wchar_t winClass[] = L"MyNotifyWindow";
      const wchar_t winTitle[] = L"WindowTitle";

      WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
      wcex.lpfnWndProc = DWProc;
      wcex.lpszClassName = L"MyNotifyWindow";
      ATOM wnd_class_ = RegisterClassExW(&wcex);

      HINSTANCE hInstance = GetModuleHandle(NULL);
      hwnd_ = CreateWindowW(winClass, winTitle, WS_ICONIC, 0, 0,
        CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);
      SetWindowLongPtr(hwnd_, GWLP_USERDATA, (LONG_PTR)this);
      ShowWindow(hwnd_, SW_HIDE);

      MSG Msg;

      while (GetMessage(&Msg, NULL, 0, 0) > 0) {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
      }
      });
    t.detach();
  }

  void kek() {
    // timer_ = new CustomTimer();
    // timer_->kek();
    // timer_->setTimeout([&]() {
    //   printf("lol\n");
    //   timer_->stop();
    //   }, 500);

    // timer_.setTimeout([&]() {
    //   printf("lol\n");
    //   timer_.stop();
    //   }, 500);
  }

private:
  static LRESULT CALLBACK DWProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    LRESULT result = 0;

    auto asdasd = (CustomNotifier*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    if (msg == WM_CLOSE) {
      exit(0);
    } else if (msg == WM_DEVICECHANGE) {
      if (DBT_DEVICEARRIVAL == wp) {
        printf("pupa\n");
      } else if (DBT_DEVICEREMOVECOMPLETE == wp) {
        printf("lupa\n");
      } else if (DBT_DEVNODES_CHANGED == wp) {
        // if (DBT_DEVNODES_CHANGED == wp) {
          // printf("%lld\n", asdasd->timer_.load());
        printf("zupa\n");
        asdasd->timer_.store(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        // printf("%lld\n", asdasd->timer_.load());
      }
    } else if (msg == WM_ERASEBKGND) {
    } else if (msg == WM_SETFOCUS) {
    } else if (msg == WM_SIZE) {
    } else if (msg == WM_CTLCOLORSTATIC) {
    } else if (msg == WM_COMMAND) {
    } else {
      result = DefWindowProc(hwnd, msg, wp, lp);
    }

    return result;
  };

  std::function<void()> cb_;
  HWND hwnd_;
  std::atomic<long long> timer_{ 0 };
};
