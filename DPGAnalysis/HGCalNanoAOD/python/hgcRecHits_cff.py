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

tbMetaDataTable = cms.EDProducer("HGCalMetaDataTableProducer")

hgcRecHitsTask = cms.Task(hgcEERecHitsTable,hgcHEfrontRecHitsTable,hgcHEbackRecHitsTable,hgcEERecHitsPositionTable,hgcHEfrontRecHitsPositionTable)
hgctbTask = cms.Task(hgctbRecHitsTable,hgctbRecHitsPositionTable,hgcDigiTable,tbMetaDataTable)
