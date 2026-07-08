#ifndef TPGFEModuleEmulation_h
#define TPGFEModuleEmulation_h

#include <iostream>
#include <cstring>
#include <vector>
#include <cassert>
#include <cstddef>

#include "EventFilter/HGCalRawToDigi/interface/TPG/TPGFEDataformat.hh"

namespace TPGFEModuleEmulation{
  
  
    uint16_t CompressHgroc(uint32_t val, bool isldm){ // isldm stand for "is low density mode". It is determined by the SelTC4 parameter of HGCROC.
      
      //////The configuration of half of ROC based on HGCROC3a [doc. no. v2.0] (Read subsection 1.3.3 of Page-39)
      //////EDMS ROCv3a: https://edms.cern.ch/ui/#!master/navigator/document?D:100570166:100570166:subDocs
      //4E+3M
      //Ref:https://graphics.stanford.edu/%7Eseander/bithacks.html

      //controlled by the SelTC4 parameter of HGCROC (1/0 for LD/HD)
      
      val = (isldm)?val>>1:val>>3;
            
      uint32_t r = 0; // r will be log_2(v)
      uint32_t sub ;
      uint32_t shift ;
      uint32_t mant ;
      
      if (val > 7) {
        uint32_t v = val;
        r = 0;
        while (v >>= 1) r++;
        sub = r - 2;
        shift = r - 3;
        if (sub <= 0xF) {
          mant = (val >> shift) & 0x7;
        } else {
          sub = 0xF;
          mant = 0x7;
        }
      } else {
        r = 0;
        sub = 0;
        shift = 0;
        mant = val & 0x7;
      }
  
      uint16_t cdata = (sub<<3) | mant;
  
      return cdata;
    };
    

  
    uint32_t DecompressEcont(uint16_t compressed, bool density){
      //4E+3M with midpoint correction
      //controlled by the density parameter of ECON-T (0/1 stands for LD/HD and represents 1/3 bit shifts)
      uint32_t mant = compressed & 0x7;
      uint32_t expo = (compressed>>3) & 0xF;
      
      if(expo==0) return (density) ? (mant<<3)+4 : (mant<<1)+1 ; //The the +4/+1 are midpoint corrections for expo==0
      
      uint32_t shift = expo+2; 
               shift += (density) ? 3 : 1;
      uint32_t decomp = 1<<shift; 
      uint32_t mpdeco = 1<<(shift-4);
      decomp = decomp | (mant<<(shift-3));
      decomp = decomp | mpdeco;

      return decomp;
    };

    uint16_t CompressEcontStc4E3M(uint64_t val, uint32_t dropLSB){
      //4E+3M
      //controlled by the dropLSB parameter of ECON-T 
      val = val>>dropLSB;  
      
      uint32_t r = 0; // r will be lg(v)
      uint32_t sub ;
      uint32_t shift ;
      uint32_t mant ;
      
      if(val>7){
        uint64_t v = val; 
        r = 0; 
        while (v >>= 1) r++;
        sub = r - 2;
        shift = r - 3;
        if(sub<=0xF){
        mant = (val>>shift) & 0x7;
        }else{
        sub = 0xF;
        mant = 0x7;
        }
        }else{
        r = 0;
        sub = 0;
        shift = 0;
        mant = val & 0x7;
      }
      
      sub = sub & 0xF;
      mant = mant & 0x7;
      
      uint16_t packed = (sub<<3) | mant;
      
      return packed;
    }
    
    uint16_t CompressEcontStc5E4M(uint64_t val){
      //5E+4M
      //dropLSB is not applicable for 5E+4M

      uint32_t r = 0; // r will be lg(v)
      uint32_t sub ;
      uint32_t shift ;
      uint32_t mant ;
      
      if(val>0xF){
        uint64_t v = val; 
        r = 0; 
        while (v >>= 1) r++;
        sub = r - 3;         
        shift = r - 4;       
	    if(sub<=0x1F){
	      mant = (val>>shift) & 0xF;
	    }else{
        sub = 0x1F;
        mant = 0xF;
	    }
      }else{
        r = 0;
        sub = 0;
        shift = 0;
        mant = val & 0xF;
      }

      sub = sub & 0x1F;
      mant = mant & 0xF;

      uint16_t packed = (sub<<4) | mant;
      
      return packed;
    }
    
   uint16_t CompressEcontBc(uint64_t val, uint32_t dropLSB){
      //4E+3M
      //controlled by the dropLSB parameter of ECON-T 
      val = val>>dropLSB;  
      
      uint32_t r = 0; // r will be lg(v)
      uint32_t sub ;
      uint32_t shift ;
      uint32_t mant ;
  
      if(val>7){
        uint64_t v = val; 
        r = 0; 
        while (v >>= 1) r++;
        sub = r - 2;
        shift = r - 3;
        if(sub<=0xF){
	        mant = (val>>shift) & 0x7;
	      }else{
          sub = 0xF;
          mant = 0x7;
	      }
      }else{
        r = 0;
        sub = 0;
        shift = 0;
        mant = val & 0x7;
      }
      
      sub = sub & 0xF;
      mant = mant & 0x7;

      uint16_t packed = (sub<<3) | mant;
      
      return packed;
    }


    
    uint16_t CompressEcontModsum(uint64_t val, uint32_t dropLSB){
      //5E+3M
      //controlled by the dropLSB parameter of ECON-T 
      //val = val>>dropLSB;  

      uint32_t r = 0; // r will be lg(v)
      uint32_t sub ;
      uint32_t shift ;
      uint32_t mant ;
      
      if(val>7){
        uint64_t v = val; 
        r = 0; 
        while (v >>= 1) r++;
        sub = r - 2;
        shift = r - 3;
        if(sub<=0x1F){
          mant = (val>>shift) & 0x7;
        }else{
          sub = 0x1F;
          mant = 0x7;
        }
      }else{
        r = 0;
        sub = 0;
        shift = 0;
        mant = val & 0x7;
      }
      
      sub = sub & 0x1F;
      mant = mant & 0x7;
      
      uint16_t packed = (sub<<3) | mant;
 
      return packed;
    }





    
  uint64_t decodeEmulatedE(TPGFEDataformat::Type t, uint16_t compressed ) {
      uint64_t decompressed = 0;
      switch(t){
      case TPGFEDataformat::BestC:
        decompressed = TPGFEDataformat::TcRawData::Decode4E3M(compressed);
        break;
      case TPGFEDataformat::STC4A:
        decompressed = TPGFEDataformat::TcRawData::Decode4E3M(compressed);
        break;
      case TPGFEDataformat::STC4B:
        decompressed = TPGFEDataformat::TcRawData::Decode5E4M(compressed);
        break;
      case TPGFEDataformat::STC16:
        decompressed = TPGFEDataformat::TcRawData::Decode5E4M(compressed);
        break;
      case TPGFEDataformat::CTC4A:
        decompressed = TPGFEDataformat::TcRawData::Decode4E3M(compressed);
        break;
      case TPGFEDataformat::CTC4B:
        decompressed = TPGFEDataformat::TcRawData::Decode5E4M(compressed);
        break;
      default: //to allow unknown type
        ;
      }
      return decompressed;
    }


}//end of namespace


#endif
