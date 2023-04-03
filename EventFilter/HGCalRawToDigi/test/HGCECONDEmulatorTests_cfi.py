import FWCore.ParameterSet.Config as cms

"""
this cfi aggregates different psets with parameters to be used in the HGCECONDEmulatorTest
for different tests of the algorithms
"""

passthroughTest = cms.PSet(
    repetitions = cms.uint32(100),
    neRx = cms.uint32(12),
    rocCharMode = cms.bool(False),
    econdPassthrough = cms.bool(True)
)

