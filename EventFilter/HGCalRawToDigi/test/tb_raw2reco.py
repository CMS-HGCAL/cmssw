import FWCore.ParameterSet.Config as cms
import FWCore.ParameterSet.VarParsing as VarParsing

process = cms.Process("TEST")

options = VarParsing.VarParsing('standard')
options.register('mode', 'hgcmodule', VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.string,
                 'type of emulation')
options.register('fedId', [0], VarParsing.VarParsing.multiplicity.list, VarParsing.VarParsing.varType.int,
                 'emulated FED id')
options.register('debug', False, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'debugging mode')
options.register('dumpFRD', False, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'also dump the FEDRawData content')
options.register('numCaptureBlocks', 1, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'number of capture blocks to emulate')
options.register('numECONDs', 1, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'number of ECON-Ds to emulate')
options.register('numChannelsPerERx', 37, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'number of channels enabled per ERx')
options.register('numERxsPerECOND', 6, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'number of ERxs enabled per ECON-D')
options.register('activeECONDs', [0], VarParsing.VarParsing.multiplicity.list, VarParsing.VarParsing.varType.int,
                 'list of ECON-Ds enabled')
options.register('ECONDsInPassthrough', [0], VarParsing.VarParsing.multiplicity.list, VarParsing.VarParsing.varType.int,
                 'list of ECON-Ds in passthrough mode')
options.register('ECONDsInCharacterisation', [], VarParsing.VarParsing.multiplicity.list, VarParsing.VarParsing.varType.int,
                 'list of ECON-Ds in characterisation mode')
options.register('ECONDToTStatus', 3, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'default ToT status bits (aka TcTp bits) value to be emulated')
options.register('randomActiveCaptureBlocks', False, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'randomly activate capture blocks on emulation')
options.register('randomActiveECOND', False, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'randomly activate ECOn-Ds on emulation')
options.register('storeOutput', True, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'also store the output into an EDM file')
options.register('storeRAWOutput', False, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'also store the RAW output into a streamer file')
options.register('storeEmulatorInfo', False, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'also store the emulator metadata')
options.register('slinkBOE', 0x2a, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,'Begin of event marker for S-link')
options.register('cbHeaderMarker', 0x5f, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,'Begin of event marker for BE/capture block')
options.register('econdHeaderMarker', 0x154, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,'Begin of event marker for ECON-D')
options.register('applyFWworkaround', False, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.bool,'Patch unpacker behavior to deal with firmware known features')
options.register(
    'configFile',
    '/eos/cms/store/group/dpg_hgcal/tb_hgcal/2023/calibration_module815/calib_withOct2022/80fC/80fC_inj_lowgain_loop_module815_beamtest/pedestal_run/run_20230412_160049/pedestal_run0_characModeOFF.yaml',
    VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.string, 'config yaml file')
options.register('conditions',
                 'default',
                 VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.string,
                 'conditions tag')
options.register('inputFiles',
                 'file:/eos/cms/store/group/dpg_hgcal/tb_hgcal/2023/labtest/module822/pedestal_run0.root',
                 VarParsing.VarParsing.multiplicity.list, VarParsing.VarParsing.varType.string,
                 'input TB file')
options.register('inputTrigFiles',
                 '',
                 VarParsing.VarParsing.multiplicity.list, VarParsing.VarParsing.varType.string,
                 'input Trigger link file')
options.register('GPU', False, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'run on GPU')
options.register('runNumber', 1, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int, 'run number')
options.register('maxEventsPerLS', 100000, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int, 'max. events per lumi section')
options.register('firstLS', 1, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int, 'first lumi section')

options.parseArguments()

#message logger
process.load("FWCore.MessageService.MessageLogger_cfi")
process.MessageLogger.cerr.FwkReport.reportEvery = 50000
if options.debug:    
    process.MessageLogger.cerr.threshold = "DEBUG"
    process.MessageLogger.debugModules = ["*"]
    process.MessageLogger.cerr.DEBUG = cms.untracked.PSet(
        limit = cms.untracked.int32(-1)
    )
process.options.wantSummary = cms.untracked.bool(True)

process.maxEvents = cms.untracked.PSet(input = cms.untracked.int32(options.maxEvents))
process.RandomNumberGeneratorService = cms.Service("RandomNumberGeneratorService",
    hgcalEmulatedSlinkRawData = cms.PSet(initialSeed = cms.untracked.uint32(42))
)

#source is empty source
process.source = cms.Source("EmptySource",
                            numberEventsInRun = cms.untracked.uint32(options.maxEvents),
                            firstRun = cms.untracked.uint32(options.runNumber),
                            numberEventsInLuminosityBlock = cms.untracked.uint32(options.maxEvents), #could use maxEventsPerLS),
                            firstLuminosityBlock = cms.untracked.uint32(options.firstLS) )

