import FWCore.ParameterSet.Config as cms
import FWCore.ParameterSet.VarParsing as VarParsing

options = VarParsing.VarParsing('standard')

options.register('jsonFile',
                 'pedestal_MH_B1W_DNT0177_115457.json',
                 VarParsing.VarParsing.multiplicity.singleton,
                 VarParsing.VarParsing.varType.string,
                 "Input JSON file")

options.register('record',
                 'HGCalPedestalRcd',
                 VarParsing.VarParsing.multiplicity.singleton,
                 VarParsing.VarParsing.varType.string,
                 "CondDB record name")

options.register('tag',
                 'HGCalPedestals_test_2026_06_22',
                 VarParsing.VarParsing.multiplicity.singleton,
                 VarParsing.VarParsing.varType.string,
                 "CondDB tag name")

options.register('sinceRun',
                 1,
                 VarParsing.VarParsing.multiplicity.singleton,
                 VarParsing.VarParsing.varType.int,
                 "IOV start run number")

options.register('writeToCondDB',
                 False,
                 VarParsing.VarParsing.multiplicity.singleton,
                 VarParsing.VarParsing.varType.bool,
                 "If True, write to SQLite")

options.parseArguments()

process = cms.Process("HGCalPedestalDB")

process.MessageLogger = cms.Service(
    "MessageLogger",
    destinations = cms.untracked.vstring('cout'),
    cout = cms.untracked.PSet(
        threshold = cms.untracked.string('INFO')
    )
)

process.source = cms.Source(
    "EmptyIOVSource",
    timetype   = cms.string('runnumber'),
    firstValue = cms.uint64(1),
    lastValue  = cms.uint64(1),
    interval   = cms.uint64(1)
)

process.load("CondCore.CondDB.CondDB_cfi")
process.CondDB.connect = cms.string('sqlite_file:hgcal_pedestals_test.db')

process.PoolDBOutputService = cms.Service(
    "PoolDBOutputService",
    process.CondDB,
    timetype   = cms.untracked.string('runnumber'),
    toPut      = cms.VPSet(
        cms.PSet(
            record = cms.string(str(options.record)),
            tag    = cms.string(str(options.tag))
        )
    ),
    logconnect = cms.untracked.string('sqlite_file:hgcal_pedestals_log.db')
)

process.hgcalPedestalAnalyzer = cms.EDAnalyzer(
    "HGCalPedestalDBAnalyzer",
    jsonFile      = cms.string(str(options.jsonFile)),
    record        = cms.string(str(options.record)),
    tag           = cms.string(str(options.tag)),
    sinceRun      = cms.uint64(options.sinceRun),
    writeToCondDB = cms.bool(bool(options.writeToCondDB)),
    verbose       = cms.untracked.bool(True)
)

process.p = cms.Path(process.hgcalPedestalAnalyzer)
