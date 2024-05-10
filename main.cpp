#include <algorithm>
#include <bit>
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

int iterations;
int N, BA, BD;
std::vector<uint32_t> vals;

using strategy = uint16_t;

struct mixed_strategy {
  std::unordered_map<strategy, uint64_t> plays;
  uint64_t size;
  void add(strategy s) {
    plays[s]++;
    size++;
  }
};

double u(strategy sa, strategy sd) {
  uint16_t won = sa & (~sd);
  double res = 0;
  for (int i = 0; i < N; i++) {
    if (won & 1)
      res += vals[i];
    won >>= 1;
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

std::pair<double, strategy> best_response_attacker(mixed_strategy msd) {
  std::pair<double, strategy> res{-1.f, 0};
  for (strategy sa = 0; sa < (1 << N); sa++)
    if (std::popcount(sa) == BA)
      res = std::max(res, std::make_pair(u(sa, msd), sa));
  return res;
}

std::pair<double, strategy> best_response_defender(mixed_strategy msa) {
  std::pair<double, strategy> res{1000000.f, 0};
  for (strategy sb = 0; sb < (1 << N); sb++)
    if (std::popcount(sb) == BD)
      res = std::min(res, std::make_pair(u(msa, sb), sb));
  return res;
}

double what_approx(mixed_strategy msa, mixed_strategy msd) {
  double current = u(msa, msd);
  double best_attacker = best_response_attacker(msd).first;
  double best_defender = best_response_defender(msa).first;
  return std::max({0., best_attacker - current, -(best_defender - current)});
}

mixed_strategy uniform_strategy(int resources) {
  mixed_strategy ms;
  for (strategy s = 0; s < (1 << N); s++)
    if (std::popcount(s) == resources)
      ms.plays[s] = 1;
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
    for (int i = 0; i < N; i++)
      csv << ((s >> i) & 1);
    csv << "," << p << "\n";
  }
}

void fictitious_play(mixed_strategy& msa, mixed_strategy& msd, std::filesystem::path path) {
  std::ofstream csv(path);
  csv << std::fixed << std::setprecision(5);
  csv << "iteration,epsilon,payoff,attacker_best_response,defender_best_response,duration\n";
  std::cout << std::fixed << std::setprecision(1);

  std::chrono::duration<double> computation_time = std::chrono::duration<double>(0);
  std::chrono::duration<double> total_time = std::chrono::duration<double>(0);
  auto total_start = std::chrono::high_resolution_clock::now();
  int i;
  int print_step = iterations / 1000 / 10;
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
        << best_defender.first << ","
        << std::chrono::duration_cast<std::chrono::microseconds>(end-start).count() << "\n";
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
  for (int i = 0; i < N; i++)
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
    for (int i = 0; i < N; i++)
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
