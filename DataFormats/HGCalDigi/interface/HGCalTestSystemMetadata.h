#ifndef DataFormats_HGCalDigis_HGCalTestSystemMetaData_h
#define DataFormats_HGCalDigis_HGCalTestSystemMetaData_h

class HGCalTestSystemMetaData
{
public:
  
  HGCalTestSystemMetaData(int trigType, int trigSubType, int trigTime,int trigWidth)
    : trigType_(trigType), trigSubType_(trigSubType), trigTime_(trigTime), trigWidth_(trigWidth), injgain_(0), injcalib_(0) { }

  HGCalTestSystemMetaData() : HGCalTestSystemMetaData(0,0,0,0) { }

  ~HGCalTestSystemMetaData() {}

  uint32_t trigType_,trigSubType_;
  uint32_t trigTime_;
  uint32_t trigWidth_;
  char injgain_;
  uint32_t injcalib_;
};

#endif
