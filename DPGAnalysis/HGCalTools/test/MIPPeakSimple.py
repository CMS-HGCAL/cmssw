import os
import ROOT
import pandas as pd
import numpy as np
import argparse
from timeit import default_timer as timer
from datetime import timedelta


def simpleBeamSpotFinder(f,deadch,maxchPerModule=1):
    """from the profile of ADC-ADC(-1) identify the channels in each module with largest <ADC> as the candidates for beam spot notice this technique doesn't work for calibration channels which don't have ADC(-1)"""
    
    ROOT.ROOT.EnableImplicitMT()

    df=ROOT.RDataFrame('Events',f)

    #select good channels/events
    chsel = 'HGC_tctp==0 && HGC_adc>0 && HGC_adcm1>0 && HGC_chType==1'
    if len(deadch)>0:
        chsel += ' && '
        chsel += ' && '. join( [f'HGC_eleid!={ch}' for ch in deadch] )
    df=df.Define('goodch',chsel) \
         .Filter('Sum(goodch)>1') \
         .Filter('hgcMetadata_trigTime>0') \
         .Define(f'deltaADC',f'HGC_adc-HGC_adcm1') \
         .Define(f'HGC_ch','HGC_roc*72+HGC_half*36+HGC_halfrocChannel')

    #build profiles
    profiles={'deltaADC':[],'ADCm1':[]}
    for econd in [0,1]:
        df=df.Define(f'goodch_mod{econd}',f'goodch && HGC_econdIdx=={econd}') \
             .Define(f'ch_mod{econd}',f'HGC_ch[goodch_mod{econd}]') \
             .Define(f'deltaADC_mod{econd}',f'deltaADC[goodch_mod{econd}]') \
             .Define(f'ADCm1_mod{econd}',f'HGC_adcm1[goodch_mod{econd}]')
        profiles['deltaADC'].append( df.Profile1D( (f'prof_deltaADC_mod{econd}',f';Channel;ADC-ADC(-1)',222,0,222,'S'), f'ch_mod{econd}',f'deltaADC_mod{econd}').GetPtr() )
        profiles['ADCm1'].append( df.Profile1D( (f'prof_ADCm1_mod{econd}',f';Channel;ADC(-1)',222,0,222,'S'), f'ch_mod{econd}',f'ADCm1_mod{econd}').GetPtr() )        
        
    ROOT.ROOT.DisableImplicitMT()

    #determine the candidate channels from the deltaADC profiles
    def _rankChannels(profDeltaADC,profADCm1):
        """helper function to rank the channels"""

        vals_list=[]
        for b in range(1,profDeltaADC.GetNbinsX()+1):

            #if channel is at a fixed value it's probably dead
            noise=profADCm1.GetBinError(b)            
            if noise<=0 : continue
        
            vals_list.append( (b-1,profDeltaADC.GetBinContent(b),profADCm1.GetBinContent(b)) )
        
        cand_ch = [ p[0] for p in sorted(vals_list, key=lambda x:x[1], reverse=True)]
        cand_pedestals = [ p[2] for p in sorted(vals_list, key=lambda x:x[1], reverse=True)]

        return cand_ch[0:maxchPerModule], cand_pedestals[0:maxchPerModule]

    #build a dataframe for easier access
    chdf=[]
    for econd in [0,1]:
        selch,selped=_rankChannels( profiles['deltaADC'][econd], profiles['ADCm1'][econd] )
        for ch,ped in zip(selch,selped):
            erx=int(ch/36)
            halfrocch=ch%36
            chdf.append( [econd,erx,halfrocch,ch,ped] )
    chdf=pd.DataFrame(chdf,columns=['econd','erx','halfrocch','ch','Pedestal_adcm1'])
    
    return chdf,profiles


