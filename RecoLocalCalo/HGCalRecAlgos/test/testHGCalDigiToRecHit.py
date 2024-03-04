import FWCore.ParameterSet.Config as cms
import FWCore.ParameterSet.VarParsing as VarParsing

process = cms.Process("testRecHit")

process.source = cms.Source("PoolSource",
    fileNames = cms.untracked.vstring('file:/eos/user/y/yumiao/public/HGCALRawDataHandling/Digis/testFakeDigis.root')
)

process.maxEvents = cms.untracked.PSet( input = cms.untracked.int32(1) )


process.output = cms.OutputModule("PoolOutputModule",
    fileName = cms.untracked.string("./testRecHit.root"),
)

process.outpath = cms.EndPath(process.output)

process.hgcalRecHits = cms.EDProducer('HGCalDigiToRecHit',
  Digis = cms.InputTag('hgcalDigis', 'DIGI', 'TEST'),
)

process.p = cms.Path(process.hgcalRecHits)

