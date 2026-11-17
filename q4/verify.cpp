// Standalone checker. It takes the uncompressed CSQ4V12 certificate on stdin.
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

constexpr int order = 20;
constexpr int free_labels = 12;
constexpr int suffixes = 10395;
constexpr int words = (suffixes + 63) / 64;
constexpr int edges = 40;
constexpr int native = edges;

using Mask = std::uint32_t;
using Word = std::uint64_t;
using Mates = std::array<std::uint8_t, order>;
using Graph = std::array<Mask, order>;
using Bitmap = std::array<Word, words>;
using Edge = std::pair<int, int>;

constexpr Mask bit(int v) { return Mask{1} << v; }
constexpr Mask all = bit(order) - 1;
constexpr Mask all_free = bit(free_labels) - 1;
constexpr Mask whole_bank = bit(order);
constexpr Word all_incoming = (Word{1} << edges) - 1;

int first(Mask& set) {
    int v = std::countr_zero(set);
    set ^= bit(v);
    return v;
}

int last(Mask& set) {
    int v = std::bit_width(set) - 1;
    set ^= bit(v);
    return v;
}

void add(Graph& graph, int u, int v) {
    graph[u] |= bit(v);
    graph[v] |= bit(u);
}

template<class Emit>
void matchings(Mask unused, const Graph& allowed, Mates& mates, const Emit& emit) {
    if (!unused) {
        emit(mates);
        return;
    }
    int u = last(unused);
    for (Mask choices = allowed[u] & unused; choices;) {
        int v = last(choices);
        mates[u] = v;
        mates[v] = u;
        matchings(unused ^ bit(v), allowed, mates, emit);
    }
}

struct Block {
    std::vector<Edge> outgoing;
    std::array<Word, order> stars{};
    Graph incoming{};
    std::array<std::vector<Mates>, edges + 1> banks;

    Block() {
        Graph graph{};
        std::array<int, order> seam{};
        auto p = [](int h, int j) { return 5 * h + j % 5; };
        auto q = [](int k, int j) { return 10 + 5 * k + j % 5; };
        for (int h = 0; h < 2; ++h) {
            for (int j = 0; j < 5; ++j) {
                add(graph, p(h, j), p(h, j + 1));
                add(graph, q(h, j), q(h, j + 2));
                seam[p(h, j)] = p(h, 2 * j + 3 * h);
                seam[q(h, j)] = q(h, 2 * j + 4 + 2 * h);
                for (int k = 0; k < 2; ++k) add(graph, p(h, j), q(k, j + h * k));
            }
        }
        for (int u = 0; u < order; ++u)
            for (int v = u + 1; v < order; ++v)
                if (graph[u] & bit(v)) outgoing.emplace_back(u, v);
        if (outgoing.size() != edges) throw std::runtime_error("wrong edge count");

        Mates mates{};
        for (int i = 0; i < edges; ++i) {
            auto [u, v] = outgoing[i];
            stars[u] |= Word{1} << i;
            stars[v] |= Word{1} << i;
            int a = seam[u], b = seam[v];
            if (graph[a] & bit(b)) throw std::runtime_error("seam creates a square");
            add(incoming, a, b);
            mates[a] = b;
            mates[b] = a;
            matchings(all ^ bit(a) ^ bit(b), graph, mates,
                      [&](const Mates& m) { banks[i].push_back(m); });
        }
        matchings(all, graph, mates, [&](const Mates& m) { banks[native].push_back(m); });
    }
};

Word field(int u, int v) {
    return Word(v) << (4 * u);
}

bool connected(const Mates& a, const Mates& b) {
    Mask seen = 1, pending = 1;
    while (pending) {
        int u = first(pending);
        Mask next = (bit(a[u]) | bit(b[u])) & ~seen;
        seen |= next;
        pending |= next;
    }
    return seen == all_free;
}

struct Table {
    std::unordered_map<Word, std::size_t> index;
    std::vector<Bitmap> accepts;
    Bitmap full;

    Table() : accepts(suffixes) {
        Graph complete{};
        complete.fill(all_free);
        std::vector<Mates> patterns;
        Mates mates{};
        index.reserve(suffixes);
        matchings(all_free, complete, mates, [&](const Mates& m) {
            Word key = 0;
            for (int v = 0; v < free_labels; ++v) key |= field(v, m[v]);
            if (!index.emplace(key, patterns.size()).second)
                throw std::runtime_error("duplicate endpoint pattern");
            patterns.push_back(m);
        });
        if (patterns.size() != suffixes) throw std::runtime_error("wrong suffix count");
        full.fill(~Word{0});
        full.back() = (Word{1} << (suffixes % 64)) - 1;
        for (int a = 0; a < suffixes; ++a) {
            for (int b = 0; b < a; ++b) {
                if (!connected(patterns[a], patterns[b])) continue;
                accepts[a][b / 64] |= Word{1} << (b % 64);
                accepts[b][a / 64] |= Word{1} << (a % 64);
            }
        }
        for (const auto& row : accepts) {
            int count = 0;
            for (Word w : row) count += std::popcount(w);
            if (count != 3840) throw std::runtime_error("wrong compatibility count");
        }
    }
};

