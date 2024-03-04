import FWCore.ParameterSet.Config as cms
process = cms.Process("Calib")

# argument parser
from FWCore.ParameterSet.VarParsing import VarParsing
options = VarParsing('python')
options.register('verb',1,mytype=VarParsing.varType.int,
                 info="Verbosity level, 0=normal, 1=debug (default)")
options.register('inputFile',None,mytype=VarParsing.varType.string,
                 info="Path to input file. Absolute, or relative to CMSSW src directory,"
                      " e.g. CalibCalorimetry/HGCalPlugins/test/pedestals_test.txt")
options.parseArguments()
#infname = '/afs/cern.ch/work/y/ykao/public/raw_data_handling/calibration_parameters.txt'
infname = options.inputFile
verb    = options.verb
print(f">>> inputFile={infname}")

process.MessageLogger = cms.Service("MessageLogger",
    debugModules = cms.untracked.vstring("HGCalPedestalsESSource*"),
    cerr = cms.untracked.PSet(
        enable = cms.untracked.bool(True),
        threshold = cms.untracked.string('DEBUG' if options.verb>=1 else 'INFO')
    ),
    cout = cms.untracked.PSet(
        enable = cms.untracked.bool(True),
        threshold = cms.untracked.string('DEBUG' if options.verb>=1 else 'INFO')
    )
)

process.source = cms.Source('EmptyIOVSource',
    timetype = cms.string('runnumber'),
    firstValue = cms.uint64(1),
    lastValue = cms.uint64(1),
    interval = cms.uint64(1)
)

process.load('CalibCalorimetry.HGCalPlugins.hgCalPedestalsESSource_cfi')
if infname:
  process.hgCalPedestalsESSource.filename = infname
else: # create test file with dummy variables
  process.hgCalPedestalsESSource.filename = 'CalibCalorimetry/HGCalPlugins/test/pedestals_test.txt'
  
  # generate values for a test file
  import numpy as np
  import itertools
  import scipy.constants as k
  consts=np.array([k.pi,k.golden,k.c/1e8,k.g,k.h/1e-34,k.alpha/1e-2,np.exp(1)])
  rows=[]
  for erx,ich in itertools.product(range(6),range(39)):
      
      # identifier
      cmflag=(ich>36)
      ch=ich%39
      id=((erx & 0xf)<<6) | (ch & 0x3f)
      
      # random values
      np.random.shuffle(consts)
      vals=consts[0:4]
      rows.append( [id] + vals.tolist() )
  
  # write the test file    
  import pandas as pd
  from pathlib import Path
  import os
  test_dir = os.path.dirname(Path( __file__ ).absolute())
  df = pd.DataFrame(rows,columns=['Channel','Pedestal','CM_slope','CM_offset','kappa_BXm1'])
  df.to_csv(os.path.join(test_dir,'pedestals_test.txt'),index=False,sep=' ',header=True, mode='w')

#instantiate the analyzer
process.analyzer = cms.EDAnalyzer("HGCalPedestalsESSourceAnalyzer",
                                  label = cms.string(''))

process.source = cms.Source('EmptySource')

process.maxEvents = cms.untracked.PSet(
    input = cms.untracked.int32(1)
)

process.p = cms.Path(
    process.analyzer
)
