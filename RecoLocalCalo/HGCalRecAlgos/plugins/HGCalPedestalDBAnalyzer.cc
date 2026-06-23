// HGCalPedestalDBAnalyzer.cc  — v3
//
// EDAnalyzer: Complete workflow
//   JSON → HGCalPedestalConditions → SQLite CondDB (HGCalPedestalRcd)
//
// Usage:
//   # Step 1: fetch from MySQL
//   python3 fetchPedestals.py MH_B1W_DNT0177 115457
//
//   # Step 2: dry-run
//   cmsRun HGCalPedestalCondDB_cfg.py
//
//   # Step 3: write to SQLite
//   cmsRun HGCalPedestalCondDB_cfg.py writeToCondDB=True
//
//   # Step 4: verify
//   conddb --db sqlite_file:hgcal_pedestals_test.db listTags
//   conddb --db sqlite_file:hgcal_pedestals_test.db list HGCalPedestals_test_2026_06_22
//
// Author: Fatma Nur Yuce (EP-UCM, RDH), CERN 2026

// ── CMSSW framework ───────────────────────────────────────────────────────────
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"

// ── CondDB output ─────────────────────────────────────────────────────────────
#include "CondCore/DBOutputService/interface/PoolDBOutputService.h"

// ── HGCal CondFormats (our new class) ────────────────────────────────────────
#include "CondFormats/HGCalObjects/interface/HGCalPedestalConditions.h"

// ── JSON parsing ──────────────────────────────────────────────────────────────
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// ── Standard library ──────────────────────────────────────────────────────────
#include <string>
#include <vector>
#include <numeric>
#include <fstream>
#include <stdexcept>
#include <algorithm>

// =============================================================================
// EDAnalyzer
// =============================================================================
class HGCalPedestalDBAnalyzer : public edm::one::EDAnalyzer<> {
public:
  explicit HGCalPedestalDBAnalyzer(const edm::ParameterSet& ps);
  ~HGCalPedestalDBAnalyzer() override = default;
  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

private:
  void beginJob() override;
  void analyze(const edm::Event&, const edm::EventSetup&) override {}
  void endJob() override;

  // Read JSON → fill HGCalPedestalConditions::ModuleData
  HGCalPedestalConditions::ModuleData readFromJSON(const std::string& filename);

  // Print summary statistics
  void printSummary(const HGCalPedestalConditions::ModuleData& m) const;

  // Write HGCalPedestalConditions to SQLite via PoolDBOutputService
  void writeToCondDB(const HGCalPedestalConditions& payload);

  // ── Config parameters ─────────────────────────────────────────────────────
  std::string        m_jsonFile;
  std::string        m_record;        // "HGCalPedestalRcd"
  std::string        m_tag;           // e.g. "HGCalPedestals_test_2026_06_22"
  unsigned long long m_sinceRun;
  bool               m_writeToCondDB;
  bool               m_verbose;
};

// =============================================================================
// Constructor
// =============================================================================
HGCalPedestalDBAnalyzer::HGCalPedestalDBAnalyzer(const edm::ParameterSet& ps)
    : m_jsonFile     (ps.getParameter<std::string>      ("jsonFile"))
    , m_record       (ps.getParameter<std::string>      ("record"))
    , m_tag          (ps.getParameter<std::string>      ("tag"))
    , m_sinceRun     (ps.getParameter<unsigned long long>("sinceRun"))
    , m_writeToCondDB(ps.getParameter<bool>             ("writeToCondDB"))
    , m_verbose      (ps.getUntrackedParameter<bool>    ("verbose", true))
{}

// =============================================================================
// fillDescriptions
// =============================================================================
void HGCalPedestalDBAnalyzer::fillDescriptions(
    edm::ConfigurationDescriptions& descriptions)
{
  edm::ParameterSetDescription desc;
  desc.add<std::string>       ("jsonFile",      "pedestal_MH_B1W_DNT0177_115457.json");
  desc.add<std::string>       ("record",        "HGCalPedestalRcd");
  desc.add<std::string>       ("tag",           "HGCalPedestals_test_2026_06_22");
  desc.add<unsigned long long>("sinceRun",       1ULL);
  desc.add<bool>              ("writeToCondDB",  false);
  desc.addUntracked<bool>     ("verbose",        true);
  descriptions.addWithDefaultLabel(desc);
}

