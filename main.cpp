#include <algorithm>
#include <bit>
#include <bitset>
#include <cassert>
#include <compare>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <unordered_map>
#include <vector>

volatile bool interrupted = false;
void signal_handler(int) { interrupted = true; }

const size_t MAX_BATTLEFIELD_NUMBER = 32;

size_t iterations;
size_t N, BA, BD;
std::vector<uint32_t> vals;

struct strategy {
  std::bitset<MAX_BATTLEFIELD_NUMBER> play;

  strategy() {}
  strategy(u_long i): play(i) {}

  bool operator==(const strategy&) const = default;
};

template<>
struct std::hash<strategy> {
  std::size_t operator()(const strategy &s) const noexcept {
    return std::hash<uint32_t>{}(s.play.to_ulong());
  }
};


struct mixed_strategy {
  std::unordered_map<strategy, uint64_t> plays;
  uint64_t size;
  void add(strategy s) {
    plays[s]++;
    size++;
  }

  mixed_strategy(): plays{} {}
};

double u(strategy sa, strategy sd) {
  double res = 0;
  for (size_t i = 0; i < N; i++) {
    if (sa.play[i] && ~sd.play[i])
      res += vals[i];
  }
  return res;
}

double u(strategy sa, mixed_strategy msd) {
  double res = 0;
  for (const auto &[sd, p] : msd.plays)
    res += p * u(sa, sd);
  return res / msd.size;
}

double u(mixed_strategy msa, strategy sd) {
  double res = 0;
  for (const auto &[sa, p] : msa.plays)
    res += p * u(sa, sd);
  return res / msa.size;
}

double u(mixed_strategy msa, mixed_strategy msd) {
  double res = 0;
  for (const auto &[sa, pa] : msa.plays)
    for (const auto &[sd, pd] : msd.plays)
      res += pa * pd * u(sa, sd);
  return res / msa.size / msd.size;
}

std::vector<double> battlefield_set_probabilities(mixed_strategy s) {
  std::vector<double> set_probabilities(N);
  for (size_t i = 0; i < N; i++) {
    auto set_size = 0;
    for (const auto &simple_strategy: s.plays) {
      if (simple_strategy.first.play[i]) set_size += simple_strategy.second;
    }
    set_probabilities[i] = static_cast<double>(set_size) / static_cast<double>(s.size);
  }
  return set_probabilities;
}

std::pair<double, strategy> best_response_attacker(mixed_strategy msd) {
  std::vector<double> defended_probabilities = battlefield_set_probabilities(msd);
  std::vector<std::pair<double, size_t>> expected_score_attacked(N);
  for (size_t i = 0; i < N; i++) {
    expected_score_attacked[i].second = i;
  }

  for (size_t i = 0; i < N; i++) {
    auto battlefield_value = vals[i];
    auto not_defended_prob = 1 - defended_probabilities[i];
    expected_score_attacked[i].first =
      static_cast<double>(battlefield_value) * static_cast<double>(not_defended_prob);
  }

  sort(expected_score_attacked.begin(), expected_score_attacked.end(), std::greater());

  double expected_score = 0;
  strategy response{};
  for (size_t i = 0; i < BA; i++) {
    response.play[expected_score_attacked[i].second] = true;
    expected_score += expected_score_attacked[i].first;
  }
  return std::make_pair(expected_score, response);
}

std::pair<double, strategy> best_response_defender(mixed_strategy msa) {
  std::vector<double> attacked_probabilities = battlefield_set_probabilities(msa);
  std::vector<std::pair<double, size_t>> expected_score_not_defended(N);
  for (size_t i = 0; i < N; i++) {
    expected_score_not_defended[i].second = i;
  }

  for (size_t i = 0; i < N; i++) {
    auto battlefield_value = vals[i];
    expected_score_not_defended[i].first =
      - static_cast<double>(battlefield_value) * static_cast<double>(attacked_probabilities[i]);
  }

  sort(expected_score_not_defended.begin(), expected_score_not_defended.end(), std::less());

  double expected_score = 0;
  strategy response{};
  for (size_t i = 0; i < BD; i++) {
    response.play[expected_score_not_defended[i].second] = true;
  }
  for (size_t i = BD; i < N; i++) {
    expected_score += expected_score_not_defended[i].first;
  }
  return std::make_pair(-expected_score, response);
}

double what_approx(mixed_strategy msa, mixed_strategy msd) {
  double current = u(msa, msd);
  double best_attacker = best_response_attacker(msd).first;
  double best_defender = best_response_defender(msa).first;
  return std::max({0., best_attacker - current, -(best_defender - current)});
}

