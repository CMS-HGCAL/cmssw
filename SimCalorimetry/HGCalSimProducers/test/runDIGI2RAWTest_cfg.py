import FWCore.ParameterSet.Config as cms

# parse command line arguments
from FWCore.ParameterSet.VarParsing import VarParsing
options = VarParsing('standard')
options.register('modules', '/eos/cms/store/group/dpg_hgcal/comm_hgcal/psilva/Hackathon_2025Jan/modulelocator_Si.txt', VarParsing.multiplicity.singleton, VarParsing.varType.string,
                 "module locator file to use")
options.register('filter', 'fedid==0', VarParsing.multiplicity.singleton, VarParsing.varType.string,
                 "filter condition to apply (pandas/sql syntax)")
options.parseArguments()

#adapt module locator
import pandas as pd
df = pd.read_csv(options.modules, sep='\s+')
filt_df = df.query(options.filter)
import os
cmssw_base = os.environ['CMSSW_BASE']
outmoduleloc = f'{cmssw_base}/src/modulelocator.txt'
filt_df.to_csv(outmoduleloc, sep=' ', header=True, index=False)
fedlist = set(filt_df['fedid'].values.tolist())
modlist = set(filt_df['typecode'].values.tolist())
del df

eraConfig = {
  'modulemapper':outmoduleloc,
  'fedconfig':'/eos/cms/store/group/dpg_hgcal/comm_hgcal/psilva/Hackathon_2025May/fed_config.json',
  'modconfig':'/eos/cms/store/group/dpg_hgcal/comm_hgcal/psilva/Hackathon_2025May/module_config.json',
  'modcalib':None
}

#filter out config files
import json

# FED
for cfg_key, url in [
  ('modcalib','/eos/cms/store/group/dpg_hgcal/comm_hgcal/psilva/Hackathon_2025Jan/level0_calib_v2.json'),
]:
  with open(url) as stream:
    cfg = json.load(stream)
  sel_cfg = {}
  iter_list = fedlist if 'fed' in cfg_key else modlist
  for v in iter_list:
    key = str(v)
    sel_cfg[key] = cfg[key]
  outputurl = f'{cmssw_base}/src/{cfg_key}.json'
  with open(outputurl,'w') as stream:
    json.dump(sel_cfg, stream)
  eraConfig[cfg_key] = outputurl

print('Configuration is:')
import rich
rich.print(eraConfig)

#init process
from HGCalCommissioning.Configuration.SimulationEras_cff import initSimulationCMSProcess
process = initSimulationCMSProcess('LOCALDIGI2RAW', options.maxEvents, eraConfig)

# input configuration
process.maxEvents = cms.untracked.PSet( input = cms.untracked.int32(options.maxEvents) )
process.source = cms.Source(
    "PoolSource",
    fileNames=cms.untracked.vstring(*options.files)
)


process.hgcalTranslatedDigis = cms.EDProducer('HGCalDigiSoAFiller')
process.hgcalRealisticDigis = cms.EDProducer('HGCalRealisticDigisProducer')
process.hgcalRealisticDigis.ROCDigis = cms.untracked.InputTag('hgcalTranslatedDigis')
process.hgcalUnpackedDigis = cms.EDProducer('HGCalRawToDigi')

# timing
process.Timing = cms.Service(
    "Timing",
    summaryOnly=cms.untracked.bool(True),
    useJobReport=cms.untracked.bool(True)
)

# define the sequence
process.seq = cms.Sequence( process.hgcalTranslatedDigis*process.hgcalRealisticDigis*process.hgcalUnpackedDigis)
process.p = cms.Path( process.seq ) 

#output
process.output = cms.OutputModule(
  "PoolOutputModule",
  fileName=cms.untracked.string(options.output),
  outputCommands=cms.untracked.vstring(
    'drop *',
    'keep *_*_*_LOCALDIGI2RAW'
  ),
  SelectEvents=cms.untracked.PSet(SelectEvents=cms.vstring('p'))
)
process.outpath = cms.EndPath(process.output)


# Add early deletion of temporary data products to reduce peak memory need
from Configuration.StandardSequences.earlyDeleteSettings_cff import customiseEarlyDelete
process = customiseEarlyDelete(process)
