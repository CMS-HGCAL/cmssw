filein=$1
[[ -z ${filein} ]] && filein="file:/afs/cern.ch/user/p/psilva/public/forMariarosaria/output_numEvent100_injscan_withcalibparams.root"

fileout=$2
[[ -z ${fileout} ]] && fileout="hgcalnano_testcmsdriver.root"

jobtag=$3
[[ -z ${jobtag} ]] && jobtag=""

configfile=$4
[[ -z ${configfile} ]] && configfile="/eos/cms/store/group/dpg_hgcal/tb_hgcal/2023/calibration_module815/calib_withOct2022/80fC/80fC_inj_lowgain_loop_module815_beamtest/pedestal_run/run_20230412_160049/pedestal_run0_characModeOFF.yaml"

cmsDriver.py NANO \
    -s USER:DPGAnalysis/HGCalNanoAOD/hgcRecHits_cff.hgctbTask \
    --datatier NANOAOD \
    --eventcontent NANOAOD \
    --filein ${filein} \
    --fileout ${fileout} \
    -n -1 \
    --nThreads 4 \
    --conditions auto:phase2_realistic_T21 \
    --geometry Extended2026D95 \
    --era Phase2C17I13M9 \
    --python_filename nanocmsdriver_${jobtag}_cfg.py \
    --customise DPGAnalysis/HGCalTools/tb2023_cfi.configTBConditions,DPGAnalysis/HGCalTools/tb2023_cfi.addPerformanceReports \
    --customise_commands "process.load('CalibCalorimetry.HGCalPlugins.hgCalConfigESSourceFromYAML_cfi')\nprocess.hgCalConfigESSourceFromYAML.filename = "\""$configfile"\""\nprocess.NANOAODoutput.compressionAlgorithm = 'ZSTD'\nprocess.NANOAODoutput.compressionLevel = 5\nprocess.MessageLogger.cerr.FwkReport.reportEvery = 50000\nprocess.options.wantSummary = True\n" \
    --no_exec

cmsRun -j FrameworkJobReport_${jobtag}_NANO.xml nanocmsdriver_${jobtag}_cfg.py