def readPedestals(ped):
    """reads the DQM outputs for pedestals and returns a list of dead channels and a dataframe for pedestals"""

    def _uniformizeEleId(df):

        """helper function to uniformize channel"""
        
        #string to integer
        df['EleID'] = df['Channel'].apply(lambda x: int(x, 16))

        #in case it's not a DTH run the modules come as two separate FEDs
        df['EleID'] = np.where(df['EleID']>0x40000,
                               df['EleID']-0x40000+0x400,
                               df['EleID'])

        df['halfrocch'] = df['EleID'].values & 0x3f
        df['erx'] = ((df['EleID'].values>>6) & 0xf)
        df['econd'] = ((df['EleID'].values>>10) & 0xf)
        df['ch'] = 36*df['erx']+df['halfrocch']
        df=df.rename(columns={'CM_slope':'CM_offset','CM_offset':'CM_slope'})

        return df
    
    #read pedestals and noise
    df=pd.read_csv(ped,sep='\s+',header='infer')
    df=_uniformizeEleId(df)

    #dead channels have fixed noise
    mask=(df['Noise']==0)
    deadch = df[mask]['EleID'].values
    
    return deadch,df[~mask][['econd','erx','halfrocch','ch','Pedestal','Noise','CM_slope','CM_offset']]



def getCMPedestals(f,deadch,maxSlink=2,maxCB=1,maxEcon=1,maxErx=6):
    """computes the CM pedestals returns the profiles and a dataframe"""

    ROOT.gInterpreter.Declare('#include "test/selection_helpers.h"')
    
    ROOT.ROOT.EnableImplicitMT()

    df=ROOT.RDataFrame('Events',f)

    chsel='HGC_chType==-1'
    if len(deadch)>0:
        chsel += ' && '
        chsel += ' && '. join( [f'HGC_eleid!={ch}' for ch in deadch] )

    maxIdx=ROOT.maxErxDenseIndex(maxSlink,maxCB,maxEcon,maxErx)
    df=df.Define('unconnCh',chsel) \
         .Define('erxDenseIndex',f'fillErxDenseIndex({maxSlink},{maxCB},{maxEcon},{maxErx})') \
         .Define('unconnCM',f'cmFromChannelList(HGC_eleid[unconnCh],HGC_adc[unconnCh],{maxSlink},{maxCB},{maxEcon},{maxErx})') \
         .Define('stdCM',f'cmFromChannelList(HGCCM_eleid,HGCCM_cm,{maxSlink},{maxCB},{maxEcon},{maxErx})')
    unconncm_prof = df.Profile1D( (f'prof_unconncm',f';Dense index;<CM> (unconn channels)',maxIdx+1,0,maxIdx+1),'erxDenseIndex','unconnCM').GetPtr()
    cm_prof = df.Profile1D( (f'prof_cm',f';Dense index;<CM>',maxIdx+1,0,maxIdx+1),'erxDenseIndex','stdCM').GetPtr()
    
    ROOT.ROOT.DisableImplicitMT()

    #build a dataframe for easier access
    cm_df=[]
    for b in range(unconncm_prof.GetNbinsX()):
        econd=int(b/maxErx)
        erx=b%maxErx
        cm_df.append( [econd,erx,unconncm_prof.GetBinContent(b+1), cm_prof.GetBinContent(b+1)] )
    cm_df=pd.DataFrame(cm_df,columns=['econd','erx','Pedestal_unconn_cm','Pedestal_cm'])
    
    return [unconncm_prof,cm_prof],cm_df








