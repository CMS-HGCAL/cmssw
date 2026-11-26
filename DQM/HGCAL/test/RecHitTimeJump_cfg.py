"""Run the RecHit time harvester on a saved Run 1 histogram file."""
import os
import ROOT
import FWCore.ParameterSet.Config as cms
from FWCore.ParameterSet.VarParsing import VarParsing
from DQMServices.Core.DQMEDHarvester import DQMEDHarvester

options = VarParsing("analysis")
options.register("inputFile", "DQM_V0001_HGCAL_R000000001.root",
                 VarParsing.multiplicity.singleton, VarParsing.varType.string,
                 "Saved Run 1 DQM histogram file (local path)")
options.parseArguments()
input_file = os.path.abspath(options.inputFile.removeprefix("file:"))

process = cms.Process("HARVESTING")
process.load("CondTools.RunInfo.lhcPhaseShiftESSource_cfi")
process.load("FWCore.MessageService.MessageLogger_cfi")
process.DQMStore = cms.Service("DQMStore")
process.source = cms.Source("EmptySource", firstRun=cms.untracked.uint32(1))
process.maxEvents = cms.untracked.PSet(input=cms.untracked.int32(1))
process.options = cms.untracked.PSet(
    numberOfThreads=cms.untracked.uint32(1),
    numberOfStreams=cms.untracked.uint32(1),
)
process.fit = DQMEDHarvester(
    "HGCalRecHitTimeHarvester", inputFile=cms.string(input_file)
)
process.fitPath = cms.Path(process.fit)
process.load("DQMServices.Components.DQMFileSaver_cfi")
process.dqmSaver.workflow = "/HGCal/RecHitTime/Harvesting"
process.dqmSaver.saveByRun = -1
process.dqmSaver.saveAtJobEnd = True
process.dqmSaver.forceRunNumber = 1
process.savePath = cms.EndPath(process.dqmSaver)
process.schedule = cms.Schedule(process.fitPath, process.savePath)