// =============================================================================
// beginJob
// =============================================================================
void HGCalPedestalDBAnalyzer::beginJob() {
  edm::LogInfo("HGCalPedestalDBAnalyzer")
      << "\n=========================================="
      << "\n  HGCal Pedestal DB Analyzer v3"
      << "\n  jsonFile     : " << m_jsonFile
      << "\n  record       : " << m_record
      << "\n  tag          : " << m_tag
      << "\n  sinceRun     : " << m_sinceRun
      << "\n  writeToCondDB: " << (m_writeToCondDB ? "YES" : "NO (dry-run)")
      << "\n==========================================";
}

// =============================================================================
// readFromJSON
// Reads fetchPedestals.py output and fills HGCalPedestalConditions::ModuleData
// =============================================================================
HGCalPedestalConditions::ModuleData
HGCalPedestalDBAnalyzer::readFromJSON(const std::string& filename)
{
  std::ifstream infile(filename);
  if (!infile.is_open())
    throw std::runtime_error("Cannot open JSON file: " + filename);

  json j = json::parse(infile);

  HGCalPedestalConditions::ModuleData m;
  m.typeCode  = j.at("typeCode") .get<std::string>();
  m.reference = j.at("reference").get<std::string>();
  m.timestamp = j.at("timestamp").get<std::string>();

  auto& p = j.at("payload");

  // Read all fields — names match calibrations_test JSON keys exactly
  auto vi = p.at("Valid")    .get<std::vector<int>>();
  auto ci = p.at("Channel")  .get<std::vector<int>>();
  m.valid    .assign(vi.begin(), vi.end());
  m.channel  .assign(ci.begin(), ci.end());
  m.adc_ped   = p.at("adc_ped")  .get<std::vector<float>>();
  m.adc_rms   = p.at("adc_rms")  .get<std::vector<float>>();
  m.cm2_ped   = p.at("cm2_ped")  .get<std::vector<float>>();
  m.cm2_rms   = p.at("cm2_rms")  .get<std::vector<float>>();
  m.cm4_ped   = p.at("cm4_ped")  .get<std::vector<float>>();
  m.cm4_rms   = p.at("cm4_rms")  .get<std::vector<float>>();
  m.cm2_slope = p.at("cm2_slope").get<std::vector<float>>();
  m.cm4_slope = p.at("cm4_slope").get<std::vector<float>>();

  edm::LogInfo("HGCalPedestalDBAnalyzer")
      << "Loaded " << m.nChannels() << " channels"
      << " for module " << m.typeCode
      << " ref=" << m.reference
      << " @ " << m.timestamp;

  return m;
}

// =============================================================================
// printSummary
// =============================================================================
void HGCalPedestalDBAnalyzer::printSummary(
    const HGCalPedestalConditions::ModuleData& m) const
{
  const size_t N = m.adc_ped.size();

  edm::LogInfo("HGCalPedestalDBAnalyzer")
      << "\n=========================================="
      << "\n  Pedestal Summary"
      << "\n  typeCode  : " << m.typeCode
      << "\n  reference : " << m.reference
      << "\n  timestamp : " << m.timestamp
      << "\n  N channels: " << N
      << "\n==========================================";

  if (m_verbose) {
    for (size_t i = 0; i < N; ++i) {
      edm::LogInfo("HGCalPedestalDBAnalyzer")
          << "  ch[" << m.channel[i] << "]"
          << "  valid="     << m.valid[i]
          << "  adc_ped="   << m.adc_ped[i]
          << "  adc_rms="   << m.adc_rms[i]
          << "  cm2_ped="   << m.cm2_ped[i]
          << "  cm2_slope=" << m.cm2_slope[i];
    }
  }

  int   nValid       = std::accumulate(m.valid.begin(),    m.valid.end(),    0);
  float mean_adc_ped = std::accumulate(m.adc_ped.begin(), m.adc_ped.end(),  0.f) / N;
  float mean_adc_rms = std::accumulate(m.adc_rms.begin(), m.adc_rms.end(),  0.f) / N;
  float min_ped      = *std::min_element(m.adc_ped.begin(), m.adc_ped.end());
  float max_ped      = *std::max_element(m.adc_ped.begin(), m.adc_ped.end());

  edm::LogInfo("HGCalPedestalDBAnalyzer")
      << "\n--- Statistics ---"
      << "\n  Valid channels : " << nValid << " / " << N
      << "\n  Mean adc_ped   : " << mean_adc_ped
      << "\n  Min  adc_ped   : " << min_ped
      << "\n  Max  adc_ped   : " << max_ped
      << "\n  Mean adc_rms   : " << mean_adc_rms << "  (= noise)";
}

