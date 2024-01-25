import ROOT
import sys
import itertools
import re

def main():

    ROOT.gROOT.SetBatch(True)
    ROOT.gStyle.SetOptStat(0)
    ROOT.gStyle.SetOptTitle(0)

    url=sys.argv[1]
    fIn=ROOT.TFile.Open(url)

    rgx=re.compile("(.*)_(.*)_(.*)_(.*).root")
    fname,voltage,ann,fscale=re.findall(rgx,url)[0]
    
    prof_list=['cce','mip','gain','encs','encp','enc','son','son_up','son_cm','ileak']
    graphs={}
    labels={}
    
    for sens,prof in itertools.product([0,1,2,3],prof_list):

        if prof in ['cce','mip'] and sens==3: continue
        
        nom=fIn.Get(f'conds_nom/sensor{sens}{prof}vsfluence')
        up=fIn.Get(f'conds_up/sensor{sens}{prof}vsfluence')
        dn=fIn.Get(f'conds_dn/sensor{sens}{prof}vsfluence')

        graphs[(sens,prof)]=ROOT.TGraphAsymmErrors()
        graphs[(sens,prof)].SetName(f'sensor{sens}{prof}vsfluence_gr')
        labels[(sens,prof)]=(nom.GetTitle(),nom.GetXaxis().GetTitle(),nom.GetYaxis().GetTitle())
        if prof in ['cce','mip']:
            thick=120 if sens==0 else 100+sens*100
            labels[(sens,prof)]=(f'{thick}#mum',nom.GetXaxis().GetTitle(),nom.GetYaxis().GetTitle())
        
        for i in range(nom.GetNbinsX()):
            f=nom.GetXaxis().GetBinCenter(i+1)

            val=nom.GetBinContent(i+1)
            ex_lo=f-nom.GetXaxis().GetBinLowEdge(i+1)
            ex_hi=nom.GetXaxis().GetBinUpEdge(i+1)-f
            ey_hi,ey_lo=0.,0.

            #assign the error
            val_up=up.GetBinContent(i+1)
            val_dn=dn.GetBinContent(i+1)
            if val_up>val: ey_hi=val_up-val
            else:          ey_lo=val-val_up
            if val_dn>val and val_dn>val_up: ey_hi=val_dn-val            
            elif val_dn<val and val_dn<val_up: ey_lo=val-val_dn

            graphs[(sens,prof)].SetPoint(i,f,val)
            graphs[(sens,prof)].SetPointError(i,ex_lo,ex_hi,ey_lo,ey_hi)

    def _addLegend(x,y):
        txt=ROOT.TLatex()
        txt.SetTextFont(42)
        txt.SetTextSize(0.045)
        txt.SetNDC()
        txt.DrawLatex(x,y,'#bf{CMS} #it{simulation}')
        txt.DrawLatex(x,y-0.08,f'{voltage} V ({ann} min. ann.)')
            
    c=ROOT.TCanvas('c','c',600,600)
    c.SetTopMargin(0.08)
    c.SetBottomMargin(0.1)
    c.SetRightMargin(0.05)
    c.SetLeftMargin(0.12)
    colors={0:2,1:8,2:9,3:6}
    markers={0:20,1:24,2:24,3:20}
    frame=ROOT.TH1F('frame','frame',1,10**13,10**(16.5))
    frame.GetXaxis().SetTitleOffset(1.2)
    ran={'cce':(0,1),'mip':(0,4),'gain':(0,400),
         'encs':(0,1),'encp':(0,1),'enc':(0,1),
         'son':(0,10),'son_up':(0,10),'son_cm':(0,10),'ileak':(0,100)}
    legpos={'cce':(0.14,0.2),'mip':(0.14,0.2),'gain':(0.14,0.2),
            'encs':(0.14,0.86),'encp':(0.14,0.86),'enc':(0.14,0.86),
            'son':(0.14,0.2),'son_up':(0.14,0.2),'son_cm':(0.14,0.2),'ileak':(0.14,0.2)}
    
    for prof in prof_list:

        c.Clear()

        leg=ROOT.TLegend(0.12,0.94,0.95,0.99)
        leg.SetFillStyle(0)
        leg.SetNColumns(4)
        leg.SetBorderSize(0)
        
        mg=ROOT.TMultiGraph()
        for sens in range(0,4):
            if not (sens,prof) in labels: continue
            label=labels[(sens,prof)]
            graphs[(sens,prof)].SetTitle(label[0])
            graphs[(sens,prof)].SetFillColor(colors[sens])
            graphs[(sens,prof)].SetMarkerColor(colors[sens])
            graphs[(sens,prof)].SetLineColor(colors[sens])
            graphs[(sens,prof)].SetMarkerStyle(markers[sens])
            mg.Add(graphs[(sens,prof)],'e2p')
            leg.AddEntry(graphs[(sens,prof)],label[0],'ep')

        frame.Draw()
        frame.GetXaxis().SetTitle(label[1])
        frame.GetYaxis().SetTitle(label[2])
        mg.Draw()
        leg.Draw()
        frame.GetYaxis().SetRangeUser(*ran[prof])
        _addLegend(*legpos[prof])
        c.SetLogx()
        c.SetGridx()
        c.SetGridy()
        c.Modified()
        c.Update()
        c.SaveAs(f'{prof}_{voltage}V_{ann}m.png')
        c.SaveAs(f'{prof}_{voltage}V_{ann}m.pdf')
            
        
            
            
if __name__ == '__main__':
    sys.exit(main())