mixed_strategy uniform_strategy(size_t resources) {
  mixed_strategy ms;
  for (uint32_t i = 0; i < (1UL << N); i++) {
    strategy s{i};
    if (s.play.count() == resources)
      ms.plays[s] = 1;
  }
  ms.size = ms.plays.size();
  return ms;
}

void print_strategy(const mixed_strategy &ms, std::filesystem::path path) {
  std::ofstream csv(path);
  std::vector<std::pair<strategy, double>> x;
  x.reserve(ms.plays.size());
  for (const auto &[s, p] : ms.plays)
    x.emplace_back(s, static_cast<double>(p) / ms.size);
  std::sort(x.begin(), x.end(),
            [](const auto &a, const auto &b) { return a.second > b.second; });
  csv << std::fixed << std::setprecision(10);
  csv << "strategy,probability\n";
  for (const auto &[s, p] : x) {
    for (size_t i = 0; i < N; i++)
      csv << s.play[i];
    csv << "," << p << "\n";
  }
}

void fictitious_play(mixed_strategy& msa, mixed_strategy& msd, std::filesystem::path path) {
  std::ofstream csv(path);
  csv << std::fixed << std::setprecision(5);
  csv << "iteration,epsilon,payoff,attacker_best_response,defender_best_response\n" /*,duration\n"*/;
  std::cout << std::fixed << std::setprecision(1);

  std::chrono::duration<double> computation_time = std::chrono::duration<double>(0);
  std::chrono::duration<double> total_time = std::chrono::duration<double>(0);
  auto total_start = std::chrono::high_resolution_clock::now();
  size_t i;
  size_t print_step = std::max(iterations / 1000 / 10, 1UL);
  for (i = 0; i < iterations && !interrupted; i++) {
    if (i / print_step != (i-1) / print_step)
      std::cout << "\r" << 100.f * i / iterations << "%" << std::flush;

    auto start = std::chrono::high_resolution_clock::now();

    double current = u(msa, msd);
    auto best_attacker = best_response_attacker(msd);
    auto best_defender = best_response_defender(msa);
    double epsilon = std::max(
        {0., best_attacker.first - current, -(best_defender.first - current)});
    msa.add(best_attacker.second);
    msd.add(best_defender.second);

    auto end = std::chrono::high_resolution_clock::now();

    computation_time += end - start;

    csv << i+1 << ","
        << epsilon << ","
        << current << ","
        << best_attacker.first << ","
        << best_defender.first << "\n";
        // << std::chrono::duration_cast<std::chrono::microseconds>(end-start).count() << "\n";
  }
  std::cout << "\r" << 100.f << "%" << std::flush;

  auto total_end = std::chrono::high_resolution_clock::now();
  total_time = total_end - total_start;

  std::cout << "\r";

  if (interrupted) {
    std::cout << "Interrupted, did " << i << " out of " << iterations << " iterations" << std::endl;
    interrupted = false;
  } else {
    std::cout << "Finished " << iterations << " iterations" << std::endl;
  }

  std::cout << std::setprecision(2) << "ficticious play took "
    << std::chrono::duration_cast<std::chrono::seconds>(total_time).count()
    << " seconds ("
    << std::chrono::duration_cast<std::chrono::seconds>(computation_time).count()
    << " computation, "
    << std::chrono::duration_cast<std::chrono::seconds>(total_time - computation_time).count()
    << " io)" << std::endl;

  csv.close();
}

void read_input(std::istream& is) {
  vals.clear();
  is >> iterations;
  is >> BA >> BD;
  is >> N;
  vals.resize(N);
  for (size_t i = 0; i < N; i++)
    is >> vals[i];
}

int main(int argc, char* argv[]) {
  if (argc <= 1) {
    std::cerr << "usage: " << argv[0] << " <config1> [config2] [config3] ..." << std::endl;
    return 1;
  }

  std::signal(SIGINT, signal_handler);

  for (int i = 1; i < argc; i++) {
    std::filesystem::path path(argv[i]);

    std::ifstream config_file(path);
    read_input(config_file);
    config_file.close();

    std::cout << path << " ... "
      << "BA = " << BA
      << ", BD = " << BD
      << ", battlefield = ";
    for (size_t i = 0; i < N; i++)
      std::cout << +vals[i] << " ";
    std::cout << "\n";

    mixed_strategy msa = uniform_strategy(BA);
    mixed_strategy msd = uniform_strategy(BD);

    path.replace_extension("csv");
    fictitious_play(msa, msd, path);

    path.replace_extension("attacker");
    print_strategy(msa, path);
    path.replace_extension("defender");
    print_strategy(msd, path);
  }

  return 0;
}
