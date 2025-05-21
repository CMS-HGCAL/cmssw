import FWCore.ParameterSet.Config as cms

def customise_hgcalmapper(process,
                          modules = 'Geometry/HGCalMapping/data/ModuleMaps/modulelocator_test.txt',
                          sicells = 'Geometry/HGCalMapping/data/CellMaps/WaferCellMapTraces.txt',
                          sipmcells = 'Geometry/HGCalMapping/data/CellMaps/channels_sipmontile.hgcal.txt',
                          offsetfile = 'Geometry/HGCalMapping/data/CellMaps/calibration_to_surrounding_offsetMap.txt',
                          sitypecodeformat = '(([MX])([LH])-([FTBLR5])).*',
                          sipmtypecodeformat = 'TB-L.*-S.*'):
    """the following function configures the mapping producers
    NOTE: for production-targetted configs should be avoided as it checks if the process as 
    already the Accelerators sequence loaded, if not it loads it to the process"""

    print("CUSTOMIZE")
    print("sipmcells: ", sipmcells)
    print("offsetfile: ", offsetfile)
    offsetfilepath = cms.FileInPath(offsetfile)
    print("DONE")
    process.load('Geometry.HGCalMapping.hgCalMappingESProducer_cfi')
    process.hgCalMappingESProducer.modules = cms.FileInPath(modules)
    process.hgCalMappingESProducer.si = cms.FileInPath(sicells)
    process.hgCalMappingESProducer.sipm = cms.FileInPath(sipmcells)
    process.hgCalMappingESProducer.sitypecodeformat = cms.string(sitypecodeformat)
    process.hgCalMappingESProducer.sipmtypecodeformat = cms.string(sipmtypecodeformat)

    if not hasattr(process, 'ProcessAcceleratorCUDA'):
        process.load('Configuration.StandardSequences.Accelerators_cff')

    print("offsetfilepath: ", offsetfilepath)
        
    process.hgCalMappingCellESProducer = cms.ESProducer('hgcal::HGCalMappingCellESProducer@alpaka',
                                                        filelist=cms.vstring(sicells, sipmcells),
                                                        cellindexer=cms.ESInputTag(''),
                                                        offsetfile=cms.FileInPath(offsetfile),
                                                        sipmtypecodeformat=cms.string(sipmtypecodeformat))
    process.hgCalMappingModuleESProducer = cms.ESProducer('hgcal::HGCalMappingModuleESProducer@alpaka',
                                                          filename=cms.FileInPath(modules),
                                                          moduleindexer=cms.ESInputTag(''),
                                                          sipmtypecodeformat=cms.string(sipmtypecodeformat))
    process.hgCalDenseIndexInfoESProducer = cms.ESProducer('hgcal::HGCalDenseIndexInfoESProducer@alpaka',
                                                           moduleindexer=cms.ESInputTag('') )
    
    return process
