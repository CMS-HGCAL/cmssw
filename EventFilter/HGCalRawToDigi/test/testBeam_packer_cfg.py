import FWCore.ParameterSet.Config as cms

process = cms.Process("TEST")

process.load('EventFilter.HGCalRawToDigi.hgcalEmulatedSlinkRawData_cfi')

process.MessageLogger = cms.Service("MessageLogger",
    cerr = cms.untracked.PSet(threshold = cms.untracked.string('DEBUG'))
)
process.maxEvents = cms.untracked.PSet(input = cms.untracked.int32(20))
process.RandomNumberGeneratorService = cms.Service("RandomNumberGeneratorService",
    hgcalEmulatedSlinkRawData = cms.PSet(initialSeed = cms.untracked.uint32(42))
)

#process.source = cms.Source("NewEventStreamFileReader",
#    fileNames = cms.untracked.vstring()
#)
process.source = cms.Source("EmptySource")

process.hgcalEmulatedSlinkRawData.inputs = cms.vstring(
    #'file:/eos/cms/store/group/dpg_hgcal/tb_hgcal/2022/sps_oct2022/electron_beam_100_160fC/beam_run/run_20221009_222828/beam_run0.root',
)
process.hgcalEmulatedSlinkRawData.econdParams.channelSurv = 0.5

process.dump = cms.EDAnalyzer("DumpFEDRawDataProduct",
    label = cms.untracked.InputTag('hgcalEmulatedSlinkRawData'),
    feds = cms.untracked.vint32(0),
    dumpPayload = cms.untracked.bool(True)
)

process.p = cms.Path(
    process.hgcalEmulatedSlinkRawData * process.dump
)

process.output = cms.OutputModule("PoolOutputModule",
    fileName = cms.untracked.string("output.root"),
    outputCommands = cms.untracked.vstring(
        'drop *',
        'keep *_hgcalEmulatedSlinkRawData_*_*',
    )
)
process.outputRAW = cms.OutputModule("FRDOutputModule",
    source = cms.InputTag('hgcalEmulatedSlinkRawData'),
    frdVersion = cms.untracked.uint32(6),
    frdFileVersion = cms.untracked.uint32(1),
    fileName = cms.untracked.string("output.raw")
)

process.outpath = cms.EndPath(process.output + process.outputRAW)
