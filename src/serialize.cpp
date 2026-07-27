#include "abalone/serialize.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace abalone {
namespace {

// --- little-endian appenders ------------------------------------------------

void put_u8(std::vector<std::uint8_t>& buf, std::uint8_t v) { buf.push_back(v); }

void put_u16(std::vector<std::uint8_t>& buf, std::uint16_t v) {
    buf.push_back(static_cast<std::uint8_t>(v & 0xFF));
    buf.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}

void put_u32(std::vector<std::uint8_t>& buf, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) buf.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
}

void put_u64(std::vector<std::uint8_t>& buf, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) buf.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
}

void put_f32(std::vector<std::uint8_t>& buf, float v) {
    std::uint32_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    put_u32(buf, bits);
}

// Index of a coordinate in Board::cells(), 0..60. Only used at export time, so
// the linear scan is fine.
int cell_index(Coord c) {
    const std::vector<Coord>& cells = Board::cells();
    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (cells[i] == c) return static_cast<int>(i);
    }
    return 0;  // unreachable for on-board coords
}

// --- Base64 -----------------------------------------------------------------

std::string base64(const std::vector<std::uint8_t>& in) {
    static const char* table =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    std::size_t i = 0;
    for (; i + 3 <= in.size(); i += 3) {
        const std::uint32_t n = (in[i] << 16) | (in[i + 1] << 8) | in[i + 2];
        out.push_back(table[(n >> 18) & 63]);
        out.push_back(table[(n >> 12) & 63]);
        out.push_back(table[(n >> 6) & 63]);
        out.push_back(table[n & 63]);
    }
    const std::size_t rem = in.size() - i;
    if (rem == 1) {
        const std::uint32_t n = in[i] << 16;
        out.push_back(table[(n >> 18) & 63]);
        out.push_back(table[(n >> 12) & 63]);
        out.push_back('=');
        out.push_back('=');
    } else if (rem == 2) {
        const std::uint32_t n = (in[i] << 16) | (in[i + 1] << 8);
        out.push_back(table[(n >> 18) & 63]);
        out.push_back(table[(n >> 12) & 63]);
        out.push_back(table[(n >> 6) & 63]);
        out.push_back('=');
    }
    return out;
}

// --- shared pieces ----------------------------------------------------------

void put_header(std::vector<std::uint8_t>& buf, std::uint8_t kind, Opening opening) {
    put_u8(buf, 0xAB);                                                  // magic
    put_u8(buf, 0x01);                                                  // version
    put_u8(buf, kind);
    put_u8(buf, static_cast<std::uint8_t>(opening == Opening::kClassic ? 0 : 1));
}

// Two bytes packing one move's replayable fields.
void put_move(std::vector<std::uint8_t>& buf, const Move& m) {
    const std::uint32_t packed =
        (static_cast<std::uint32_t>(cell_index(m.head) & 0x3F)) |
        (static_cast<std::uint32_t>((m.count - 1) & 0x03) << 6) |
        (static_cast<std::uint32_t>(static_cast<int>(m.line_dir) & 0x07) << 8) |
        (static_cast<std::uint32_t>(static_cast<int>(m.dir) & 0x07) << 11) |
        (static_cast<std::uint32_t>(m.inline_move ? 1 : 0) << 14);
    put_u16(buf, static_cast<std::uint16_t>(packed));
}

void put_stats(std::vector<std::uint8_t>& buf, const MoveReport& r, unsigned mask) {
    if (mask & kStatTime)  put_u32(buf, static_cast<std::uint32_t>(r.elapsed.count()));
    if (mask & kStatNodes) put_u64(buf, r.nodes);
    if (mask & kStatEvals) put_u64(buf, r.evals);
    if (mask & kStatScore) {
        put_u8(buf, r.score ? 1 : 0);
        if (r.score) put_f32(buf, static_cast<float>(*r.score));
    }
    if (mask & kStatFlags) {
        put_u8(buf, static_cast<std::uint8_t>((r.timed_out ? 1 : 0) | (r.forfeited ? 2 : 0)));
    }
}

std::string encode_game_impl(const Game& game, bool with_stats, unsigned mask) {
    std::vector<std::uint8_t> buf;
    put_header(buf, with_stats ? 2 : 1, game.config().opening);

    const std::vector<MoveReport>& hist = game.history();
    put_u16(buf, static_cast<std::uint16_t>(hist.size()));
    if (with_stats) put_u8(buf, static_cast<std::uint8_t>(mask & 0xFF));

    for (const MoveReport& r : hist) {
        put_move(buf, r.move);
        if (with_stats) put_stats(buf, r, mask);
    }
    return base64(buf);
}

}  // namespace

