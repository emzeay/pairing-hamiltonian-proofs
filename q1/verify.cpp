#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numeric>
#include <unordered_map>
#include <utility>
#include <vector>

using Mask = std::uint32_t;
using Edge = std::pair<int, int>;
constexpr int vertices = 8;
constexpr int states = 764;
constexpr int words = (states + 63) / 64;
using StateSet = std::array<std::uint64_t, words>;

struct PartialMatching {
    Mask edges;
    unsigned support;
};

struct Transition {
    StateSet open{};
    bool can_finish = false;
};

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    std::vector<Edge> edges;
    std::array<std::array<Mask, vertices>, vertices> edge_bit{};
    Mask native = 0;
    for (int a = 0; a < vertices; ++a) {
        for (int b = a + 1; b < vertices; ++b) {
            Mask bit = Mask{1} << edges.size();
            edges.emplace_back(a, b);
            edge_bit[a][b] = edge_bit[b][a] = bit;
            if (std::has_single_bit(unsigned(a ^ b))) native |= bit;
        }
    }

    std::vector<PartialMatching> boundary{{0, 0}};
    std::vector<Mask> letters;
    std::vector<PartialMatching> cube_matchings;
    std::unordered_map<Mask, int> index;
    // Enumerate all boundary matchings by skipping or adding each possible edge.
    for (std::size_t j = 0; j < edges.size(); ++j) {
        const auto [a, b] = edges[j];
        unsigned ends = (1u << a) | (1u << b);
        const auto prior = boundary.size();
        for (std::size_t i = 0; i < prior; ++i) {
            const auto item = boundary[i];
            if (!(item.support & ends)) {
                boundary.push_back({item.edges | (Mask{1} << j), item.support | ends});
            }
        }
    }
    for (std::size_t i = 0; i < boundary.size(); ++i) {
        const auto item = boundary[i];
        index.emplace(item.edges, i);
        if (item.support == 255) letters.push_back(item.edges);
        if (!(item.edges & ~native)) cube_matchings.push_back(item);
    }
    if (boundary.size() != states || letters.size() != 105) return 1;

    std::vector<Transition> table(boundary.size() * letters.size());
    for (std::size_t i = 0; i < boundary.size(); ++i) {
        const auto incoming = boundary[i];
        for (std::size_t j = 0; j < letters.size(); ++j) {
            auto& transition = table[i * letters.size() + j];
            for (const auto inner : cube_matchings) {
                if (incoming.support & inner.support) continue;
                const unsigned ports = 255 ^ (incoming.support | inner.support);
                const Mask graph = incoming.edges | letters[j] | inner.edges;
                std::array<int, vertices> parent;
                std::iota(parent.begin(), parent.end(), 0);
                auto root = [&](int x) {
                    while (parent[x] != x) x = parent[x];
                    return x;
                };
                for (Mask remaining = graph; remaining; remaining &= remaining - 1) {
                    const auto [a, b] = edges[std::countr_zero(remaining)];
                    parent[root(a)] = root(b);
                }
                std::array<unsigned, vertices> endpoints{};
                int component_count = 0;
                for (int v = 0; v < vertices; ++v) {
                    if (root(v) == v) ++component_count;
                    if ((ports >> v) & 1) endpoints[root(v)] |= 1u << v;
                }
                if (ports == 0) {
                    transition.can_finish |= component_count == 1;
                    continue;
                }

                Mask outgoing = 0;
                int path_count = 0;
                for (unsigned ends : endpoints) {
                    if (std::popcount(ends) != 2) continue;
                    outgoing |= edge_bit[std::countr_zero(ends)][std::bit_width(ends) - 1];
                    ++path_count;
                }
                if (path_count == component_count) {
                    const auto target = index.at(outgoing);
                    transition.open[target / 64] |= std::uint64_t{1} << (target % 64);
                }
            }
        }
    }

    std::ifstream file(argv[1]);
    if (!file) return 1;
    std::vector<std::pair<int, StateSet>> certificate;
    std::vector<std::vector<int>> members;
    bool has_initial = false;
    int size = 0;
    while (file >> size) {
        if (size <= 0 || size > states) return 1;
        StateSet set{};
        std::vector<int> options;
        for (int j = 0; j < size; ++j) {
            Mask code;
            if (!(file >> code) || !index.contains(code)) return 1;
            int id = index.at(code);
            auto bit = std::uint64_t{1} << (id % 64);
            if (set[id / 64] & bit) return 1;
            set[id / 64] |= bit;
            options.push_back(id);
        }
        if (size == 1 && options.front() == index.at(0)) has_initial = true;
        if (size != 1 && (set[0] & 1)) return 1;
        certificate.emplace_back(size, set);
        members.push_back(std::move(options));
    }
    if (!file.eof() || !has_initial) return 1;
    // Index witness sets by their smallest member. A subset witness must
    // have its smallest member present in the image.
    std::vector<std::vector<std::pair<int, StateSet>>> by_anchor(states);
    for (std::size_t i = 0; i < certificate.size(); ++i) {
        const auto anchor = *std::min_element(members[i].begin(), members[i].end());
        by_anchor[anchor].push_back(certificate[i]);
    }
    for (auto& bucket : by_anchor) std::sort(bucket.begin(), bucket.end());

    std::uint64_t verified = 0;
    for (std::size_t i = 0; i < members.size(); ++i) {
        for (std::size_t j = 0; j < letters.size(); ++j) {
            StateSet image{};
            bool can_finish = false;
            for (int source : members[i]) {
                const auto& transition = table[source * letters.size() + j];
                can_finish |= transition.can_finish;
                for (int word = 0; word < words; ++word) {
                    image[word] |= transition.open[word];
                }
            }
            if (!can_finish) {
                std::cerr << "No accepting transition at " << i << ' ' << j << '\n';
                return 1;
            }
            int image_size = 0;
            for (auto word : image) image_size += std::popcount(word);
            bool witnessed = false;
            for (int w = 0; w < words && !witnessed; ++w) {
                for (auto options = image[w]; options && !witnessed; options &= options - 1) {
                    const int anchor = 64 * w + std::countr_zero(options);
                    for (const auto& [count, candidate] : by_anchor[anchor]) {
                        if (count > image_size) break;
                        int k = 0;
                        while (k < words && !(candidate[k] & ~image[k])) ++k;
                        if (k == words) {
                            witnessed = true;
                            break;
                        }
                    }
                }
            }
            if (!witnessed) {
                std::cerr << "Certificate fails closure at " << i << ' ' << j << '\n';
                return 1;
            }
            ++verified;
        }
    }
    std::cout << boundary.size() << " boundary matchings, " << letters.size()
              << " local pairings, " << cube_matchings.size() << " native partial matchings\n"
              << certificate.size() << " certificate sets, "
              << verified << " acceptance and closure checks passed\n";
}