def saveBeamspotSnapshot(flist,mipch,fOut):


    ROOT.gInterpreter.Declare('#include "test/selection_helpers.h"')
    
    ROOT.ROOT.EnableImplicitMT()

    if type(flist)==str: flist=[flist]
    df=ROOT.RDataFrame('Events',flist)

    cols=[]
    for i,row in mipch.iterrows():

        #select specific channel
        chname=f'ch{i}'
        econd=row['econd']
        erx=row['erx']
        halfrocch=row['halfrocch']
        chsel=f'(HGC_econdIdx=={econd}) && (HGC_econdeRx=={erx}) && (HGC_halfrocChannel=={halfrocch})'
        df=df.Define(chname,chsel).Define(f'n{chname}',f'Sum({chname})')

        #set the status to true if the channel was found for this event
        newcol=f'status_{chname}'
        df=df.Define(newcol,f'n{chname}==1 ? kTRUE : kFALSE')
        cols.append(newcol)
        
        #add the columns for this channel (0 if not found)
        for c in ['econdIdx','econdeRx','halfrocChannel','tctp','adc','adcm1','toa','tot']:
            newcol,oldcol=f'{c}_{chname}',f'HGC_{c}'
            df=df.Define(newcol,f'n{chname}==1 ? {oldcol}[{chname}][0] : 0')
            cols.append(newcol)

        #add the calibration constants (a bit waste of space but handy later)
        for c in ['Pedestal_adcm1', 'Pedestal', 'Noise',  'CM_slope', 'CM_offset', 'Pedestal_unconn_cm', 'Pedestal_cm']:
            newcol=f'{c}_{chname}'
            val=row[c]
            df=df.Define(newcol,f'n{chname}==1 ? {val} : -1')
            cols.append(newcol)
            
        #define new columns (#TOA, CM, unconn_CM)
            
        
    df.Snapshot('Events',fOut,cols)
    ROOT.ROOT.DisableImplicitMT()


    
def main():
    
    outurl='/eos/user/p/psilva/histos.root'
    basedir='/eos/cms/store/group/dpg_hgcal/comm_hgcal/2023/CMSSW/ReReco_Sep26/'

    ped_run=f'{basedir}/Run1695716961/d1643b06-5cc3-11ee-bee0-fa163e17e03d/calibs/level0_calib_params.txt'
    run=f'{basedir}/Run1695564694/c764c29c-5cc3-11ee-bee0-fa163e17e03d'
    flist=[os.path.join(run,f) for f in os.listdir(run) if 'NANO' in f]
    
    
    #read pedestals
    start = timer()
    deadch,pedestals=readPedestals(ped_run)
    end = timer()
    dt=timedelta(seconds=end-start)
    print(f'Retrieved list of {len(deadch)} dead channels and {pedestals.shape[0]} pedestals (DQM-based)')
    print(f'Time elapsed {dt}')
    
    #compute CM pedestals
    start = timer()
    cmprofiles,cm_pedestals=getCMPedestals(flist[0],deadch)
    end = timer()
    dt=timedelta(seconds=end-start)
    print(f'Computed CM pedestals for {cm_pedestals.shape[0]} eRx')
    print(f'Time elapsed {dt}')
    
    #find beam spot and join with pedestals
    start = timer()
    mipch,profiles=simpleBeamSpotFinder(flist[0],deadch)
    mipch=mipch.merge(pedestals,on=['econd','erx','halfrocch','ch'],how='inner',suffixes=('',''))
    mipch=mipch.merge(cm_pedestals,on=['econd','erx'],how='inner',suffixes=('',''))
    end = timer()
    dt=timedelta(seconds=end-start)
    print(f'Golden channels for beamspot')
    print(mipch.head())
    print(f'Time elapsed {dt}')
    
    #save channel snapshot
    start = timer()
    saveBeamspotSnapshot(flist[0],mipch,outurl)
    end = timer()
    dt=timedelta(seconds=end-start)
    print(f'Saved snapshot for selected channels @ {outurl}')
    print(f'Time elapsed {dt}')
    
    
    #add plots to snapshot file
    fOut=ROOT.TFile.Open(outurl,'UPDATE')
    obj_list=cmprofiles
    for plist in profiles.values(): obj_list+=plist
    print(obj_list)
    for o in obj_list:
        o.Write()
    fOut.Close()
        

    
if __name__ == '__main__':
    main()