std::string encode_board(const Board& board, Player to_move, int ply, Opening opening) {
    // A single readable line: header, metadata, then the occupied cells only.
    std::ostringstream os;
    os << "abalone/board v1"
       << " opening=" << (opening == Opening::kClassic ? "classic" : "belgian")
       << " turn=" << (to_move == Player::kBlack ? 'b' : 'w')
       << " ply=" << ply
       << " losses=" << board.losses(Player::kBlack) << ',' << board.losses(Player::kWhite)
       << " cells=";

    bool first = true;
    for (const Coord& c : Board::cells()) {
        const Cell v = board.at(c);
        if (v != Cell::kBlack && v != Cell::kWhite) continue;
        if (!first) os << ',';
        first = false;
        os << coord_to_string(c) << ':' << (v == Cell::kBlack ? 'b' : 'w');
    }
    return os.str();
}

std::string encode_game(const Game& game) {
    return encode_game_impl(game, /*with_stats=*/false, 0);
}

std::string encode_game_with_stats(const Game& game, unsigned stat_mask) {
    return encode_game_impl(game, /*with_stats=*/true, stat_mask);
}

// --- decoding ---------------------------------------------------------------

namespace {

// Base64 decode. Returns false on any character outside the alphabet (padding
// aside), so a truncated or corrupted blob is rejected rather than misread.
bool unbase64(const std::string& in, std::vector<std::uint8_t>* out) {
    auto value = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    out->clear();
    std::uint32_t acc = 0;
    int bits = 0;
    for (char c : in) {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
        const int v = value(c);
        if (v < 0) return false;
        acc = (acc << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out->push_back(static_cast<std::uint8_t>((acc >> bits) & 0xFF));
        }
    }
    return true;
}

// Cursor over a decoded byte buffer, with bounds-checked little-endian reads.
struct Reader {
    const std::vector<std::uint8_t>& buf;
    std::size_t pos = 0;
    bool ok = true;

    bool has(std::size_t n) const { return pos + n <= buf.size(); }

    std::uint8_t u8() {
        if (!has(1)) { ok = false; return 0; }
        return buf[pos++];
    }
    std::uint16_t u16() {
        if (!has(2)) { ok = false; return 0; }
        std::uint16_t v = static_cast<std::uint16_t>(buf[pos] | (buf[pos + 1] << 8));
        pos += 2;
        return v;
    }
    std::uint32_t u32() {
        if (!has(4)) { ok = false; return 0; }
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(buf[pos + i]) << (8 * i);
        pos += 4;
        return v;
    }
    std::uint64_t u64() {
        if (!has(8)) { ok = false; return 0; }
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v |= static_cast<std::uint64_t>(buf[pos + i]) << (8 * i);
        pos += 8;
        return v;
    }
    float f32() {
        std::uint32_t bits = u32();
        float v;
        std::memcpy(&v, &bits, sizeof(v));
        return v;
    }
};

// Reads and validates the common 4-byte header, returning the kind byte and the
// opening. Sets r.ok = false on a bad magic/version.
int read_header(Reader& r, Opening* opening) {
    if (r.u8() != 0xAB || r.u8() != 0x01) { r.ok = false; return -1; }
    const int kind = r.u8();
    *opening = (r.u8() == 0) ? Opening::kClassic : Opening::kBelgianDaisy;
    return kind;
}

// Finds the legal move in `board` for `p` matching the decoded fields, or
// returns false if none does (a corrupt or foreign blob).
bool match_move(const Board& board, Player p, Coord head, int count, Direction line_dir,
                Direction dir, bool inl, Move* out) {
    for (const Move& m : generate_moves(board, p)) {
        if (m.head == head && m.count == count && m.line_dir == line_dir && m.dir == dir &&
            m.inline_move == inl) {
            *out = m;
            return true;
        }
    }
    return false;
}

}  // namespace

int blob_kind(const std::string& blob) {
    // The board form is plain text with a fixed prefix; everything else is a
    // Base64 game blob whose header carries the kind (1 or 2).
    if (blob.rfind("abalone/board", 0) == 0) return 0;

    std::vector<std::uint8_t> buf;
    if (!unbase64(blob, &buf)) return -1;
    Reader r{buf};
    Opening opening;
    const int kind = read_header(r, &opening);
    if (!r.ok || kind < 1 || kind > 2) return -1;
    return kind;
}

