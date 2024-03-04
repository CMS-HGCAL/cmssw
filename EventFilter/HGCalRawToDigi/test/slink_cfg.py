import FWCore.ParameterSet.Config as cms
import FWCore.ParameterSet.VarParsing as VarParsing

process = cms.Process("TEST")

options = VarParsing.VarParsing('standard')
options.register('mode', 'slinkfromraw', VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.string,
                 'type of emulation')
options.register('fedId', 0, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'emulated FED id')
options.register('debug', False, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'debugging mode')
options.register('dumpFRD', False, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'also dump the FEDRawData content')
options.register('numCaptureBlocks', 1, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'number of capture blocks to emulate')
options.register('numECONDs', -1, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'number of ECON-Ds to emulate')
options.register('numChannelsPerERx', 37, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'number of channels enabled per ERx')
options.register('numERxsPerECOND', 12, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'number of ERxs enabled per ECON-D')
options.register('activeECONDs', [], VarParsing.VarParsing.multiplicity.list, VarParsing.VarParsing.varType.int,
                 'list of ECON-Ds enabled')
options.register('ECONDsInPassthrough', [], VarParsing.VarParsing.multiplicity.list, VarParsing.VarParsing.varType.int,
                 'list of ECON-Ds in passthrough mode')
options.register('ECONDsInCharacterisation', [], VarParsing.VarParsing.multiplicity.list, VarParsing.VarParsing.varType.int,
                 'list of ECON-Ds in characterisation mode')
options.register('ECONDToTStatus', 3, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'default ToT status bits (aka TcTp bits) value to be emulated')
options.register('randomActiveCaptureBlocks', False, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'randomly activate capture blocks on emulation')
options.register('randomActiveECOND', False, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'randomly activate ECOn-Ds on emulation')
options.register('storeOutput', False, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'also store the output into an EDM file')
options.register('storeRAWOutput', True, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'also store the RAW output into a streamer file')
options.register('storeEmulatorInfo', False, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int,
                 'also store the emulator metadata')

options.maxEvents = 10  # number of events to emulate
options.output = 'output.root'  # output EDM file
options.secondaryOutput = 'output.raw'  # output streamer file
options.parseArguments()

process.load('EventFilter.HGCalRawToDigi.hgcalEmulatedSlinkRawData_cfi')
process.load('EventFilter.HGCalRawToDigi.hgcalDigis_cfi')

process.load("FWCore.MessageService.MessageLogger_cfi")
if options.debug:
    process.MessageLogger.cerr.threshold = "DEBUG"
    process.MessageLogger.debugModules = ["hgcalEmulatedSlinkRawData", "hgcalDigis"]

process.maxEvents = cms.untracked.PSet(input = cms.untracked.int32(options.maxEvents))

process.source = cms.Source("EmptySource")
process.hgcalEmulatedSlinkRawData.emulatorType = options.mode
if process.hgcalEmulatedSlinkRawData.emulatorType == 'slinkfromraw':
    process.hgcalEmulatedSlinkRawData.inputs = cms.untracked.vstring('/eos/cms/store/group/dpg_hgcal/tb_hgcal/2023/fakeRawData/Run1683391179_Link0_File0000000000.bin')
                                                                     # just for file handling testing
                                                                     #'/eos/cms/store/group/dpg_hgcal/tb_hgcal/2023/fakeRawData/Run1683391179_Link0_File0000000000.bin', 
                                                                     #'/eos/cms/store/group/dpg_hgcal/tb_hgcal/2023/fakeRawData/Run1683391179_Link0_File0000000001.bin')

    process.hgcalEmulatedSlinkRawData.treeName = cms.untracked.string('')

process.hgcalEmulatedSlinkRawData.storeEmulatorInfo = bool(options.storeEmulatorInfo)


# steer the unpacker
process.hgcalDigis.src = cms.InputTag('hgcalEmulatedSlinkRawData','hgcalFEDRawData')
process.hgcalDigis.fedIds = cms.vuint32(options.fedId)

process.p = cms.Path(process.hgcalEmulatedSlinkRawData * process.hgcalDigis)

process.outpath = cms.EndPath()

if options.storeOutput:
    process.output = cms.OutputModule("PoolOutputModule",
        fileName = cms.untracked.string(options.output),
        outputCommands = cms.untracked.vstring(
            'drop *',
            'keep *_hgcalEmulatedSlinkRawData_*_*',
            'keep *_hgcalDigis_*_*',
        )
    )
    process.outpath += process.output
    
if options.storeRAWOutput:
    process.outputRAW = cms.OutputModule("FRDOutputModule",
        source = cms.InputTag('hgcalEmulatedSlinkRawData'),
        frdVersion = cms.untracked.uint32(6),
        frdFileVersion = cms.untracked.uint32(1),
        fileName = cms.untracked.string(options.secondaryOutput)
    )
    process.outpath += process.outputRAW


