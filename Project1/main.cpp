#include <windows.h>
#include <vector>
#include <iostream>
#include <string>
#include <map>
#include "Timer.h"
#include "Vector2.h"

using std::wstring;

constexpr float GRAVITY = 9.807f;
constexpr float PX_PER_METER = 1.f;
constexpr float NEW_WINDOW_CHECK_RATE = 0.5f;

static POINT old_mouse_pos;
static POINT mouse_pos;

std::pair<int, int> GetDesktopDimensions1() {
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    return { screenWidth, screenHeight };
}

BOOL CALLBACK GetAllWindows(HWND hwnd, LPARAM lParam) {
    const DWORD TITLE_SIZE = 1024;  
    WCHAR windowTitle[TITLE_SIZE];

    GetWindowTextW(hwnd, windowTitle, TITLE_SIZE);

    int length = ::GetWindowTextLength(hwnd);
    std::wstring title(&windowTitle[0]);
    if (!IsWindowVisible(hwnd) || length == 0 || title == L"Program Manager") {
        return TRUE;
    }

    std::map<HWND, wstring>& titles =
        *reinterpret_cast<std::map<HWND, wstring>*>(lParam);
    titles[hwnd] = windowTitle;

    return TRUE;
}

struct FRECT {
    FRECT() : left(0), top(0), right(0), bottom(0) {};
    FRECT(RECT rect) : left(rect.left), top(rect.top), right(rect.right), bottom(rect.bottom) {};
    float bottom;
    float left;
    float top;
    float right;
};

struct Winfo {
    Vector2 acceleration;
    Vector2 velocity;
    FRECT rect;
};

struct Physics {
    std::map<HWND, Winfo> windows;
    Timer timer;
    Timer enum_window_timer;
    int screen_width;
    int screen_height;
};

void UpdateWindowsList(std::map<HWND, Winfo>* windows) {
    std::map<HWND, wstring> w_input_vec;
    EnumWindows(GetAllWindows, reinterpret_cast<LPARAM>(&w_input_vec));
    for (const auto& [hwnd, _] : w_input_vec) {
        if (windows->contains(hwnd)) {
            continue;
        }
        std::wcout << L"found window: " << hwnd << std::endl;
        RECT rect_{};
        LPRECT rect{ &rect_ };
        GetWindowRect(hwnd, rect);
        //std::cout << '(' << rect->left << ',' << rect->top << ')' << '\n';
        (*windows)[hwnd] = {
            {0, GRAVITY},
            {(float)(10 - rand() % 20), 0},
            FRECT(rect_)
        };
    }
}

void UpdatePhysics(Physics* physics) {
    float dt = physics->timer.elapsed();
    physics->timer.reset();
    // upd mouse stuff
    GetCursorPos(&mouse_pos);
    if (mouse_pos.x != old_mouse_pos.x || mouse_pos.y != old_mouse_pos.y) {
        float dx = fabs(mouse_pos.x - old_mouse_pos.x);
        float dy = fabs(mouse_pos.y - old_mouse_pos.y);

        old_mouse_pos.x = mouse_pos.x;
        old_mouse_pos.y = mouse_pos.y;
    }
    if (physics->enum_window_timer.elapsed() > NEW_WINDOW_CHECK_RATE) {
        physics->enum_window_timer.reset();
        UpdateWindowsList(&physics->windows);
    }
    const auto& h = physics->screen_height;
    const auto& w = physics->screen_width;
    HDWP defer_info = BeginDeferWindowPos(physics->windows.size());
    for (auto& [hwnd, winfo] : physics->windows) {
        FRECT& rect = winfo.rect;
        float old_vy = winfo.velocity.y;
        float old_vx = winfo.velocity.x;
        bool clips_bottom = rect.bottom + old_vy >= h;
        bool clips_top = rect.top + old_vy <= 0;
        bool clips_left = rect.left + old_vx <= 0;
        bool clips_right = rect.right + old_vx >= w;
        
        winfo.velocity.y = old_vy + winfo.acceleration.y * dt;
        winfo.velocity.x = old_vx + winfo.acceleration.x * dt;
        
        if (clips_bottom || clips_top) {
            float rebound = clips_bottom ? 
                rect.bottom + winfo.velocity.y - h : 
                (rect.top + winfo.velocity.y);
            rect.top -= rebound;
            rect.bottom -= rebound;
            winfo.velocity.y = -winfo.velocity.y;
        }
        else {
            rect.top += winfo.velocity.y * PX_PER_METER;
            rect.bottom += winfo.velocity.y * PX_PER_METER;
        }
        if (clips_left || clips_right) {
            float rebound = clips_left ?
                (rect.left + winfo.velocity.x) :
                rect.right + winfo.velocity.x - w;
            rect.left -= rebound;
            rect.right -= rebound;
            winfo.velocity.x = -winfo.velocity.x;
        }
        else {
            rect.left += winfo.velocity.x * PX_PER_METER;
            rect.right += winfo.velocity.x * PX_PER_METER;
        }

        UINT flags =
            SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOREDRAW | SWP_NOZORDER;
        DeferWindowPos(defer_info, hwnd, NULL, rect.left, rect.top, NULL, NULL, flags);
    }
    if (EndDeferWindowPos(defer_info) != TRUE) {
        exit(1);
    }
}

int main() {
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    std::map<HWND, Winfo> window_positions;
    UpdateWindowsList(&window_positions);
    auto dimensions = GetDesktopDimensions1();
    Physics physics {
        window_positions,
        {},
        {},
        dimensions.first,
        dimensions.second
    };
    while (true) {
        UpdatePhysics(&physics);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    std::cin.get();
    return 0;
}