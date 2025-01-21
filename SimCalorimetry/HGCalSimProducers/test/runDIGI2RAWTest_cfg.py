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
options.register('modules', '/eos/cms/store/group/dpg_hgcal/comm_hgcal/psilva/Hackathon_2025Jan/modulelocator_layer1.txt', VarParsing.multiplicity.singleton, VarParsing.varType.string,
                 "module locator file to use")
options.parseArguments()

# input configuration
process.maxEvents = cms.untracked.PSet( input = cms.untracked.int32(options.maxEvents) )
process.source = cms.Source(
    "PoolSource",
    fileNames=cms.untracked.vstring(*options.files)
)

# mapping (!dirty trick ahead! use symbolic link always with the same name)
from Geometry.HGCalMapping.hgcalmapping_cff import customise_hgcalmapper
import os
os.system(f'cp -v {options.modules} ${{CMSSW_BASE}}/src/modulelocator.txt')
process = customise_hgcalmapper(process, modules = 'modulelocator.txt')

#converter
process.load('SimCalorimetry.HGCalSimProducers.hgCalDigiSoAFiller_cfi')

# timing
process.Timing = cms.Service(
    "Timing",
    summaryOnly=cms.untracked.bool(True),
    useJobReport=cms.untracked.bool(True)
)
                                
process.t = cms.Task( process.hgCalDigiSoAFiller )
process.p = cms.Path( process.t ) 
