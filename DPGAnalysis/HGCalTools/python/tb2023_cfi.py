import FWCore.ParameterSet.Config as cms


def addPerformanceReports(process, addMemCheck=False):

    # add timing and mem (too slow) for FWK jobs report
    process.Timing = cms.Service("Timing",
                                 summaryOnly=cms.untracked.bool(True),
                                 useJobReport=cms.untracked.bool(True))

    if addMemCheck:
        process.SimpleMemoryCheck = cms.Service("SimpleMemoryCheck",
                                                ignoreTotal=cms.untracked.int32(1),
                                                jobReportOutputOnly=cms.untracked.bool(True))

    return process


def configTBConditions_default(process):
    return configTBConditions(process, key='default')

def configTBConditions_DTH(process):
    return configTBConditions(process, key='DTH')

def configTBConditions_MLFL00041(process):
    return configTBConditions(process, key='MLFL00041')
def configTBConditions_MLFL00041ped(process):
    return configTBConditions(process, key='MLFL00041ped')
def configTBConditions_MLDSL57(process):
    return configTBConditions(process, key='MLDSL57')
def configTBConditions_MLDSL57ped(process):
    return configTBConditions(process, key='MLDSL57ped')
def configTBConditions_MLDSR01(process):
    return configTBConditions(process, key='MLDSR01')
def configTBConditions_MLDSR01ped(process):
    return configTBConditions(process, key='MLDSR01ped')
def configTBConditions_MHDF56(process):
    return configTBConditions(process, key='MHDF56')
def configTBConditions_MHDF56ped(process):
    return configTBConditions(process, key='MHDF56ped')

def configTBConditions(process, key='default'):
    """ maybe here we should also set the emulator/unpacker configs in case they are in the process (see comments in the methods above) """

    modulelocator_dict = {
        'default': 'Geometry/HGCalMapping/data/modulelocator_tb.txt',
        'DTH': 'Geometry/HGCalMapping/data/modulelocator_tb_DTH.txt',
        'MLFL00041': 'Geometry/HGCalMapping/data/modulelocator_tb_MLFL00041.txt',
        'MLDSL57': 'Geometry/HGCalMapping/data/modulelocator_tb_MLDSL57.txt',        
        'MLDSR01': 'Geometry/HGCalMapping/data/modulelocator_tb_MLDSR01.txt',
        'MHDF56': 'Geometry/HGCalMapping/data/modulelocator_tb_MHDF56.txt',
    }

    modtypekey=key.replace('ped','')
    modulelocator = modulelocator_dict[modtypekey] if modtypekey in modulelocator_dict else modulelocator_dict['default']

    #yet some additional era-dependent configs
    if modtypekey in ['MLFL00041','MLDSL57','MLDSR01','MHDF56'] and hasattr(process, 'hgcalEmulatedSlinkRawData'):
        process.hgCalConfigESSourceFromYAML.charMode = 1 if 'ped' in key else 0
        print(f'charMode = {process.hgCalConfigESSourceFromYAML.charMode}')
        maxERx=6
        if 'MHDF56' in key: maxERx=12
        if 'MLDSL57' in key or 'MLDSR01' in key: maxERx=3
        econd_id=0
        for econd in process.hgcalEmulatedSlinkRawData.slinkParams.ECONDs:
            econd.passthroughMode=cms.bool(True)
            econd.characterisationMode=cms.bool(True)
            econd.enabledERxs=cms.vuint32([i for i in range(maxERx)])
            econd.active=cms.bool( (econd_id==0) )
            econd_id+=1
        print(f'key is {key} maxERx set to {maxERx} and charMode={process.hgCalConfigESSourceFromYAML.charMode}')
    
    process.hgCalModuleInfoESSource.filename = modulelocator
    process.hgCalSiModuleInfoESSource.filename = 'Geometry/HGCalMapping/data/WaferCellMapTraces.txt'
    print(f'Module locator is {modulelocator}')

    
    pedestals_dict = {
        'default': '/eos/cms/store/group/dpg_hgcal/comm_hgcal/ykao/calibration_parameters_v2.txt',
    }
    pedestals = pedestals_dict[key] if key in pedestals_dict else pedestals_dict['default']

    if hasattr(process, 'hgCalPedestalsESSource'):
        process.hgCalPedestalsESSource.filename = pedestals
    if hasattr(process, 'hgcalCalibESProducer'):
        process.hgcalCalibESProducer.filename = pedestals

    return process