#RAW 2 DIGI and UNPACKER
process.load('EventFilter.HGCalRawToDigi.hgcalEmulatedSlinkRawData_cfi')
process.load('EventFilter.HGCalRawToDigi.hgcalDigis_cfi')
process.load('Configuration.StandardSequences.Accelerators_cff')
process.load('HeterogeneousCore.AlpakaCore.ProcessAcceleratorAlpaka_cfi')
# steer the emulator part
process.hgcalEmulatedSlinkRawData.emulatorType = options.mode
if process.hgcalEmulatedSlinkRawData.emulatorType == 'hgcmodule':
    #process.hgcalEmulatedSlinkRawData.treeName = cms.untracked.string('unpacker_data/hgcroc')
    process.hgcalEmulatedSlinkRawData.inputs = cms.untracked.vstring(options.inputFiles)
    process.hgcalEmulatedSlinkRawData.storeEmulatorInfo = bool(options.storeEmulatorInfo)
elif process.hgcalEmulatedSlinkRawData.emulatorType == 'slinkfromraw':
    process.hgcalEmulatedSlinkRawData.fedIds = cms.untracked.vuint32(options.fedId)
    process.hgcalEmulatedSlinkRawData.inputs = cms.untracked.vstring(options.inputFiles)
    process.hgcalEmulatedSlinkRawData.trig_inputs = cms.untracked.vstring(options.inputTrigFiles)

# steer the number of capture blocks
if options.randomActiveCaptureBlocks:
    from random import randint
    process.hgcalEmulatedSlinkRawData.slinkParams.numCaptureBlocks = randint(1, 50)  # randomise the number of capture blocks emulated
else:
    process.hgcalEmulatedSlinkRawData.slinkParams.numCaptureBlocks = options.numCaptureBlocks
print('S-link: number of capture blocks: {}'.format(
    process.hgcalEmulatedSlinkRawData.slinkParams.numCaptureBlocks.value()))

# steer the number (and/or list) of ECON-Ds per capture block
if options.numECONDs > 0:
    for i in range(options.numECONDs - len(process.hgcalEmulatedSlinkRawData.slinkParams.ECONDs)):
        process.hgcalEmulatedSlinkRawData.slinkParams.ECONDs.append(process.hgcalEmulatedSlinkRawData.slinkParams.ECONDs[0].clone())
    process.hgcalEmulatedSlinkRawData.slinkParams.checkECONDsLimits = False  # allows to mess with unconventional, high number
                                                                             # of ECON-Ds per capture block

econd_id=0
for econd in process.hgcalEmulatedSlinkRawData.slinkParams.ECONDs:
    # must use 'cms.' python configuration types
    if options.randomActiveECOND:  # randomly turn on/off any ECON-D in capture block
        from random import getrandbits
        econd.active = cms.bool(bool(getrandbits(1)))
    else:  # use a fixed, user-steered list of ECON-Ds in capture block
        econd.active = cms.bool((econd_id in options.activeECONDs))
    econd.passthroughMode = cms.bool((econd_id in options.ECONDsInPassthrough))
    econd.characterisationMode = cms.bool((econd_id in options.ECONDsInCharacterisation))
    econd.enabledERxs = cms.vuint32([i for i in range(options.numERxsPerECOND)])
    econd.numChannelsPerERx = cms.uint32(options.numChannelsPerERx)
    econd.defaultToTStatus = cms.uint32(options.ECONDToTStatus)
    print('ECON-D {}: active? {}, enabled eRxs: {}, number of channels/eRx: {}, passthrough? {}, characterisation? {}'.format(
        econd_id, bool(econd.active),
        [i for i in econd.enabledERxs], econd.numChannelsPerERx.value(),
        bool(econd.passthroughMode), bool(econd.characterisationMode)))
    econd_id += 1

# steer the unpacker
process.hgcalDigis.src = cms.InputTag('hgcalEmulatedSlinkRawData','hgcalFEDRawData')
process.hgcalDigis.fedIds = cms.vuint32(options.fedId)
process.hgcalDigis.maxCaptureBlock = process.hgcalEmulatedSlinkRawData.slinkParams.numCaptureBlocks
process.hgcalDigis.numERxsInECOND = options.numERxsPerECOND
process.hgcalDigis.captureBlockECONDMax = max(  # allows to mess with unconventional, high number of ECON-Ds per capture block
    process.hgcalDigis.captureBlockECONDMax,
    len([ec for ec in process.hgcalEmulatedSlinkRawData.slinkParams.ECONDs if ec.active]))

process.hgcalDigis.config_label = cms.ESInputTag('') # for HGCalConfigESSourceFromYAML
process.hgcalDigis.module_info_label = cms.ESInputTag('') # for HGCalModuleInfoESSource
process.hgcalDigis.slinkBOE=cms.uint32(options.slinkBOE)
process.hgcalDigis.cbHeaderMarker=cms.uint32(options.cbHeaderMarker)
process.hgcalDigis.econdHeaderMarker=cms.uint32(options.econdHeaderMarker)
process.hgcalDigis.applyFWworkaround=options.applyFWworkaround
    
