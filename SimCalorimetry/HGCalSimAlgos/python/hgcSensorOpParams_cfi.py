import FWCore.ParameterSet.Config as cms
import re
from math import exp

def hgcSiSensorIleakRadDam(version,t=-30):

    """ 
    this method returns radiation damange constants of the leakage current for different versions
    {600V,800V}_annealing. The units are [1e-19 A/m]
    if version is unknown a ValueError exception is raised
    """
    
    # from values measured in 2022+2023 at -20C
    alphadict={400:6.7, 600: 8.04, 800: 11.1}
    v=int(re.findall('(\d+)V',version)[0])
    if not v in alphadict:
        raise ValueError('version={} is unknown to retrieve Ileak parameterization for HGC Si sensors'.format(version))
        
    def _scaleTo(t_op,t_meas=-20):
        Eb=1.21  #effective band gap energy
        kB=8.62E-5 #eV/K
        t0=273.15 #0 deg C
        t_meas=t0+t_meas
        t_op=t0+t_op
        return ((t_op/t_meas)**2)*exp((-Eb/(2*kB))*(1/t_op-1/t_meas))

    alpha=alphadict[v]*_scaleTo(t,-20)
    print(f'Will use alpha={alpha} for V={v}V and T={t}C')

    return alpha



def hgcSiSensorCCE(sensor,version):

    """ 
    this method returns different parameterizations of the charge collection efficiency (CCE)
    for different sensor types (sensor) and measurement versions (version)
    sensor = 120,200,300
    version = {600V,800V}_{nom,up,dn}_{annealing}   - 2023 base measurements at different voltages
    if the pair (sensor,version) is unknown a ValueError exception is raised
    """

    if version=='400V_nom_90m':
        if sensor==120  : return [-28.733693, 1100.407862]
        elif sensor==200: return [-30.417584, 1124.505054]
        elif sensor==300: return [-15.622096, 578.596145]
    if version=='400V_up_90m':
        if sensor==120  : return [-34.539155, 1313.791556]
        elif sensor==200: return [-35.775578, 1316.575315]
        elif sensor==300: return [-20.256177, 741.398882]
    if version=='400V_dn_90m':
        if sensor==120  : return [-22.928231, 887.024168]
        elif sensor==200: return [-25.059589, 932.434792]
        elif sensor==300: return [-10.988014, 415.793408]
    if version=='600V_nom_90m':
        if sensor==120  : return [-20.748488, 	821.624741]
        elif sensor==200: return [-27.472852, 	1033.207357]
        elif sensor==300: return [-20.879892, 	775.663642]
    if version=='600V_up_90m':
        if sensor==120  : return [-24.630752, 962.768599]
        elif sensor==200: return [-31.523520, 1176.318898]
        elif sensor==300: return [-24.819613, 912.723214]
    if version=='600V_dn_90m':
        if sensor==120  : return [-16.866224, 680.480884]
        elif sensor==200: return [-23.422184, 890.095817]
        elif sensor==300: return [-16.940171, 638.604070]
    if version=='800V_nom_90m':
        if sensor==120  : return [-16.541535, 677.591848]
        elif sensor==200: return [-21.841270, 842.617185]
        elif sensor==300: return [-20.663088, 780.950736]
    if version=='800V_up_90m':
        if sensor==120  : return [-20.832028, 833.111051]
        elif sensor==200: return [-26.711620, 1014.261129]
        elif sensor==300: return [-25.635598, 953.756809]
    if version=='800V_dn_90m':
        if sensor==120  : return [-12.251043, 522.072646]
        elif sensor==200: return [-16.970920, 670.973240]
        elif sensor==300: return [-15.690578, 608.144663]
        

    raise ValueError('sensor={} version={} is unknown to retrieve CCE parameterization for HGC Si sensors'.format(sensor,version))
