import FWCore.ParameterSet.Config as cms

process = cms.Process('DIGI')

# import of standard configurations
process.load('Configuration.StandardSequences.Services_cff')
process.load('FWCore.MessageService.MessageLogger_cfi')
process.load('Configuration.Geometry.GeometryExtendedRun4D104_cff')
process.load('Configuration.Geometry.GeometryExtendedRun4D104_cff')
process.load('Configuration.StandardSequences.MagneticField_38T_cff')
process.load('Configuration.StandardSequences.EndOfProcess_cff')

# global tag
process.load('Configuration.StandardSequences.FrontierConditions_GlobalTag_cff')
from Configuration.AlCa.GlobalTag import GlobalTag
process.GlobalTag = GlobalTag(process.GlobalTag, 'auto:upgradePLS3', '')

# configure mixing module
process.load('SimGeneral.MixingModule.mixNoPU_cfi')
from SimCalorimetry.HGCalSimProducers.hgcalDigitizer_cfi import *
process.mix.theDigitizers = cms.PSet( hgcee      = cms.PSet( hgceeDigitizer),
                                      hgchefront = cms.PSet( hgchefrontDigitizer),
                                      hgcheback  = cms.PSet( hgchebackDigitizer)
                                      )
print(process.mix.theDigitizers)

# input configuration
process.maxEvents = cms.untracked.PSet( input = cms.untracked.int32(-1) )
process.source = cms.Source("PoolSource",
                            fileNames=cms.untracked.vstring('/store/group/dpg_hgcal/comm_hgcal/Production/SinglePhotonGun_eta2p0_CMSSW_13_1_0_pre4_D99_vanilla_A10_C20/Events_36.root')
                            )



# path and EndPath definitions
process.simulation_step = cms.Path(process.mix)

# Schedule definition
process.schedule = cms.Schedule(process.simulation_step)