#
# TRANSLATOR TO PHASE I COLLECTION
#
process.load('RecoLocalCalo.HGCalRecAlgos.hgCalRecHitsFromSoAproducer_cfi')

#
# CONDITIONS AND CONFIGURATIONS
#
# Configuration from YAML files
process.load('CalibCalorimetry.HGCalPlugins.hgCalConfigESSourceFromYAML_cfi') # read yaml config file(s)
process.hgCalConfigESSourceFromYAML.filename = options.configFile

# Alpaka ESProducer
process.hgcalCalibrationParameterESRecord = cms.ESSource('EmptyESSource',
    recordName = cms.string('HGCalCondSerializableModuleInfoRcd'),
    iovIsRunNotTime = cms.bool(True),
    firstValid = cms.vuint32(1)
)

process.hgcalCalibrationESProducer = cms.ESProducer('HGCalRecHitCalibrationESProducer@alpaka',
    filename = cms.string(''), # to be set up in configTBConditions
    ModuleInfo = cms.ESInputTag('')
)

# CONDITIONS
# RecHit producer: pedestal txt file for DIGI -> RECO calibration
# Logical mapping
process.load('CalibCalorimetry.HGCalPlugins.hgCalPedestalsESSource_cfi') 
process.load('Geometry.HGCalMapping.hgCalModuleInfoESSource_cfi')
process.load('Geometry.HGCalMapping.hgCalSiModuleInfoESSource_cfi')
from DPGAnalysis.HGCalTools.tb2023_cfi import configTBConditions,addPerformanceReports
configTBConditions(process,options.conditions)

process.load('HeterogeneousCore.CUDACore.ProcessAcceleratorCUDA_cfi')
if options.GPU:
    process.hgcalRecHit = cms.EDProducer( 'alpaka_cuda_async::HGCalRecHitProducer',
        digis = cms.InputTag('hgcalDigis', '', 'TEST'),
        eventSetupSource = cms.ESInputTag('hgcalCalibrationESProducer', ''),
        n_hits_scale = cms.int32(1),
        n_blocks = cms.int32(4096),
        n_threads = cms.int32(1024)
    )
else:
    process.hgcalRecHit = cms.EDProducer( 'alpaka_serial_sync::HGCalRecHitProducer',
        digis = cms.InputTag('hgcalDigis', '', 'TEST'),
        eventSetupSource = cms.ESInputTag('hgcalCalibrationESProducer', ''),
        n_hits_scale = cms.int32(1),
        n_blocks = cms.int32(1024),
        n_threads = cms.int32(4096)
    )

#filter on empty events
process.load('EventFilter.HGCalRawToDigi.hgCalEmptyEventFilter_cfi')
process.hgCalEmptyEventFilter.src = process.hgcalDigis.src
process.hgCalEmptyEventFilter.fedIds = process.hgcalDigis.fedIds

#main path
process.p = cms.Path(process.hgcalEmulatedSlinkRawData*process.hgCalEmptyEventFilter #RAW GENERATION (filtered on empty)
                     *process.hgcalDigis                                             #RAW->DIGI
                     *process.hgcalRecHit                                            #DIGI->RECO
                     *process.hgCalRecHitsFromSoAproducer                            #Phase I format translator (RecHits for NANO)                     
                     )

if options.dumpFRD:
    process.dump = cms.EDAnalyzer("DumpFEDRawDataProduct",
        label = cms.untracked.InputTag('hgcalEmulatedSlinkRawData','hgcalFEDRawData'),
        feds = cms.untracked.vint32(options.fedId),
        dumpPayload = cms.untracked.bool(True)
    )
    process.p *= process.dump


#output
process.outpath = cms.EndPath()
if options.storeOutput:
    process.output = cms.OutputModule("PoolOutputModule",
                                      fileName = cms.untracked.string(options.output),
                                      outputCommands = cms.untracked.vstring(
                                          'drop *',
                                          'keep *_hgcalEmulatedSlinkRawData_*_*',
                                          'keep *_hgcalDigis_*_*',
                                          'keep *_hgcalRecHit_*_*',
                                          'keep *_hgCalRecHitsFromSoAproducer_*_*',
                                      ),
                                      SelectEvents = cms.untracked.PSet( SelectEvents = cms.vstring('p') )
                                  )
    process.outpath += process.output

if options.storeRAWOutput:
    process.outputRAW = cms.OutputModule("FRDOutputModule",
                                         source = cms.InputTag('hgcalEmulatedSlinkRawData'),
                                         frdVersion = cms.untracked.uint32(6),
                                         frdFileVersion = cms.untracked.uint32(1),
                                         fileName = cms.untracked.string(options.output.replace('.root','.raw')),
                                         SelectEvents = cms.untracked.PSet( SelectEvents = cms.vstring('p') )
                                     )
    process.outpath += process.outputRAW

addPerformanceReports(process)
