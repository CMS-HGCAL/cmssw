import FWCore.ParameterSet.Config as cms

from Configuration.Eras.Era_Phase2C17I13M9_cff import Phase2C17I13M9
process = cms.Process('LOCALDIGI2RAW', Phase2C17I13M9)

process.load('Configuration.StandardSequences.Services_cff')
process.load('FWCore.MessageService.MessageLogger_cfi')
process.load('Configuration.Geometry.GeometryExtendedRun4D104Reco_cff')
process.load('Configuration.Geometry.GeometryExtendedRun4D104_cff')
process.load('Configuration.StandardSequences.EndOfProcess_cff')
process.load('Configuration.StandardSequences.FrontierConditions_GlobalTag_cff')

# parse command line arguments
from FWCore.ParameterSet.VarParsing import VarParsing
options = VarParsing('standard')
options.register('modules', '/eos/cms/store/group/dpg_hgcal/comm_hgcal/psilva/Hackathon_2025Jan/modulelocator_Si.txt', VarParsing.multiplicity.singleton, VarParsing.varType.string,
                 "module locator file to use")
options.register('filter', 'fedid==0', VarParsing.multiplicity.singleton, VarParsing.varType.string,
                 "filter condition to apply (pandas/sql syntax)")
options.parseArguments()

# input configuration
process.maxEvents = cms.untracked.PSet( input = cms.untracked.int32(options.maxEvents) )
process.source = cms.Source(
    "PoolSource",
    fileNames=cms.untracked.vstring(*options.files)
)

# mapping
from Geometry.HGCalMapping.hgcalmapping_cff import customise_hgcalmapper
import pandas as pd
df = pd.read_csv(options.modules, sep='\s+')
filt_df = df.query(options.filter)
import os
cmssw_base = os.environ['CMSSW_BASE']
outmoduleloc = f'{cmssw_base}/src/modulelocator.txt'
filt_df.to_csv(outmoduleloc, sep=' ', header=True, index=False)
process = customise_hgcalmapper(process, modules = 'modulelocator.txt')
del df
print(f'Module locator @ {outmoduleloc}')

process.digi2soa = cms.EDProducer('HGCalDigiSoAFiller')

# timing
process.Timing = cms.Service(
    "Timing",
    summaryOnly=cms.untracked.bool(True),
    useJobReport=cms.untracked.bool(True)
)
                                
process.t = cms.Task( process.digi2soa )
process.p = cms.Path( process.t ) 

#output
process.output = cms.OutputModule(
  "PoolOutputModule",
  fileName=cms.untracked.string(options.output),
  outputCommands=cms.untracked.vstring(
    'drop *',
    'keep *_digi2soa_*_*' #SoA*_hgcalDigis_*_*',
  ),
  SelectEvents=cms.untracked.PSet(SelectEvents=cms.vstring('p'))
)
process.outpath = cms.EndPath(process.output)


# Add early deletion of temporary data products to reduce peak memory need
from Configuration.StandardSequences.earlyDeleteSettings_cff import customiseEarlyDelete
process = customiseEarlyDelete(process)