// =============================================================================
// writeToCondDB
//
// Writes HGCalPedestalConditions payload to SQLite via PoolDBOutputService.
// Pattern follows EcalIntercalibHandler::getNewObjects() / PopCon approach.
//
// After this runs verify with:
//   conddb --db sqlite_file:hgcal_pedestals_test.db listTags
//   conddb --db sqlite_file:hgcal_pedestals_test.db list HGCalPedestals_test_2026_06_22
// =============================================================================
void HGCalPedestalDBAnalyzer::writeToCondDB(
    const HGCalPedestalConditions& payload)
{
  edm::Service<cond::service::PoolDBOutputService> poolDbService;
  if (!poolDbService.isAvailable())
    throw std::runtime_error(
        "PoolDBOutputService not available! "
        "Check config: is PoolDBOutputService defined?");

  // writeOneIOV:
  //   arg1: payload object (HGCalPedestalConditions)
  //   arg2: since run     (IOV start)
  //   arg3: record name   ("HGCalPedestalRcd")
  poolDbService->writeOneIOV(payload, m_sinceRun, m_record);

  edm::LogInfo("HGCalPedestalDBAnalyzer")
      << "CondDB write OK:"
      << "\n  record   : " << m_record
      << "\n  tag      : " << m_tag
      << "\n  sinceRun : " << m_sinceRun
      << "\n  modules  : " << payload.nModules()
      << "\n"
      << "\n  Verify with:"
      << "\n  conddb --db sqlite_file:hgcal_pedestals_test.db listTags"
      << "\n  conddb --db sqlite_file:hgcal_pedestals_test.db list " << m_tag;
}

// =============================================================================
// endJob — full workflow
// =============================================================================
void HGCalPedestalDBAnalyzer::endJob() {
  try {

    // Step 1: Read JSON → ModuleData
    edm::LogInfo("HGCalPedestalDBAnalyzer") << "Step 1: Reading JSON...";
    auto moduleData = readFromJSON(m_jsonFile);

    // Step 2: Print summary
    edm::LogInfo("HGCalPedestalDBAnalyzer") << "Step 2: Summary...";
    printSummary(moduleData);

    // Step 3: Pack into HGCalPedestalConditions payload
    HGCalPedestalConditions payload;
    payload.modules.push_back(moduleData);

    edm::LogInfo("HGCalPedestalDBAnalyzer")
        << "Payload ready: " << payload.nModules() << " module(s), "
        << payload.modules[0].nChannels() << " channels";

    // Step 4: Write to SQLite CondDB
    if (m_writeToCondDB) {
      edm::LogInfo("HGCalPedestalDBAnalyzer") << "Step 4: Writing to CondDB...";
      writeToCondDB(payload);
    } else {
      edm::LogInfo("HGCalPedestalDBAnalyzer")
          << "Step 4: SKIPPED (writeToCondDB=False)"
          << "\n  To write: cmsRun HGCalPedestalCondDB_cfg.py writeToCondDB=True";
    }

    edm::LogInfo("HGCalPedestalDBAnalyzer") << "Done.";

  } catch (const std::exception& e) {
    edm::LogError("HGCalPedestalDBAnalyzer") << "ERROR: " << e.what();
    throw;
  }
}

DEFINE_FWK_MODULE(HGCalPedestalDBAnalyzer);
