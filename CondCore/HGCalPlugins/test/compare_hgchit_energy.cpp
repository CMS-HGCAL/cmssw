// Validation utility for the JSON and SQLite HGCal calibration backends.
//
// Events are matched by (run, luminosityBlock, event).
// RecHits within each event are matched by HGCHit_denseIndex.
// HGCHit_energy is the only physics quantity compared.
//
// The two NANO files must be produced using the same RAW input,
// calibration content, geometry, and detector mapping.

#include <TFile.h>
#include <TLeaf.h>
#include <TObjArray.h>
#include <TTree.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct EventKey {
  std::uint32_t run;
  std::uint32_t lumi;
  std::uint64_t event;

  bool operator==(const EventKey& other) const noexcept {
    return run == other.run &&
           lumi == other.lumi &&
           event == other.event;
  }
};

struct EventKeyHash {
  std::size_t operator()(const EventKey& key) const noexcept {
    std::size_t seed =
        std::hash<std::uint64_t>{}(
            (static_cast<std::uint64_t>(key.run) << 32) |
            static_cast<std::uint64_t>(key.lumi));

    seed ^= std::hash<std::uint64_t>{}(key.event) +
            0x9e3779b97f4a7c15ULL +
            (seed << 6) +
            (seed >> 2);

    return seed;
  }
};

TLeaf* requireLeaf(TTree* tree, const char* name) {
  auto* leaf = tree->GetLeaf(name);

  if (leaf == nullptr) {
    throw std::runtime_error(
        std::string("Required branch was not found: ") + name);
  }

  return leaf;
}

EventKey readEventKey(TLeaf* runLeaf,
                      TLeaf* lumiLeaf,
                      TLeaf* eventLeaf) {
  return {
      static_cast<std::uint32_t>(runLeaf->GetValueLong64()),
      static_cast<std::uint32_t>(lumiLeaf->GetValueLong64()),
      static_cast<std::uint64_t>(eventLeaf->GetValueLong64())
  };
}

