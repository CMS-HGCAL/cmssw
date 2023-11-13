import FWCore.ParameterSet.Config as cms
process = cms.Process("TEST")

from FWCore.ParameterSet.VarParsing import VarParsing
options = VarParsing('python')
options.register('modules','Geometry/HGCalMapping/data/modulelocator.txt',mytype=VarParsing.varType.string,
                 info="Path to module mapper. Absolute, or relative to CMSSW src directory")
options.register('sicells','Geometry/HGCalMapping/data/WaferCellMapTraces.txt',mytype=VarParsing.varType.string,
                 info="Path to Si cell mapper. Absolute, or relative to CMSSW src directory")
options.register('sipmcells','Geometry/HGCalMapping/data/channels_sipmontile.hgcal.txt',mytype=VarParsing.varType.string,
                 info="Path to SiPM-on-tile cell mapper. Absolute, or relative to CMSSW src directory")
options.parseArguments()

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

#ESSources/Producers for the logical mapping
process.load('Geometry.HGCalMapping.hgCalMappingIndexESSource_cfi')
process.hgCalMappingIndexESSource.modules = options.modules

process.load('Configuration.StandardSequences.Accelerators_cff')
process.load('HeterogeneousCore.AlpakaCore.ProcessAcceleratorAlpaka_cfi')
process.load('HeterogeneousCore.CUDACore.ProcessAcceleratorCUDA_cfi')
process.hgCalMappingModuleESProducer = cms.ESProducer('hgcal::HGCalMappingModuleESProducer@alpaka',
                                                      filename=cms.string(options.modules),
                                                      moduleindexer=cms.ESInputTag('') )
process.hgCalMappingSiCellESProducer = cms.ESProducer('hgcal::HGCalMappingSiCellESProducer@alpaka',
                                                      filename=cms.string(options.sicells),
                                                      cellindexer=cms.ESInputTag('') )
process.hgCalMappingSiPMCellESProducer = cms.ESProducer('hgcal::HGCalMappingSiPMCellESProducer@alpaka',
                                                        filename=cms.string(options.sipmcells),
                                                        cellindexer=cms.ESInputTag('') )

process.analyzer = cms.EDAnalyzer("HGCalMappingIndexESSourceTester")

process.p = cms.Path(process.analyzer)
