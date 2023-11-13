import FWCore.ParameterSet.Config as cms
process = cms.Process("TEST")

process.MessageLogger = cms.Service("MessageLogger",
    debugModules = cms.untracked.vstring("HGCalMappingIndexESSource*"),
    cerr = cms.untracked.PSet(
        enable = cms.untracked.bool(True),
        threshold = cms.untracked.string('DEBUG')
    ),
    cout = cms.untracked.PSet(
        enable = cms.untracked.bool(True),
        threshold = cms.untracked.string('DEBUG')
    )
)

process.source = cms.Source('EmptySource')

process.maxEvents = cms.untracked.PSet(
    input = cms.untracked.int32(1)
)

process.load('Geometry.HGCalMapping.hgCalMappingIndexESSource_cfi')
process.analyzer = cms.EDAnalyzer("HGCalMappingIndexESSourceTester")

process.p = cms.Path(process.analyzer)
