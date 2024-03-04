import FWCore.ParameterSet.Config as cms
process = cms.Process("Calib")

process.MessageLogger = cms.Service("MessageLogger",
    cerr = cms.untracked.PSet(
        enable = cms.untracked.bool(False)
    ),
    cout = cms.untracked.PSet(
        enable = cms.untracked.bool(True),
        threshold = cms.untracked.string('INFO')
    )
)

process.source = cms.Source('EmptyIOVSource',
    timetype = cms.string('runnumber'),
    firstValue = cms.uint64(1),
    lastValue = cms.uint64(1),
    interval = cms.uint64(1)
)

process.load('Geometry.HGCalMapping.hgCalModuleInfoESSource_cfi')
process.hgCalModuleInfoESSource.filename = 'Geometry/HGCalMapping/data/modulelocator_test.txt'
process.load('Geometry.HGCalMapping.hgCalSiModuleInfoESSource_cfi')
process.hgCalSiModuleInfoESSource.filename = 'Geometry/HGCalMapping/data/WaferCellMapTraces.txt'
process.load('Geometry.HGCalMapping.hgCalSiPMModuleInfoESSource_cfi')
process.hgCalSiPMModuleInfoESSource.filename = 'Geometry/HGCalMapping/data/channels_sipmontile.hgcal.txt'

process.analyzer = cms.EDAnalyzer("HGCalElectronicsMapESSourceTester",
                                  ModuleInfo = cms.ESInputTag(''),
                                  SiModuleInfo = cms.ESInputTag(''),
                                  SiPMModuleInfo = cms.ESInputTag(''), )

process.source = cms.Source('EmptySource')

process.maxEvents = cms.untracked.PSet(
    input = cms.untracked.int32(1)
)

process.p = cms.Path(
    process.analyzer
)
