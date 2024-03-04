import FWCore.ParameterSet.Config as cms
import FWCore.ParameterSet.VarParsing as VarParsing

process = cms.Process("RAWDATA")

options = VarParsing.VarParsing('standard')
options.register('fedId', 0, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int, 'FED id to print')
options.register('skipEvents', 0, VarParsing.VarParsing.multiplicity.singleton, VarParsing.VarParsing.varType.int, 'skip first N events')
options.parseArguments()


process.source = cms.Source("PoolSource",
                            fileNames=cms.untracked.vstring(options.files),
                            skipEvents=cms.untracked.uint32(options.skipEvents))                            
process.maxEvents = cms.untracked.PSet(input=cms.untracked.int32(options.maxEvents))

process.dump = cms.EDAnalyzer("DumpFEDRawDataProduct",
                              label = cms.untracked.InputTag('hgcalEmulatedSlinkRawData','hgcalFEDRawData'),
                              feds = cms.untracked.vint32(options.fedId),
                              dumpPayload = cms.untracked.bool(True) )
process.p=cms.Path(process.dump)


#process.outputRAW = cms.OutputModule("FRDOutputModule",
#                                     source = cms.InputTag('hgcalEmulatedSlinkRawData'),
#                                     frdVersion = cms.untracked.uint32(6),
#                                     frdFileVersion = cms.untracked.uint32(1),
#                                     fileName = cms.untracked.string(f'fed{options.fedId}rawdata.bin') )
#process.outpath=cms.EndPath( process.outputRAW )