struct Checker {
    Block block;
    Table table;
    Mates prefix{};
    std::array<int, order> dense{};
    std::array<Bitmap, edges> good{};
    std::array<Bitmap, order> isolated{};
    std::uint64_t groups = 0;
    std::uint64_t selected = 0;
    std::uint64_t matching_suffixes = 0;
    std::uint64_t whole = 0;

    // Start at degree-one endpoints and walk through the original vertices.
    // A closed component is unreachable and leaves some vertices unseen.
    bool project(const Mates& q, Mask live, Word& key) const {
        Mask seen = live;
        for (Mask ends = live; ends;) {
            int u = first(ends);
            int v = q[u];
            while (!(live & bit(v))) {
                int w = prefix[v];
                seen |= bit(v) | bit(w);
                v = q[w];
            }
            if (!(ends & bit(v))) throw std::runtime_error("bad path endpoints");
            ends ^= bit(v);
            key |= field(dense[u], dense[v]) | field(dense[v], dense[u]);
        }
        return seen == all;
    }

    Bitmap twice(int row, Mask live) const {
        Bitmap one{}, two{};
        // Each original matching contributes once, even if its key repeats.
        for (const Mates& q : block.banks[row]) {
            Word key = 0;
            if (!project(q, live, key)) continue;
            const Bitmap& works = table.accepts[table.index.at(key)];
            for (int w = 0; w < words; ++w) {
                two[w] |= one[w] & works[w];
                one[w] |= works[w];
            }
        }
        return two;
    }

    void verify(Mask live, Word selection) {
        int rank = 0;
        for (Mask scan = live; scan;) dense[first(scan)] = rank++;
        if (twice(native, live) != table.full)
            throw std::runtime_error("fewer than two native completions");
        for (int i = 0; i < edges; ++i)
            good[i] = selection & (Word{1} << i) ? twice(i, live) : Bitmap{};

        Mask eligible = 0;
        for (int u = 0; u < order; ++u) {
            Word incident = block.stars[u];
            if (incident & ~selection) continue;
            Bitmap iso = table.full;
            while (incident) {
                int i = std::countr_zero(incident);
                incident &= incident - 1;
                for (int w = 0; w < words; ++w) iso[w] &= good[i][w];
            }
            if (std::any_of(iso.begin(), iso.end(), [](Word w) { return w != 0; })) {
                isolated[u] = iso;
                eligible |= bit(u);
            }
        }
        Bitmap need = table.full;
        for (Mask scan = eligible; scan;) {
            int u = first(scan);
            for (Mask peers = scan & ~block.incoming[u]; peers;) {
                int v = first(peers);
                for (int w = 0; w < words; ++w) need[w] &= ~(isolated[u][w] & isolated[v][w]);
            }
        }
        for (int w = 0; w < words; ++w) {
            Word uncovered = need[w];
            if (!uncovered) continue;
            matching_suffixes += std::popcount(uncovered);
            for (int u = 0; u < order; ++u) {
                Word seen_bad = 0;
                for (Word star = block.stars[u]; star;) {
                    int i = std::countr_zero(star);
                    star &= star - 1;
                    Word bad = uncovered & ~good[i][w];
                    if (bad & seen_bad) throw std::runtime_error("row mask leaves an uncovered suffix");
                    seen_bad |= bad;
                }
            }
        }
    }

    void visit(Mask unused, int depth) {
        if (depth == 4) {
            std::array<unsigned char, 3> bytes{};
            if (!std::cin.read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
                throw std::runtime_error("truncated vertex mask");
            Mask record = 0;
            for (int i = 0; i < 3; ++i) record |= Mask(bytes[i]) << (8 * i);
            if (record > whole_bank) throw std::runtime_error("vertex mask out of range");
            Word selection = record == whole_bank ? all_incoming : 0;
            whole += record == whole_bank;
            for (Mask scan = record & all; scan;) selection |= block.stars[first(scan)];
            verify(unused, selection);
            ++groups;
            selected += std::popcount(selection);
            return;
        }
        int a = first(unused);
        for (Mask scan = unused; scan;) {
            int b = first(scan);
            prefix[a] = b;
            prefix[b] = a;
            visit(unused ^ bit(b), depth + 1);
        }
    }
};

int main() {
    std::uint64_t position = 0;
    try {
        constexpr std::string_view magic = "CSQ4V12\n";
        std::array<char, magic.size()> header{};
        if (!std::cin.read(header.data(), header.size()) ||
            std::string_view(header.data(), header.size()) != magic)
            throw std::runtime_error("bad certificate header");
        Checker checker;
        try {
            checker.visit(all, 0);
        } catch (...) {
            position = checker.groups + 1;
            throw;
        }
        if (std::cin.get() != std::char_traits<char>::eof() || std::cin.bad())
            throw std::runtime_error("unexpected data after last vertex mask");
        if (checker.groups != 62985 || checker.groups * suffixes != 654729075)
            throw std::runtime_error("wrong prefix coverage");
        std::cout << "PASS both finite conditions on " << checker.groups * suffixes
                  << " labelled pairings in " << checker.groups << " prefixes\n"
                  << "selected incoming banks " << checker.selected
                  << ", full-bank records " << checker.whole
                  << ", suffixes verified by the matching branch " << checker.matching_suffixes << '\n';
    } catch (const std::exception& e) {
        std::cerr << e.what();
        if (position) std::cerr << " at record " << position;
        std::cerr << '\n';
        return 2;
    }
}
