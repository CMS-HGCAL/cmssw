#include "EventFilter/HGCalRawToDigi/interface/HGCalUnpackerTrigger.h"
#include "EventFilter/HGCalRawToDigi/interface/TPG/TPGFEDataformat.hh"
#include "EventFilter/HGCalRawToDigi/interface/TPG/TPGBEDataformat.hh"
#include "EventFilter/HGCalRawToDigi/interface/TPG/Stage1IO.hh"
#include "EventFilter/HGCalRawToDigi/interface/TPG/TpgSubpacketHeader.h"

using namespace hgcal;

bool HGCalUnpackerTrigger::parseFEDData(unsigned fedId,
                                        const RawFragmentWrapper& fed_data,
                                        const HGCalTriggerConfiguration& config,
                                        const HGCalMappingModuleIndexerTrigger& moduleIndexer,
                                        hgcaldigi::HGCalDigiTriggerHost& digisTrigger,
					hgcaldigi::HGCalECONTPacketInfoHost& econtPacketInfo) {
  
  // Endianness assumption
  // From 32-bit word(ECOND) to 64-bit word(capture block): little endianness
  // Others: big endianness
  
  // TODO: if this also depends on the unpacking configuration, it should be moved to the specialization
  //const auto& fedConfig = config.feds[fedId];
  const auto& fedReadoutSequence = moduleIndexer.fedReadoutSequences()[fedId];
  const auto* start_fed_data = &(fed_data.data().front());
  const auto* const header = reinterpret_cast<const uint64_t*>(start_fed_data);
  const auto* const trailer = reinterpret_cast<const uint64_t*>(start_fed_data + fed_data.size());
  
  std::cout << "[HGCalUnpackerTrigger]" << " nwords (64b) = " << std::distance(header, trailer) << "\n"<< std::endl;

  HGCalTriggerFedConfig fedConfig = config.feds[fedId];
  const uint64_t* ptr = header;
  char num[10], word64[20], word32m[20], word32l[20];
  for (unsigned iword = 0; ptr < trailer; ++iword) {
    uint64_t tword = *ptr;
    uint32_t tword32m = ((tword>>32) & 0xffffffff);
    uint32_t tword32l = tword & 0xffffffff;
    sprintf(num,"%03u",iword);
    sprintf(word64,"0x%016lx",tword);
    sprintf(word32m,"0x%08x",tword32m);
    sprintf(word32l,"0x%08x",tword32l);

    std::cout << "[HGCalUnpackerTrigger]"  << "HGCalUnpackerTrigger::parseFEDData::tword " << num << " " << word64  << " (" << word32m << ", " << word32l << ")" << std::endl;
    ++ptr;
  }
  
  unsigned n64(std::distance(header, trailer));  
  const Hgcal10gLinkReceiver::TpgSubpacketHeader *tsh(reinterpret_cast<const Hgcal10gLinkReceiver::TpgSubpacketHeader*>(header+2));
  const Hgcal10gLinkReceiver::TpgSubpacketHeader *tshEnd(reinterpret_cast<const Hgcal10gLinkReceiver::TpgSubpacketHeader*>(header+n64-2-2));
  
  if(!tsh->validPattern()) {
    tsh->print();
    return false;
  }

  tsh=tsh->nextSubpacketHeader();
  int noffecafe = 0;
  bool done(false);
  uint32_t econTOffset = 0; ///THIS DEPENDS ON module 
  uint32_t denseIndexOffset = 0 ;
  uint32_t TdaqIdx = 0;  
  while(tsh<=tshEnd && !done && TdaqIdx < 3 ) {
    if(!tsh->validPattern()) {
      done=true;	
    } else {
	            //tsh->print();

	std::cout << "tdaq idx " << TdaqIdx  << std::endl;
	tsh->print();	  
	unsigned emp_chan(tsh->channelId()/2);
	if (TdaqIdx >= fedConfig.tdaqs.size()) {
	  std::cout << "aqui"<< std::endl;
	  edm::LogWarning("HGCalUnpackerTrigger")
	      << "TDAQ index out of range for FED " << fedId << ": idx=" << TdaqIdx
	      << ", size=" << fedConfig.tdaqs.size() << ". Skipping remaining subpackets.";
	  done = true;
	  break;
	}

	HGCalTDAQConfig tdaqConfig = fedConfig.tdaqs[TdaqIdx];
	uint32_t isValidTdaq;
	isValidTdaq = tdaqConfig.econts.size();
        std::cout << "tdaqsize" << isValidTdaq<< std::endl;
	if (isValidTdaq != 0){
	 
	  uint32_t nEconTs = isValidTdaq;
	  for(unsigned bx(0);bx<tsh->numberOfBxs();bx++) {
	    const uint64_t *el64packed((const uint64_t*)(tsh+1+bx*tsh->numberOfWordsPerBx()));
	    const uint32_t econTLocation = static_cast<uint32_t>(el64packed - header);
	    uint32_t *elinks = new uint32_t[tsh->numberOfWordsPerBx()];
	    for(unsigned j(0);j<tsh->numberOfWordsPerBx();j++) {
	      elinks[2*j] = el64packed[j] & 0xffffffff;
	      elinks[2*j+1] = (el64packed[j]>>32) & 0xffffffff;
	      sprintf(word64,"0x%016lx",el64packed[j]);
	      LogDebug("[HGCalUnpackerTrigger]")  << "Word " << std::setw(6) << j << " = 0x"
						  << std::hex << std::setfill('0')
						  << std::setw(16) << word64
						  << std::dec << std::setfill(' ')
						  << std::endl;	      
	      
	    }
	    for(unsigned iel(0);iel<8;iel++) {
	      sprintf(word32m,"0x%08x",elinks[iel]);
	      LogDebug("[HGCalUnpackerTrigger]")  << "\t elink " << std::setw(3) << iel << " = 0x"
						  << std::hex << std::setfill('0')
						  << std::setw(8) << word32m
						  << std::dec << std::setfill(' ')
						  << std::endl;	      
	    }


	      uint32_t nprevTxs = 0 ;
	      for(unsigned iecon(0) ; iecon < nEconTs ; iecon++) {
	        const auto& econt_conf = tdaqConfig.econts[iecon];
		const int neTx = econt_conf.eportTxNumen;
		std::cout << "neTx " << neTx << std::endl;
		uint32_t *el = new uint32_t[neTx];
		TPGFEConfiguration::ConfigEconT cfgecont;
		cfgecont.setNElinks(uint32_t(neTx));
		const int select = econt_conf.select;
		std::cout << "select " << select << std::endl;

		cfgecont.setSelect(select);

		for(int iel=0;iel<neTx;iel++) el[iel] = elinks[nprevTxs + iel];
		
                uint32_t econTId = iecon + econTOffset; //unique per fedId
		uint32_t econtDenseIdx = moduleIndexer.getIndexForModule(fedId, econTId);
		std::cout <<  "ECONT dense "<< econtDenseIdx << " econtid " << econTId << std::endl;
		if (bx == 0) {
		  econtPacketInfo.view()[econtDenseIdx].exception() = 0;
		  econtPacketInfo.view()[econtDenseIdx].location() = econTLocation;
		  econtPacketInfo.view()[econtDenseIdx].payloadLength() = static_cast<uint16_t>(neTx);
		}


		TPGFEDataformat::TcRawDataPacket rdp;
                try {
                    TPGStage1Emulation::Stage1IO::convertElinksToTcRawData(cfgecont.getOutType(), cfgecont.getNofTCs(), el, rdp);
                }
                catch (cms::Exception &e) {
                 edm::LogWarning("Stage1IORecoverable")
                 << "Skipping ECON-T " << iecon
                 << " (neTx=" << neTx << ")\n"
                 << e.what();
                 if (bx == 0) {
                    econtPacketInfo.view()[econtDenseIdx].exception() = 1;
                    econtPacketInfo.view()[econtDenseIdx].location() = econTLocation;
                    econtPacketInfo.view()[econtDenseIdx].payloadLength() = static_cast<uint16_t>(neTx);
                  }
 
		 continue;
                }

		//rdp.print();
		std::cout <<  "TCs "<< cfgecont.getNofTCs() <<  " out "<< cfgecont.getOutType() << " econTId " << iecon << " offset "  << econTOffset << " nElinks "<< cfgecont.getNElinks() << " Select " << cfgecont.getSelect()  << std::endl;

		delete [] el;
		
		uint32_t totE = 0;
		for(const auto& itc: rdp.getTcData()) totE += itc.decodedE(rdp.type());

		//// How much of below will be **CONFIGURE** ed
		
		for(unsigned itc(0) ; itc < rdp.size() ; itc++){

		  //uint32_t tcidx = uint32_t(rdp.getTc(itc).address()); 
		  uint32_t tcidx = itc;
		  // uint32_t denseIdx = tcidx + fedReadoutSequence.TCOffsets_.at(econTId) ; //same as following function call
		  uint32_t denseIdx = moduleIndexer.getIndexForModuleData(fedId, econTId, tcidx) ;
		  
		  digisTrigger.view()[denseIdx].algo() = uint8_t(cfgecont.getOutType());
		  digisTrigger.view()[denseIdx].valid()(bx,0) = true;
		  digisTrigger.view()[denseIdx].nBxs() = uint8_t(tsh->numberOfBxs());
		  digisTrigger.view()[denseIdx].econTId() = econTId;
		  digisTrigger.view()[denseIdx].nTCs() = uint8_t(cfgecont.getNofTCs());
		  digisTrigger.view()[denseIdx].bxId()(bx,0) = uint8_t(rdp.bx());
		  digisTrigger.view()[denseIdx].TotE()(bx,0) = (rdp.type()==TPGFEDataformat::BestC)? uint32_t(TPGFEDataformat::TcRawData::Decode5E3M(rdp.moduleSum())) : totE ;
		  digisTrigger.view()[denseIdx].TCEnergy()(bx,0) = uint32_t(rdp.getTc(itc).decodedE(rdp.type()));
		  digisTrigger.view()[denseIdx].TCAddress()(bx,0) = uint8_t(rdp.getTc(itc).address());

		  LogDebug("[HGCalUnpackerTrigger]")  << "HGCalUnpackerTrigger::parseFEDData fedId : " << fedId
			     << ", iecon " << iecon
			     << ", econTId " << econTId
			     << ", tcidx: " << tcidx
			     << ", denseIdx: " << denseIdx
			     << ", getDenseTCIndex00: " << moduleIndexer.getDenseTCIndex(fedId, econTId, 0, tcidx) 
			     << ", getDenseTCIndex01: " << moduleIndexer.getDenseTCIndex(fedId, econTId+1, 1, tcidx) 
			     << ", getDenseTCIndex02: " << moduleIndexer.getDenseTCIndex(fedId, econTId+2, 2, tcidx) 
			     << ", getIndexForModuleData00: " << moduleIndexer.getIndexForModuleData(fedId, econTId, tcidx) 
			     << ", getIndexForModuleData01: " << moduleIndexer.getIndexForModuleData(fedId, econTId+1, tcidx) 
			     << ", getIndexForModuleData02: " << moduleIndexer.getIndexForModuleData(fedId, econTId+2, tcidx) 
			     << std::endl;
		  LogDebug("[HGCalUnpackerTrigger]")  << "HGCalUnpackerTrigger::parseFEDData "
			     << " algo = " << uint16_t(digisTrigger.view()[denseIdx].algo())
			     << ", valid = " << uint16_t(digisTrigger.view()[denseIdx].valid()(bx,0))
			     << ", nBxs = " << uint16_t(digisTrigger.view()[denseIdx].nBxs())
			     << ", nTCs = " << uint16_t(digisTrigger.view()[denseIdx].nTCs())
			     << ", ieconTId = " << uint32_t(digisTrigger.view()[denseIdx].econTId())
			     << std::endl;
		  LogDebug("[HGCalUnpackerTrigger]")  << "HGCalUnpackerTrigger::parseFEDData ibx : " << bx
			     << ", bxID : " << uint16_t(digisTrigger.view()[denseIdx].bxId()(bx,0))
			     << ", MS/totE : " << uint32_t(digisTrigger.view()[denseIdx].TotE()(bx,0))
			     << std::endl;
		  LogDebug("[HGCalUnpackerTrigger]")  << "HGCalUnpackerTrigger::parseFEDData itc : " << itc
			     << ", Address: " << uint16_t(digisTrigger.view()[denseIdx].TCAddress()(bx,0))
			     << ", Unpacked Energy: " << uint32_t(digisTrigger.view()[denseIdx].TCEnergy()(bx,0))
			     << std::endl;
		 //denseIdx++;
		}	      
		//denseIndexOffset += rdp.size();
		nprevTxs += neTx;
	      }//iecon loop

	      delete []elinks;
	  }
	  econTOffset += nEconTs;
	}//list of valid emp channel
      TdaqIdx++;
      tsh=tsh->nextSubpacketHeader();
    }
    noffecafe++;
  }
  
  return true;
}

bool HGCalUnpackerTrigger::parseTDAQBlock(){
  return true;
}
