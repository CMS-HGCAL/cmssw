import FWCore.ParameterSet.Config as cms

def buildTemplatedCalib(modloc : str, wafmap : str, calibout : str):

  """this is temporary hack to generate on the file a 'passthrough' level0 calib file"""

  import pandas as pd
  import numpy as np
  from HGCalCommissioning.LocalCalibration.JSONEncoder import saveAsJson
  
  def _getCalibTemplate(nch) :
    z=np.zeros(nch).tolist()
    o=np.ones(nch).tolist()
    calib_templ_dict = {
      'Channel': [i for i in range(nch)],
      'ADC_ped': z,
      'Noise': z,
      'CM_ped': z,
      'CM_slope': z,
      'BXm1_slope': z,
      'BXm1_ped': z,
      'TOTtoADC': o,
      'TOT_ped': z,
      'TOT_lin': z,
      'TOT_P0': z,
      'TOT_P1': z,
      'TOT_P2': z,
      'TOA_CTDC': np.zeros((nch, 32)).tolist(),
      'TOA_FTDC': np.zeros((nch, 8)).tolist(),
      'TOA_TW': np.zeros((nch, 3)).tolist(),
      'MIPS_scale': o,
      'Valid': o
    }
    return calib_templ_dict


  #get typecodes needed from module locator
  df = pd.read_csv(modloc,sep='\\s+')
  typecodes = df['typecode'].apply(lambda x : x[0:4]).unique()

  #count the channels needed and generate calib constants accordingly
  df = pd.read_csv(wafmap, sep='\\s+')
  df = df[df['Typecode'].isin(typecodes)]

  calib_dict = {}
  for waf, group in df.groupby('Typecode'):
    calib_dict[waf+'*'] = _getCalibTemplate(nch=group.shape[0])

  saveAsJson(calibout, calib_dict)


#
# parse command line arguments
#
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

#adapt rechit config
outputlevel0calib = f'{cmssw_base}/src/level0_calib.json'
buildTemplatedCalib(outmoduleloc,
                    f'{cmssw_base}/src/Geometry/HGCalMapping/data/CellMaps/WaferCellMapTraces.txt',
                    outputlevel0calib)

#final configuration
eraConfig = {
  'modulemapper':outmoduleloc,
  'fedconfig':'/eos/cms/store/group/dpg_hgcal/comm_hgcal/psilva/Hackathon_2025May/fed_config.json',
  'modconfig':'/eos/cms/store/group/dpg_hgcal/comm_hgcal/psilva/Hackathon_2025May/module_config.json',
  'modcalib':outputlevel0calib
}

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
process.hgcalUnpackedDigis.src = cms.InputTag('hgcalRealisticDigis')

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
