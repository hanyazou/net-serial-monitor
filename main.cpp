/*
 * Net & Serial Monitor (Raspberry Pi OS, C++/FLTK)
 *
 * Purpose:
 *   A tiny GUI that periodically runs two background subprocesses:
 *     1) "test_network.sh" to check network reachability.
 *     2) "test_serial.sh" to check serial connectivity.
 *   It shows:
 *     - A one-line status text like: "network=OK, serial=connected".
 *     - Three traffic-light-style filled circles horizontally:
 *         [0] network  (green=success, red=failure, gray=unknown at startup)
 *         [1] serial   (green=success, red=failure, gray=unknown at startup)
 *         [2] reserved (always gray for future use)
 *     - An [Exit] button to quit safely.
 *
 * Notes:
 *   - Keep the program small & simple (single source file).
 *   - All UI labels and comments are in English.
 *   - FLTK is used for minimal dependencies on Raspberry Pi OS.
 *   - UI thread never blocks; worker threads update atomics.
 *   - A periodic FLTK timer polls the atomics and redraws.
 */

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/fl_draw.H>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>   // access()

// ----- Simple tri-state: unknown / ok / fail -----
enum class ProbeState : int { Unknown = -1, Fail = 0, Ok = 1 };

// ----- Shared application state for background workers and UI -----
struct AppState {
    std::atomic<ProbeState> network{ProbeState::Unknown};
    std::atomic<ProbeState> serial{ProbeState::Unknown};
    std::atomic<bool> running{true};
};

// ----- Resolve script path -----
static std::string resolve_path(const std::string& script) {
    std::string path;
    if (const char* p = std::getenv("PATH")) {
        std::string sp(p);
        std::stringstream ss(sp);
        std::string dir;
        while (std::getline(ss, dir, ':')) {
            if (dir.empty()) continue;
            std::string candidate = dir + "/" + script;
            if (access(candidate.c_str(), X_OK) == 0) {
                path = candidate;
                return path;
            }
        }
    }
    const char* fallbacks[] = { "/usr/local/bin", "/usr/bin", nullptr };
    for (int i = 0; fallbacks[i]; ++i) {
        std::string candidate = std::string(fallbacks[i]) + "/" + script;
        if (access(candidate.c_str(), X_OK) == 0) {
            path = candidate;
            return path;
        }
    }
    path.clear(); // not found
    return path;
}

// ----- Small helper to run a shell command and return success/fail -----
static inline ProbeState run_command_success(const char* cmd) {
    // Run command via /bin/sh -c, suppressing output within the command string.
    int rc = std::system(cmd);
    // std::system returns -1 on error launching shell; otherwise wait status.
    // We treat ==0 as success, everything else as failure.
    return (rc == 0) ? ProbeState::Ok : ProbeState::Fail;
}

// ----- Custom widget to draw the three status circles and captions -----
class StatusPanel : public Fl_Widget {
public:
    StatusPanel(int X, int Y, int W, int H, AppState* s)
        : Fl_Widget(X, Y, W, H), state_(s) {}

private:
    AppState* state_;

    static Fl_Color color_for(ProbeState st) {
        switch (st) {
            case ProbeState::Ok:     return FL_GREEN;
            case ProbeState::Fail:   return FL_RED;
            case ProbeState::Unknown:
            default:                 return fl_rgb_color(128,128,128); // gray
        }
    }

    void draw_circle(int cx, int cy, int d, Fl_Color fill) {
        // filled circle using a full pie; add a darker outline
        fl_color(fill);
        fl_pie(cx, cy, d, d, 0.0, 360.0);
        fl_color(FL_DARK3);
        fl_arc(cx, cy, d, d, 0.0, 360.0);
    }

    void draw_caption_centered(int x, int y, int w, const char* s) {
        fl_font(FL_HELVETICA, 12);
        int tw = 0, th = 0;
        fl_measure(s, tw, th, false);
        int tx = x + (w - tw) / 2;
        int ty = y + th; // draw baseline from y
        fl_color(FL_BLACK);
        fl_draw(s, tx, ty);
    }

    void draw() override {
        // Layout:
        // left/right margin and gaps so three ~100px circles fit in W.
        const int margin = 10;
        const int available = w() - margin*2;
        // Try to keep diameter near 100, but fit within panel.
        int d = available / 3 - 10;          // base diameter
        if (d > 100) d = 100;
        if (d < 60)  d = 60;                 // keep visible on small panels

        const int gap = (available - 3*d) / 2;
        const int top = y() + 10;
        const int left = x() + margin;

        // Vertical positions
        const int circleY = top;
        const int captionY = circleY + d + 8;

        // Determine colors from state
        Fl_Color c0 = color_for(state_->network.load());
        Fl_Color c1 = color_for(state_->serial.load());
        Fl_Color c2 = fl_rgb_color(128,128,128); // reserved = gray

        // Draw three circles (network, serial, reserved)
        int x0 = left;
        int x1 = left + d + gap;
        int x2 = left + 2*(d + gap);

        draw_circle(x0, circleY, d, c0);
        draw_circle(x1, circleY, d, c1);
        draw_circle(x2, circleY, d, c2);

        // Captions (only for network/serial as requested)
        draw_caption_centered(x0, captionY, d, "network");
        draw_caption_centered(x1, captionY, d, "serial");
        // third intentionally left without caption (reserved)
    }
};

