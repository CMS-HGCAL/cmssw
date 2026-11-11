import FWCore.ParameterSet.Config as cms
from FWCore.ParameterSet.VarParsing import VarParsing
from CondCore.CondDB.CondDB_cfi import CondDB

options = VarParsing("analysis")

options.register(
    "jsonFile",
    "-",
    VarParsing.multiplicity.singleton,
    VarParsing.varType.string,
    "Calibration JSON input; use '-' for stdin",
)

options.register(
    "outputDB",
    "sqlite_file:hgcal_calibration.db",
    VarParsing.multiplicity.singleton,
    VarParsing.varType.string,
    "Output SQLite connection string",
)


options.register(
    "outputTag",
    "HGCalCalibration_v1",
    VarParsing.multiplicity.singleton,
    VarParsing.varType.string,
    "Prefix for the five pedestal Conditions tags",
)

options.parseArguments()

process = cms.Process("WRITECALIB")

process.load("FWCore.MessageLogger.MessageLogger_cfi")

condition_names = (
    "ADCPedestals",
    "Noise",
    "CommonModePedestals",
    "CommonModeSlope",
    "CalibrationValidity",
)

process.PoolDBOutputService = cms.Service(
    "PoolDBOutputService",
    CondDB.clone(connect=cms.string(options.outputDB)),
    timetype=cms.untracked.string("runnumber"),
    toPut=cms.VPSet(*[
        cms.PSet(
            record=cms.string(f"HGCal{name}Rcd"),
            tag=cms.string(f"{options.outputTag}_{name}"),
        )
        for name in condition_names
    ]),
    loadBlobStreamer=cms.untracked.bool(False),
)

process.source = cms.Source(
    "EmptySource",
    firstRun=cms.untracked.uint32(1),
)

process.maxEvents = cms.untracked.PSet(
    input=cms.untracked.int32(1)
)

process.hgcalCalibrationDBWriter = cms.EDAnalyzer(
    "HGCalCalibrationDBWriter",
    jsonFile=cms.string(options.jsonFile),
)

process.writeCalibration = cms.Path(
    process.hgcalCalibrationDBWriter
)
