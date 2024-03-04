import FWCore.ParameterSet.Config as cms
from PhysicsTools.NanoAOD.common_cff import Var

hgcEERecHitsTable = cms.EDProducer("SimpleCaloRecHitFlatTableProducer",
    src = cms.InputTag("HGCalRecHit:HGCEERecHits"),
    cut = cms.string(""), 
    name = cms.string("RecHitHGCEE"),
    doc  = cms.string("RecHits in HGCAL Electromagnetic endcap"),
    singleton = cms.bool(False), # the number of entries is variable
    extension = cms.bool(False),
    variables = cms.PSet(
        detId = Var('detid().rawId()', 'int', precision=-1, doc='rechit detId'),
        energy = Var('energy', 'float', precision=14, doc='rechit energy'),
        time = Var('time', 'float', precision=14, doc='rechit time'),
    )
)

hgcEERecHitsPositionTable = cms.EDProducer("HGCRecHitPositionTableProducer",
     src = hgcEERecHitsTable.src,
     cut = hgcEERecHitsTable.cut,
     name = hgcEERecHitsTable.name,
     doc  = hgcEERecHitsTable.doc,
 )

hgcHEfrontRecHitsTable = hgcEERecHitsTable.clone()
hgcHEfrontRecHitsTable.src = "HGCalRecHit:HGCHEFRecHits"
hgcHEfrontRecHitsTable.name = "RecHitHGCHEF"

hgcHEfrontRecHitsPositionTable = cms.EDProducer("HGCRecHitPositionTableProducer",
     src = hgcHEfrontRecHitsTable.src,
     cut = hgcHEfrontRecHitsTable.cut,
     name = hgcHEfrontRecHitsTable.name,
     doc  = hgcHEfrontRecHitsTable.doc,
 )

hgcHEbackRecHitsTable = hgcEERecHitsTable.clone()
hgcHEbackRecHitsTable.src = "HGCalRecHit:HGCHEBRecHits"
hgcHEbackRecHitsTable.name = "RecHitHGCHEB"

hgctbRecHitsTable =  hgcEERecHitsTable.clone()
hgctbRecHitsTable.src = "hgCalRecHitsFromSoAproducer"
hgctbRecHitsTable.name = "HGC"

hgctbRecHitsPositionTable = hgcEERecHitsPositionTable.clone()
hgctbRecHitsPositionTable.src = hgctbRecHitsTable.src
hgctbRecHitsPositionTable.name = hgctbRecHitsTable.name

from Geometry.HGCalMapping.hgCalModuleInfoESSource_cfi import hgCalModuleInfoESSource # as hgCalModuleInfoESSource_
from Geometry.HGCalMapping.hgCalSiModuleInfoESSource_cfi import hgCalSiModuleInfoESSource #as hgCalSiModuleInfoESSource_

hgCalModuleInfoESSource.filename = 'Geometry/HGCalMapping/data/modulelocator_test.txt'
hgCalSiModuleInfoESSource.filename = 'Geometry/HGCalMapping/data/WaferCellMapTraces.txt'

hgcDigiTable = cms.EDProducer("HGCRecHitDigiTableProducer",
    srcHits = hgctbRecHitsTable.src,
    srcDigis = cms.InputTag("hgcalDigis:DIGI"),
    fixCalibChannel = cms.bool(True),  # FIXME
    cut = cms.string(""),
    name = hgctbRecHitsTable.name, # want to have the same name of the rechits
    doc  = cms.string("Digi in HGCAL Electromagnetic endcap"),
    singleton = cms.bool(False), # the number of entries is variable
    extension = cms.bool(False), # this is the main table for the muons
)

hgcSoaDigiTable = cms.EDProducer("HGCalSoADigisTableProducer")

hgcCMDigiTable = cms.EDProducer("SimpleHGCalDigiFlatTableProducer",
                                src = cms.InputTag("hgcalDigis:CM"),
                                cut = cms.string(""), 
                                name = cms.string("HGCCM"),
                                doc  = cms.string("Common mode words"),
                                singleton = cms.bool(False), # the number of entries is variable
                                extension = cms.bool(False),
                                variables = cms.PSet(
                                    eleid = Var('id().raw()', 'uint', precision=-1, doc='electronics id'),
                                    zSide = Var('id().zSide()', 'bool', precision=-1, doc='z side'),
                                    fedId = Var('id().fedId()','uint8', precision=-1, doc='FED index'),
                                    captureBlock = Var('id().captureBlock()', 'uint8', precision=-1, doc='capture block index (with FED)'),
                                    econdIdx = Var('id().econdIdx()', 'uint8', precision=-1, doc='ECON-D index (within capture block)'),
                                    econdeRx = Var('id().econdeRx()', 'uint8', precision=-1, doc='ECON-D e-Rx (within ECON-D)'),
                                    roc = Var('id().roc()','uint8', precision=-1, doc='roc'),
                                    half = Var('id().half()','uint8', precision=-1, doc='HGCROC half'), #% not accepted?
                                    halfrocChannel = Var('id().halfrocChannel()','uint8', precision=-1, doc='1/2 ROC channel'),
                                    isCM = Var('id().isCM()','bool', precision=-1, doc='is common mode'),
                                    cm = Var('adc', 'uint16', doc='common mode word'),
                                )
)

unpackerFlagsTable = cms.EDProducer("SimpleHGCalUnpackerFlagsTableProducer",
                                    src = cms.InputTag("hgcalDigis:UnpackerFlags"),
                                    cut = cms.string(""), 
                                    name = cms.string("HGCUnpackerFlags"),
                                    doc  = cms.string("Unpacker quality flags"),
                                    singleton = cms.bool(False), # the number of entries is variable
                                    extension = cms.bool(False),
                                    variables = cms.PSet(
                                        eleid = Var('eleid', 'uint', precision=-1, doc='electronics id'),
                                        flags = Var('flags', 'uint', precision=-1, doc='HGCalFlaggedECONDInfo flags word'),
                                        cbflags = Var('cbflags', 'uint', precision=-1, doc='Capture block flags for this ECON-D'),
                                        payload = Var('payload', 'uint', precision=-1, doc='ECON-D payload in the header'),
                                        iword = Var('iword', 'uint', precision=-1, doc='ECON-D 32b word position when unpacking FED data'),
                                    )
)



tbMetaDataTable = cms.EDProducer("HGCalMetaDataTableProducer")

hgcRecHitsTask = cms.Task(hgcEERecHitsTable,hgcHEfrontRecHitsTable,hgcHEbackRecHitsTable,hgcEERecHitsPositionTable,hgcHEfrontRecHitsPositionTable)
hgctbTask = cms.Task(hgcSoaDigiTable,tbMetaDataTable,hgcCMDigiTable,unpackerFlagsTable)
#august version
#hgctbTask = cms.Task(hgctbRecHitsTable,hgctbRecHitsPositionTable,hgcDigiTable,tbMetaDataTable)

