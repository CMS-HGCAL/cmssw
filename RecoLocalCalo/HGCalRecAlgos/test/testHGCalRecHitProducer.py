import FWCore.ParameterSet.Config as cms
run_on_gpu = False

n_hits_scale_value = 50000 #100000

if run_on_gpu:
    n_blocks_value=4096
    n_threads_value=1024
else:
    n_blocks_value=1024
    n_threads_value=4096

process = cms.Process('HGCalRecHitProducerTest')
process.source = cms.Source("PoolSource",
   fileNames = cms.untracked.vstring('file:/afs/cern.ch/user/y/yumiao/public/HGCAL_Raw_Data_Handling/Data/Digis/testFakeDigisSoA.root')
)
process.load('Configuration.StandardSequences.Accelerators_cff')
process.load('HeterogeneousCore.AlpakaCore.ProcessAcceleratorAlpaka_cfi')
process.load("FWCore.MessageLogger.MessageLogger_cfi")
process.load('CalibCalorimetry.HGCalPlugins.hgCalPedestalsESSource_cfi')
process.MessageLogger.cerr.FwkReport.reportEvery = cms.untracked.int32(1)
process.MessageLogger.cerr.INFO.limit = cms.untracked.int32(10000000)
# This doesn't work for some reason, so using a switch "run_on_gpu" for now
# process.testProducer = cms.EDProducer(
#     'HGCalRecHitProducer@alpaka',
#     alpaka = cms.untracked.Pset( backend = cms.untracked.string("serial_sync"))
# )
process.producerGpu = cms.EDProducer(
    'alpaka_cuda_async::HGCalRecHitProducer',
    digis = cms.InputTag('hgcalDigis', '', 'TEST'),
    n_blocks = cms.int32(n_blocks_value),
    n_threads = cms.int32(n_threads_value),
    n_hits_scale = cms.int32(n_hits_scale_value),
    pedestal_label = cms.string(''), # for hgCalPedestalsESSource
)
process.producerCpu = cms.EDProducer(
    'alpaka_serial_sync::HGCalRecHitProducer',
    digis = cms.InputTag('hgcalDigis', '', 'TEST'),
    n_blocks = cms.int32(n_blocks_value),
    n_threads = cms.int32(n_threads_value),
    n_hits_scale = cms.int32(n_hits_scale_value),
    pedestal_label = cms.string(''), # for hgCalPedestalsESSource
)
process.hgCalPedestalsESSource.filename = '/afs/cern.ch/work/y/ykao/public/raw_data_handling/calibration_parameters.txt'
process.output = cms.OutputModule("PoolOutputModule",
    fileName = cms.untracked.string("./recHits.root"),
#     outputCommands = cms.untracked.vstring(
#         'drop *',
#         'keep *_producerGpu_*_*',
#         'keep *_producerCpu_*_*',
#
)
process.path = cms.Path(process.producerGpu if run_on_gpu else process.producerCpu)
process.output_path = cms.EndPath(process.output)
process.maxEvents.input = 10