std::set<std::string> collectHGCHitBranches(TTree* tree) {
  std::set<std::string> names;
  auto* branches = tree->GetListOfBranches();

  for (int i = 0; i < branches->GetEntries(); ++i) {
    const std::string name = branches->At(i)->GetName();

    if (name.rfind("HGCHit_", 0) == 0) {
      names.insert(name);
    }
  }

  return names;
}

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr
        << "Usage: " << argv[0]
        << " <JSON_NANO.root> <SQLITE_NANO.root>\n";
    return EXIT_FAILURE;
  }

  try {
    std::unique_ptr<TFile> jsonFile(
        TFile::Open(argv[1], "READ"));

    std::unique_ptr<TFile> sqliteFile(
        TFile::Open(argv[2], "READ"));

    if (!jsonFile || jsonFile->IsZombie()) {
      throw std::runtime_error(
          std::string("Cannot open JSON NANO: ") + argv[1]);
    }

    if (!sqliteFile || sqliteFile->IsZombie()) {
      throw std::runtime_error(
          std::string("Cannot open SQLite NANO: ") + argv[2]);
    }

    auto* jsonTree =
        dynamic_cast<TTree*>(jsonFile->Get("Events"));

    auto* sqliteTree =
        dynamic_cast<TTree*>(sqliteFile->Get("Events"));

    if (jsonTree == nullptr || sqliteTree == nullptr) {
      throw std::runtime_error(
          "The Events tree was not found in one or both files.");
    }

    const auto jsonBranches =
        collectHGCHitBranches(jsonTree);

    const auto sqliteBranches =
        collectHGCHitBranches(sqliteTree);

    std::vector<std::string> commonBranches;

    std::set_intersection(
        jsonBranches.begin(),
        jsonBranches.end(),
        sqliteBranches.begin(),
        sqliteBranches.end(),
        std::back_inserter(commonBranches));

    std::cout << "\nAvailable common HGCHit_* quantities\n";
    std::cout << "------------------------------------\n";

    for (const auto& branch : commonBranches) {
      std::cout << branch << '\n';
    }

    auto* jsonRun =
        requireLeaf(jsonTree, "run");

    auto* jsonLumi =
        requireLeaf(jsonTree, "luminosityBlock");

    auto* jsonEvent =
        requireLeaf(jsonTree, "event");

    auto* jsonNHits =
        requireLeaf(jsonTree, "nHGCHit");

    auto* jsonDenseIndex =
        requireLeaf(jsonTree, "HGCHit_denseIndex");

    auto* jsonEnergy =
        requireLeaf(jsonTree, "HGCHit_energy");

    auto* sqliteRun =
        requireLeaf(sqliteTree, "run");

    auto* sqliteLumi =
        requireLeaf(sqliteTree, "luminosityBlock");

    auto* sqliteEvent =
        requireLeaf(sqliteTree, "event");

    auto* sqliteNHits =
        requireLeaf(sqliteTree, "nHGCHit");

    auto* sqliteDenseIndex =
        requireLeaf(sqliteTree, "HGCHit_denseIndex");

    auto* sqliteEnergy =
        requireLeaf(sqliteTree, "HGCHit_energy");

    const Long64_t jsonEntries =
        jsonTree->GetEntries();

    const Long64_t sqliteEntries =
        sqliteTree->GetEntries();

    std::unordered_map<EventKey, Long64_t, EventKeyHash>
        sqliteEventIndex;

    sqliteEventIndex.reserve(
        static_cast<std::size_t>(sqliteEntries * 1.3));

    std::uint64_t duplicateSQLiteEventKeys = 0;
    std::uint64_t totalSQLiteRecHits = 0;

    for (Long64_t entry = 0;
         entry < sqliteEntries;
         ++entry) {
      sqliteTree->GetEntry(entry);

      const EventKey key =
          readEventKey(
              sqliteRun,
              sqliteLumi,
              sqliteEvent);

      if (!sqliteEventIndex.emplace(key, entry).second) {
        ++duplicateSQLiteEventKeys;
      }

      totalSQLiteRecHits +=
          static_cast<std::uint64_t>(
              sqliteNHits->GetValueLong64());
    }

    std::unordered_set<EventKey, EventKeyHash>
        seenJSONEvents;

    seenJSONEvents.reserve(
        static_cast<std::size_t>(jsonEntries * 1.3));

    std::uint64_t duplicateJSONEventKeys = 0;
    std::uint64_t matchedEvents = 0;
    std::uint64_t eventsMissingInSQLite = 0;
    std::uint64_t eventsWithDifferentHitCounts = 0;

    std::uint64_t totalJSONRecHits = 0;
    std::uint64_t energyValuesCompared = 0;
    std::uint64_t energyMismatches = 0;

    std::uint64_t duplicateJSONDenseIndex = 0;
    std::uint64_t duplicateSQLiteDenseIndex = 0;
    std::uint64_t cellsMissingInSQLite = 0;
    std::uint64_t cellsMissingInJSON = 0;

    double maximumAbsoluteDifference = 0.0;

    for (Long64_t jsonEntry = 0;
         jsonEntry < jsonEntries;
         ++jsonEntry) {
      jsonTree->GetEntry(jsonEntry);

      const EventKey eventKey =
          readEventKey(
              jsonRun,
              jsonLumi,
              jsonEvent);

      if (!seenJSONEvents.insert(eventKey).second) {
        ++duplicateJSONEventKeys;
        continue;
      }

      const int jsonHitCount =
          static_cast<int>(
              jsonNHits->GetValueLong64());

      totalJSONRecHits +=
          static_cast<std::uint64_t>(jsonHitCount);

      const auto sqliteEventPosition =
          sqliteEventIndex.find(eventKey);

      if (sqliteEventPosition ==
          sqliteEventIndex.end()) {
        ++eventsMissingInSQLite;
        continue;
      }

      ++matchedEvents;

      sqliteTree->GetEntry(
          sqliteEventPosition->second);

      const int sqliteHitCount =
          static_cast<int>(
              sqliteNHits->GetValueLong64());

      if (jsonHitCount != sqliteHitCount) {
        ++eventsWithDifferentHitCounts;
      }

      std::unordered_map<std::int64_t, double>
          sqliteEnergyByDenseIndex;

      sqliteEnergyByDenseIndex.reserve(
          static_cast<std::size_t>(
              sqliteHitCount * 1.3 + 1));

      for (int hit = 0;
           hit < sqliteHitCount;
           ++hit) {
        const std::int64_t denseIndex =
            sqliteDenseIndex->GetValueLong64(hit);

        const double energy =
            sqliteEnergy->GetValue(hit);

        if (!sqliteEnergyByDenseIndex
                 .emplace(denseIndex, energy)
                 .second) {
          ++duplicateSQLiteDenseIndex;
        }
      }

      std::unordered_set<std::int64_t>
          seenJSONDenseIndices;

      seenJSONDenseIndices.reserve(
          static_cast<std::size_t>(
              jsonHitCount * 1.3 + 1));

      std::uint64_t matchedCellsInEvent = 0;

      for (int hit = 0;
           hit < jsonHitCount;
           ++hit) {
        const std::int64_t denseIndex =
            jsonDenseIndex->GetValueLong64(hit);

        if (!seenJSONDenseIndices
                 .insert(denseIndex)
                 .second) {
          ++duplicateJSONDenseIndex;
          continue;
        }

        const auto sqliteHit =
            sqliteEnergyByDenseIndex.find(
                denseIndex);

        if (sqliteHit ==
            sqliteEnergyByDenseIndex.end()) {
          ++cellsMissingInSQLite;
          continue;
        }

        ++matchedCellsInEvent;
        ++energyValuesCompared;

        const double jsonValue =
            jsonEnergy->GetValue(hit);

        const double sqliteValue =
            sqliteHit->second;

        const bool bothNaN =
            std::isnan(jsonValue) &&
            std::isnan(sqliteValue);

        const bool exactlyEqual =
            bothNaN ||
            jsonValue == sqliteValue;

        if (!exactlyEqual) {
          ++energyMismatches;

          double difference =
              std::abs(
                  jsonValue -
                  sqliteValue);

          if (!std::isfinite(difference)) {
            difference =
                std::numeric_limits<double>::infinity();
          }

          maximumAbsoluteDifference =
              std::max(
                  maximumAbsoluteDifference,
                  difference);
        }
      }

      if (sqliteEnergyByDenseIndex.size() >
          matchedCellsInEvent) {
        cellsMissingInJSON +=
            sqliteEnergyByDenseIndex.size() -
            matchedCellsInEvent;
      }
    }

    const std::uint64_t eventsMissingInJSON =
        sqliteEventIndex.size() >= matchedEvents
            ? sqliteEventIndex.size() -
              matchedEvents
            : 0;

    std::cout << "\nFocused HGCHit_energy validation\n";
    std::cout << "------------------------------------\n";

    std::cout
        << "Event matching key             : "
        << "(run, luminosityBlock, event)\n";

    std::cout
        << "RecHit matching key            : "
        << "HGCHit_denseIndex\n";

    std::cout
        << "Primary comparison quantity    : "
        << "HGCHit_energy\n\n";

    std::cout
        << "JSON events                    : "
        << jsonEntries << '\n';

    std::cout
        << "SQLite events                  : "
        << sqliteEntries << '\n';

    std::cout
        << "Matched events                 : "
        << matchedEvents << '\n';

    std::cout
        << "Duplicate JSON event keys      : "
        << duplicateJSONEventKeys << '\n';

    std::cout
        << "Duplicate SQLite event keys    : "
        << duplicateSQLiteEventKeys << '\n';

    std::cout
        << "Events missing in SQLite       : "
        << eventsMissingInSQLite << '\n';

    std::cout
        << "Events missing in JSON         : "
        << eventsMissingInJSON << '\n';

    std::cout
        << "Different nHGCHit counts       : "
        << eventsWithDifferentHitCounts << '\n';

    std::cout
        << "\nJSON RecHits                    : "
        << totalJSONRecHits << '\n';

    std::cout
        << "SQLite RecHits                  : "
        << totalSQLiteRecHits << '\n';

    std::cout
        << "Duplicate JSON denseIndex      : "
        << duplicateJSONDenseIndex << '\n';

    std::cout
        << "Duplicate SQLite denseIndex    : "
        << duplicateSQLiteDenseIndex << '\n';

    std::cout
        << "Cells missing in SQLite        : "
        << cellsMissingInSQLite << '\n';

    std::cout
        << "Cells missing in JSON          : "
        << cellsMissingInJSON << '\n';

    std::cout
        << "\nEnergy values compared         : "
        << energyValuesCompared << '\n';

    std::cout
        << "Energy mismatches              : "
        << energyMismatches << '\n';

    std::cout
        << "Maximum absolute difference    : "
        << std::setprecision(17)
        << maximumAbsoluteDifference
        << '\n';

    const bool passed =
        duplicateJSONEventKeys == 0 &&
        duplicateSQLiteEventKeys == 0 &&
        eventsMissingInSQLite == 0 &&
        eventsMissingInJSON == 0 &&
        eventsWithDifferentHitCounts == 0 &&
        duplicateJSONDenseIndex == 0 &&
        duplicateSQLiteDenseIndex == 0 &&
        cellsMissingInSQLite == 0 &&
        cellsMissingInJSON == 0 &&
        energyMismatches == 0;

    if (passed) {
      std::cout
          << "\nRESULT: PASS — HGCHit_energy is exactly "
          << "identical after matching events by "
          << "(run, luminosityBlock, event) and "
          << "RecHits by HGCHit_denseIndex.\n";

      return EXIT_SUCCESS;
    }

    std::cout << "\nRESULT: FAIL\n";
    return EXIT_FAILURE;

  } catch (const std::exception& error) {
    std::cerr << "ERROR: "
              << error.what()
              << '\n';

    return EXIT_FAILURE;
  }
}