// ----- Compose the one-line status text from atomics -----
static inline std::string make_status_line(const AppState& s) {
    auto p = s.network.load();
    auto q = s.serial.load();

    auto to_str = [](ProbeState st) -> const char* {
        switch (st) {
            case ProbeState::Ok:     return "OK";
            case ProbeState::Fail:   return "down";
            case ProbeState::Unknown:
            default:                 return "unknown";
        }
    };

    char buf[128];
    std::snprintf(buf, sizeof(buf), "network=%s, serial=%s", to_str(p), to_str(q));
    return std::string(buf);
}

// ----- Background worker loops -----
static void network_worker(AppState* s) {
    const std::string& script = resolve_path("test_network.sh");
    if (script.empty()) {
        s->serial.store(ProbeState::Unknown);
        return;
    }

    // Redirect output to /dev/null to stay quiet.
    std::string cmd = "'" + script + "' >/dev/null 2>&1";
    using namespace std::chrono_literals;
    while (s->running.load()) {
        s->network.store(run_command_success(cmd.c_str()));
        // Sleep in small steps to react quickly to stop
        for (int i = 0; i < 40 && s->running.load(); ++i) std::this_thread::sleep_for(50ms);
    }
}

static void serial_worker(AppState* s) {
    // Resolve script only once. If missing, set Unknown and exit the worker.
    const std::string& script = resolve_path("test_serial.sh");
    if (script.empty()) {
        s->serial.store(ProbeState::Unknown);
        return;
    }

    // Redirect output to /dev/null to stay quiet.
    std::string cmd = "'" + script + "' >/dev/null 2>&1";
    using namespace std::chrono_literals;
    while (s->running.load()) {
        s->serial.store(run_command_success(cmd.c_str()));
        for (int i = 0; i < 40 && s->running.load(); ++i) std::this_thread::sleep_for(50ms);
    }
}

// ----- Periodic UI timer: refresh status line and the panel -----
struct UiRefs {
    AppState* state{};
    Fl_Box*   status_box{};
    StatusPanel* panel{};
};

static void ui_timer_cb(void* userdata) {
    UiRefs* ui = static_cast<UiRefs*>(userdata);
    if (ui && ui->status_box && ui->panel) {
        std::string line = make_status_line(*ui->state);
        ui->status_box->copy_label(line.c_str());
        ui->panel->redraw();
    }
    // Re-arm timer (5 Hz)
    Fl::repeat_timeout(0.2, ui_timer_cb, userdata);
}

// ----- main -----
int main(int argc, char** argv) {
    AppState state;

    // Window & basic layout
    const int W = 320, H = 200;
    Fl_Window win(W, H, "Net & Serial Monitor");

    // Panel area (top)
    StatusPanel panel(10, 10, W - 20, 160, &state);

    // One-line status box (non-editable)
    Fl_Box status_box(0, H - 20, W, 20);
    status_box.box(FL_EMBOSSED_BOX);
    status_box.labelsize(14);
    status_box.copy_label("network=unknown, serial=unknown");

    // Exit button (bottom-right)
    Fl_Button exit_btn(W - 110, H - 60, 100, 30, "Exit");

    // Handle exit: stop workers, close window
    exit_btn.callback(
        [](Fl_Widget*, void* v) {
            auto* st = static_cast<AppState*>(v);
            st->running.store(false);
            // Hide all windows to make Fl::run() return
            if (Fl::first_window()) Fl::first_window()->hide();
        },
        &state
    );

    // Also stop on window close
    win.callback(
        [](Fl_Widget*, void* v) {
            auto* st = static_cast<AppState*>(v);
            st->running.store(false);
            if (Fl::first_window()) Fl::first_window()->hide();
        },
        &state
    );

    win.end();
    win.show(argc, argv);

    // Start background threads
    std::thread t_network(network_worker, &state);
    std::thread t_ser(serial_worker, &state);

    // Start periodic UI timer
    UiRefs ui{&state, &status_box, &panel};
    Fl::add_timeout(0.2, ui_timer_cb, &ui);

    // Enter UI loop
    Fl::run();

    // Join workers and exit cleanly
    state.running.store(false);
    if (t_network.joinable()) t_network.join();
    if (t_ser.joinable())  t_ser.join();
    return 0;
}