namespace {

// Splits `s` on `sep` into non-empty pieces.
std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start <= s.size()) {
        const std::size_t at = s.find(sep, start);
        const std::string piece =
            s.substr(start, at == std::string::npos ? std::string::npos : at - start);
        if (!piece.empty()) out.push_back(piece);
        if (at == std::string::npos) break;
        start = at + 1;
    }
    return out;
}

}  // namespace

bool decode_board(const std::string& blob, Board* board, Player* to_move, int* ply,
                  Opening* opening) {
    std::istringstream is(blob);
    std::string tag, version;
    is >> tag >> version;
    if (tag != "abalone/board") return false;

    Opening op = Opening::kClassic;
    Player mover = Player::kBlack;
    int p = 0, black_losses = 0, white_losses = 0;
    Board b;  // empty

    std::string tok;
    while (is >> tok) {
        const std::size_t eq = tok.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = tok.substr(0, eq);
        const std::string val = tok.substr(eq + 1);

        if (key == "opening") {
            if (!parse_opening(val, &op)) return false;
        } else if (key == "turn") {
            mover = (val == "b") ? Player::kBlack : Player::kWhite;
        } else if (key == "ply") {
            p = std::atoi(val.c_str());
        } else if (key == "losses") {
            const std::vector<std::string> parts = split(val, ',');
            if (parts.size() != 2) return false;
            black_losses = std::atoi(parts[0].c_str());
            white_losses = std::atoi(parts[1].c_str());
        } else if (key == "cells") {
            for (const std::string& cell : split(val, ',')) {
                const std::size_t colon = cell.find(':');
                if (colon == std::string::npos) return false;
                Coord c;
                if (!parse_coord(cell.substr(0, colon), &c)) return false;
                const char col = cell[colon + 1];
                if (col == 'b')      b.set(c, Cell::kBlack);
                else if (col == 'w') b.set(c, Cell::kWhite);
                else                 return false;
            }
        }
    }

    for (int i = 0; i < black_losses; ++i) b.add_loss(Player::kBlack);
    for (int i = 0; i < white_losses; ++i) b.add_loss(Player::kWhite);

    if (board) *board = b;
    if (to_move) *to_move = mover;
    if (ply) *ply = p;
    if (opening) *opening = op;
    return true;
}

bool decode_game(const std::string& blob, DecodedGame* out) {
    std::vector<std::uint8_t> buf;
    if (!unbase64(blob, &buf)) return false;
    Reader r{buf};
    Opening op;
    const int kind = read_header(r, &op);
    if (!r.ok || (kind != 1 && kind != 2)) return false;

    const int move_count = r.u16();
    const unsigned mask = (kind == 2) ? r.u8() : 0u;
    if (!r.ok) return false;

    DecodedGame g;
    g.opening = op;
    g.has_stats = (kind == 2);
    g.stat_mask = mask;

    Board board = Board::from_opening(op);
    Player p = Player::kBlack;
    const std::vector<Coord>& cells = Board::cells();

    for (int i = 0; i < move_count; ++i) {
        const std::uint16_t packed = r.u16();
        if (!r.ok) return false;

        const int head_index = packed & 0x3F;
        const int count = ((packed >> 6) & 0x03) + 1;
        const Direction line_dir = static_cast<Direction>((packed >> 8) & 0x07);
        const Direction dir = static_cast<Direction>((packed >> 11) & 0x07);
        const bool inl = ((packed >> 14) & 0x01) != 0;
        if (head_index < 0 || head_index >= static_cast<int>(cells.size())) return false;

        MoveReport report;
        if (!match_move(board, p, cells[head_index], count, line_dir, dir, inl, &report.move)) {
            return false;  // move not legal here: not a real game / wrong opening
        }

        if (kind == 2) {
            if (mask & kStatTime)  report.elapsed = std::chrono::milliseconds(r.u32());
            if (mask & kStatNodes) report.nodes = r.u64();
            if (mask & kStatEvals) report.evals = r.u64();
            if (mask & kStatScore) {
                if (r.u8()) report.score = static_cast<double>(r.f32());
            }
            if (mask & kStatFlags) {
                const std::uint8_t f = r.u8();
                report.timed_out = (f & 1) != 0;
                report.forfeited = (f & 2) != 0;
            }
            if (!r.ok) return false;
        }

        apply_move(&board, p, report.move);
        p = other(p);
        g.moves.push_back(report);
    }

    if (out) *out = std::move(g);
    return true;
}

}  // namespace abalone
