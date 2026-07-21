#include "abalone/gui.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "abalone/agent.hpp"
#include "abalone/move.hpp"

namespace abalone {
namespace {

// --- palette ----------------------------------------------------------------

const Color kBg          = {24, 26, 32, 255};
const Color kPanel       = {34, 37, 45, 255};
const Color kHole        = {44, 48, 58, 255};
const Color kBlackMarble = {28, 30, 36, 255};
const Color kWhiteMarble = {232, 231, 226, 255};
const Color kSelect      = {90, 200, 250, 255};
const Color kDest        = {80, 220, 140, 255};
const Color kPush        = {250, 170, 60, 255};
const Color kPushOff     = {240, 90, 90, 255};
const Color kInk         = {228, 229, 234, 255};
const Color kInkDim      = {140, 145, 158, 255};

// --- board geometry ---------------------------------------------------------
//
// The axial frame from board.hpp maps straight onto a hex layout: +1 col is
// due East, +1 row is "up". Shifting x by half a cell per row is all it takes
// to turn (row, col) into pixels.

struct Layout {
    Vector2 origin{};  // pixel position of cell (0, 0)
    float radius = 30.0f;
};

Vector2 cell_center(const Layout& lo, Coord c) {
    const float dx = lo.radius * 1.90f;
    const float dy = lo.radius * 1.66f;
    return Vector2{lo.origin.x + (static_cast<float>(c.col) - static_cast<float>(c.row) * 0.5f) * dx,
                   lo.origin.y - static_cast<float>(c.row) * dy};
}

// Centres the 61 cells inside `area`, picking the largest radius that fits.
Layout make_layout(Rectangle area) {
    Layout lo;
    lo.origin = Vector2{0, 0};
    lo.radius = 1.0f;

    // Measure the board at unit radius, then scale to the space available.
    float min_x = 1e9f, max_x = -1e9f, min_y = 1e9f, max_y = -1e9f;
    for (const Coord& c : Board::cells()) {
        const Vector2 p = cell_center(lo, c);
        min_x = std::min(min_x, p.x);
        max_x = std::max(max_x, p.x);
        min_y = std::min(min_y, p.y);
        max_y = std::max(max_y, p.y);
    }

    const float span_x = (max_x - min_x) + 2.0f;  // +2 for the marble itself
    const float span_y = (max_y - min_y) + 2.0f;
    lo.radius = std::min(area.width / span_x, area.height / span_y);

    // Re-measure at the real radius so we can centre it.
    min_x = 1e9f; max_x = -1e9f; min_y = 1e9f; max_y = -1e9f;
    for (const Coord& c : Board::cells()) {
        const Vector2 p = cell_center(lo, c);
        min_x = std::min(min_x, p.x);
        max_x = std::max(max_x, p.x);
        min_y = std::min(min_y, p.y);
        max_y = std::max(max_y, p.y);
    }
    lo.origin.x = area.x + area.width * 0.5f - (min_x + max_x) * 0.5f;
    lo.origin.y = area.y + area.height * 0.5f - (min_y + max_y) * 0.5f;
    return lo;
}

// The cell under the mouse, or an off-board Coord.
Coord cell_at(const Layout& lo, Vector2 p) {
    for (const Coord& c : Board::cells()) {
        if (CheckCollisionPointCircle(p, cell_center(lo, c), lo.radius)) return c;
    }
    return Coord{-1, -1};
}

// --- small widgets ----------------------------------------------------------

bool button(Rectangle r, const char* label, bool enabled = true) {
    const bool hot = enabled && CheckCollisionPointRec(GetMousePosition(), r);
    Color fill = kPanel;
    if (!enabled)    fill = Color{30, 32, 38, 255};
    else if (hot)    fill = Color{52, 57, 70, 255};

    DrawRectangleRounded(r, 0.22f, 6, fill);
    DrawRectangleRoundedLines(r, 0.22f, 6, hot ? kSelect : Color{60, 64, 76, 255});

    const int fs = 20;
    const int tw = MeasureText(label, fs);
    DrawText(label, static_cast<int>(r.x + (r.width - tw) * 0.5f),
             static_cast<int>(r.y + (r.height - fs) * 0.5f), fs, enabled ? kInk : kInkDim);

    return hot && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void centered(const char* text, int fs, float cx, float y, Color col) {
    DrawText(text, static_cast<int>(cx - MeasureText(text, fs) * 0.5f), static_cast<int>(y), fs, col);
}

// --- app state --------------------------------------------------------------

enum class Screen { kMenu, kSetup, kSettings, kPlay };

// A destination the human can click, derived from the legal move list.
struct Target {
    Coord cell{};
    Move move{};
};

struct App {
    GameConfig config;
    Screen screen = Screen::kMenu;
    bool quit = false;  // set by the Quit button; the window is closed once, by run_gui

    // Setup choices.
    bool human_black = true;
    bool human_white = true;
    int black_agent = 0;
    int white_agent = 0;

    // Live game.
    std::unique_ptr<Game> game;
    std::shared_ptr<Agent> black_ai;
    std::shared_ptr<Agent> white_ai;
    std::vector<Move> legal;
    std::vector<Coord> selection;
    std::vector<Target> targets;
    std::string banner;
    bool auto_play = true;
    double next_ai_move_at = 0.0;

    bool human_turn() const {
        if (!game) return false;
        return game->to_move() == Player::kBlack ? human_black : human_white;
    }
};

const std::vector<AgentEntry>& agents() { return AgentRegistry::instance().all(); }

// Recomputes the clickable destinations for the current selection.
//
// This is the whole point of the graphical UI: rather than asking the player to
// name a direction, every legal move whose marble set equals the selection is
// turned into a destination cell they can click.
void refresh_targets(App& app) {
    app.targets.clear();
    if (app.selection.empty()) return;

    std::vector<Coord> want = app.selection;
    std::sort(want.begin(), want.end(), [](Coord a, Coord b) {
        return a.row != b.row ? a.row < b.row : a.col < b.col;
    });

    for (const Move& m : app.legal) {
        std::vector<Coord> got = m.marbles();
        std::sort(got.begin(), got.end(), [](Coord a, Coord b) {
            return a.row != b.row ? a.row < b.row : a.col < b.col;
        });
        if (got != want) continue;

        // Inline moves advance along their own axis, so the head's landing
        // square is the one that reads as "the destination". Broadside moves
        // shift every marble sideways, so each marble offers a landing square.
        if (m.inline_move) {
            const Coord dest = step(m.head, m.dir);
            app.targets.push_back(Target{dest, m});
        } else {
            for (const Coord& c : got) app.targets.push_back(Target{step(c, m.dir), m});
        }
    }
}

void begin_turn(App& app) {
    app.legal = app.game->legal_moves();
    app.selection.clear();
    app.targets.clear();
    if (!app.human_turn()) app.next_ai_move_at = GetTime() + 0.35;
}

void start_game(App& app) {
    app.game = std::make_unique<Game>(app.config);
    app.black_ai.reset();
    app.white_ai.reset();
    if (!app.human_black && !agents().empty()) app.black_ai = agents()[app.black_agent].factory();
    if (!app.human_white && !agents().empty()) app.white_ai = agents()[app.white_agent].factory();
    app.banner.clear();
    app.screen = Screen::kPlay;
    begin_turn(app);
}

bool selected(const App& app, Coord c) {
    return std::find(app.selection.begin(), app.selection.end(), c) != app.selection.end();
}

void play(App& app, const Move& m) {
    MoveReport report;
    report.move = m;
    app.game->play(m, report);
    begin_turn(app);
}

// --- board drawing ----------------------------------------------------------

void draw_marble(Vector2 p, float r, Cell what) {
    const Color base = (what == Cell::kBlack) ? kBlackMarble : kWhiteMarble;
    DrawCircleV(p, r, base);
    // A brighter rim plus an offset highlight reads as a sphere without any art.
    DrawCircleLinesV(p, r, (what == Cell::kBlack) ? Color{70, 74, 86, 255}
                                                  : Color{170, 168, 160, 255});
    DrawCircleV(Vector2{p.x - r * 0.30f, p.y - r * 0.32f}, r * 0.22f,
                (what == Cell::kBlack) ? Color{255, 255, 255, 28} : Color{255, 255, 255, 150});
}

void draw_board(App& app, const Layout& lo) {
    const Board& b = app.game->board();
    const Coord hover = cell_at(lo, GetMousePosition());

    // Cells and marbles.
    for (const Coord& c : Board::cells()) {
        const Vector2 p = cell_center(lo, c);
        DrawCircleV(p, lo.radius * 0.92f, kHole);

        const Cell what = b.at(c);
        if (what == Cell::kBlack || what == Cell::kWhite) {
            draw_marble(p, lo.radius * 0.80f, what);
        }
        if (selected(app, c)) {
            DrawCircleLinesV(p, lo.radius * 0.88f, kSelect);
            DrawCircleLinesV(p, lo.radius * 0.84f, kSelect);
        }
        if (app.human_turn() && c == hover && !selected(app, c) &&
            what == to_cell(app.game->to_move())) {
            DrawCircleLinesV(p, lo.radius * 0.88f, kInkDim);
        }
    }

    // Destination markers, drawn on top so they are never hidden by a marble.
    for (const Target& t : app.targets) {
        const Vector2 p = cell_center(lo, t.cell);
        Color col = kDest;
        if (t.move.pushes_off)   col = kPushOff;
        else if (t.move.pushed)  col = kPush;

        const bool hot = CheckCollisionPointCircle(GetMousePosition(), p, lo.radius);
        DrawCircleV(p, lo.radius * (hot ? 0.46f : 0.32f), Fade(col, hot ? 0.95f : 0.65f));
        DrawCircleLinesV(p, lo.radius * 0.80f, Fade(col, hot ? 1.0f : 0.5f));
    }

    // Rank labels down the left edge.
    for (int row = 0; row < kRows; ++row) {
        const Coord first{row, row <= 4 ? 0 : row - 4};
        const Vector2 p = cell_center(lo, first);
        const char label[2] = {static_cast<char>('A' + row), '\0'};
        DrawText(label, static_cast<int>(p.x - lo.radius * 2.0f),
                 static_cast<int>(p.y - 10), 20, kInkDim);
    }
}

void draw_side_panel(App& app, Rectangle r) {
    DrawRectangleRounded(r, 0.05f, 6, kPanel);
    const float cx = r.x + r.width * 0.5f;
    float y = r.y + 18;

    const Game& g = *app.game;
    centered(TextFormat("Ply %d", g.ply()), 22, cx, y, kInk);
    y += 38;

    const char* turn = player_name(g.to_move());
    centered(app.game->over() ? result_name(g.result())
                              : TextFormat("%s to move", turn),
             24, cx, y, app.game->over() ? kPush : kInk);
    y += 44;

    // Losses: six marbles lost ends the game, so show it as a tally.
    for (Player p : {Player::kBlack, Player::kWhite}) {
        const int lost = g.board().losses(p);
        DrawText(TextFormat("%s lost %d/%d", player_name(p), lost, kMarblesToLose),
                 static_cast<int>(r.x + 18), static_cast<int>(y), 18, kInk);
        y += 24;
        for (int i = 0; i < kMarblesToLose; ++i) {
            const Vector2 p2{r.x + 26 + i * 22.0f, y + 8};
            if (i < lost) DrawCircleV(p2, 8, kPushOff);
            else          DrawCircleLinesV(p2, 8, kInkDim);
        }
        y += 34;
    }

    y += 6;
    if (!app.banner.empty()) {
        DrawText(app.banner.c_str(), static_cast<int>(r.x + 18), static_cast<int>(y), 16, kPush);
    }
    y += 26;

    // Legend for the destination colours.
    struct { Color c; const char* t; } key[] = {
        {kDest,    "move"},
        {kPush,    "push"},
        {kPushOff, "push off the edge"},
    };
    for (const auto& k : key) {
        DrawCircleV(Vector2{r.x + 26, y + 8}, 7, k.c);
        DrawText(k.t, static_cast<int>(r.x + 42), static_cast<int>(y), 16, kInkDim);
        y += 24;
    }

    y = r.y + r.height - 132;
    if (!app.human_turn() && !app.game->over()) {
        if (button(Rectangle{r.x + 16, y, r.width - 32, 34},
                   app.auto_play ? "Pause AI" : "Step / Resume")) {
            app.auto_play = !app.auto_play;
        }
    }
    y += 42;
    if (button(Rectangle{r.x + 16, y, r.width - 32, 34}, "Restart")) start_game(app);
    y += 42;
    if (button(Rectangle{r.x + 16, y, r.width - 32, 34}, "Main menu")) app.screen = Screen::kMenu;
}

// --- input ------------------------------------------------------------------

void handle_board_click(App& app, const Layout& lo) {
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;
    if (app.game->over() || !app.human_turn()) return;

    const Coord c = cell_at(lo, GetMousePosition());
    if (!on_board(c)) {
        app.selection.clear();
        refresh_targets(app);
        return;
    }

    // Clicking a highlighted destination commits the move.
    for (const Target& t : app.targets) {
        if (t.cell == c) {
            play(app, t.move);
            return;
        }
    }

    // Otherwise the click edits the selection.
    const Cell what = app.game->board().at(c);
    if (what != to_cell(app.game->to_move())) {
        app.selection.clear();
    } else if (selected(app, c)) {
        app.selection.erase(std::find(app.selection.begin(), app.selection.end(), c));
    } else if (app.selection.size() < 3) {
        app.selection.push_back(c);
    } else {
        app.selection.assign(1, c);
    }

    refresh_targets(app);
    if (!app.selection.empty() && app.targets.empty()) {
        app.banner = "No legal move for that group.";
    } else {
        app.banner.clear();
    }
}

void step_ai(App& app) {
    if (app.game->over() || app.human_turn()) return;
    if (!app.auto_play || GetTime() < app.next_ai_move_at) return;

    const std::shared_ptr<Agent>& who =
        (app.game->to_move() == Player::kBlack) ? app.black_ai : app.white_ai;
    if (!who) {  // no agents registered; nothing we can do
        app.banner = "No agent available for this seat.";
        return;
    }

    const MoveReport r = app.game->play_agent_turn(who);
    app.banner = r.forfeited ? "Agent forfeited a turn (bug in the agent)." : "";
    begin_turn(app);
}

// --- screens ----------------------------------------------------------------

void screen_menu(App& app) {
    const float cx = GetScreenWidth() * 0.5f;
    centered("ABALONE", 64, cx, 90, kInk);
    centered("click marbles, then click where they should go", 20, cx, 166, kInkDim);

    const float w = 340, x = cx - w * 0.5f;
    float y = 240;
    const bool have_agents = !agents().empty();

    if (button(Rectangle{x, y, w, 52}, "Human vs Human")) {
        app.human_black = app.human_white = true;
        start_game(app);
    }
    y += 66;
    if (button(Rectangle{x, y, w, 52}, "Human vs AI", have_agents)) {
        app.human_black = true;
        app.human_white = false;
        app.screen = Screen::kSetup;
    }
    y += 66;
    if (button(Rectangle{x, y, w, 52}, "AI vs AI", have_agents)) {
        app.human_black = app.human_white = false;
        app.screen = Screen::kSetup;
    }
    y += 66;
    if (button(Rectangle{x, y, w, 52}, "Settings")) app.screen = Screen::kSettings;
    y += 66;
    // Never call CloseWindow() here: the rest of this frame, and the top of the
    // next loop iteration, would still be talking to a destroyed window.
    if (button(Rectangle{x, y, w, 52}, "Quit")) app.quit = true;

    if (!have_agents) {
        centered("No agents registered -- AI modes unavailable.", 18, cx,
                 GetScreenHeight() - 46.0f, kPushOff);
    }
}

void screen_setup(App& app) {
    const float cx = GetScreenWidth() * 0.5f;
    centered("Choose sides", 40, cx, 90, kInk);

    const float w = 420, x = cx - w * 0.5f;
    float y = 190;

    for (Player p : {Player::kBlack, Player::kWhite}) {
        const bool is_black = (p == Player::kBlack);
        const bool human = is_black ? app.human_black : app.human_white;
        int& pick = is_black ? app.black_agent : app.white_agent;

        DrawText(player_name(p), static_cast<int>(x), static_cast<int>(y), 22, kInk);
        y += 32;

        if (button(Rectangle{x, y, w * 0.45f, 44}, human ? "Human" : "AI")) {
            (is_black ? app.human_black : app.human_white) = !human;
        }
        if (!human && !agents().empty()) {
            const char* name = agents()[pick].name.c_str();
            if (button(Rectangle{x + w * 0.5f, y, w * 0.5f, 44}, name)) {
                pick = (pick + 1) % static_cast<int>(agents().size());
            }
        }
        y += 74;
    }

    y += 20;
    if (button(Rectangle{x, y, w * 0.48f, 50}, "Start")) start_game(app);
    if (button(Rectangle{x + w * 0.52f, y, w * 0.48f, 50}, "Back")) app.screen = Screen::kMenu;
}

void screen_settings(App& app) {
    const float cx = GetScreenWidth() * 0.5f;
    centered("Settings", 40, cx, 90, kInk);

    const float w = 460, x = cx - w * 0.5f;
    float y = 200;

    DrawText("Opening", static_cast<int>(x), static_cast<int>(y), 20, kInkDim);
    y += 28;
    if (button(Rectangle{x, y, w, 44}, opening_name(app.config.opening))) {
        app.config.opening = (app.config.opening == Opening::kClassic) ? Opening::kBelgianDaisy
                                                                      : Opening::kClassic;
    }
    y += 70;

    DrawText("Move limit (plies)", static_cast<int>(x), static_cast<int>(y), 20, kInkDim);
    y += 28;
    {
        const int cur = app.config.move_limit.value_or(0);
        if (button(Rectangle{x, y, w, 44}, cur ? TextFormat("%d", cur) : "uncapped")) {
            // Cycle through a few useful caps; uncapped stays the default.
            const int next = (cur == 0) ? 200 : (cur == 200) ? 400 : (cur == 400) ? 1000 : 0;
            app.config.move_limit = next ? std::optional<int>(next) : std::nullopt;
        }
    }
    y += 70;

    DrawText("Time per AI move", static_cast<int>(x), static_cast<int>(y), 20, kInkDim);
    y += 28;
    {
        const int cur = app.config.time_per_move
                            ? static_cast<int>(app.config.time_per_move->count())
                            : 0;
        if (button(Rectangle{x, y, w, 44}, cur ? TextFormat("%d ms", cur) : "unlimited")) {
            const int next = (cur == 0) ? 100 : (cur == 100) ? 500 : (cur == 500) ? 2000 : 0;
            app.config.time_per_move =
                next ? std::optional<std::chrono::milliseconds>(std::chrono::milliseconds(next))
                     : std::nullopt;
        }
    }
    y += 80;

    if (button(Rectangle{x, y, w, 50}, "Back")) app.screen = Screen::kMenu;
}

void screen_play(App& app) {
    const float panel_w = 300.0f;
    const Rectangle board_area{16, 16, GetScreenWidth() - panel_w - 48.0f,
                               GetScreenHeight() - 32.0f};
    const Layout lo = make_layout(board_area);

    step_ai(app);
    handle_board_click(app, lo);

    draw_board(app, lo);
    draw_side_panel(app, Rectangle{GetScreenWidth() - panel_w - 16.0f, 16, panel_w,
                                   GetScreenHeight() - 32.0f});
}

}  // namespace

int run_gui(GameConfig config) {
    // raylib logs every shader, texture and buffer it loads at INFO level, which
    // buries anything that actually matters. Keep warnings and errors.
    SetTraceLogLevel(LOG_WARNING);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1120, 780, "Abalone");
    SetWindowMinSize(900, 640);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);  // Esc backs out of a screen instead of killing the app

    App app;
    app.config = config;

    while (!WindowShouldClose() && !app.quit) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (app.screen == Screen::kMenu) break;
            app.screen = Screen::kMenu;
        }

        BeginDrawing();
        ClearBackground(kBg);
        switch (app.screen) {
            case Screen::kMenu:     screen_menu(app);     break;
            case Screen::kSetup:    screen_setup(app);    break;
            case Screen::kSettings: screen_settings(app); break;
            case Screen::kPlay:     screen_play(app);     break;
        }
        EndDrawing();
    }

    CloseWindow();
    return 0;
}

}  // namespace abalone
