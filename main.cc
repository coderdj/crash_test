#include <iostream>
#include <unistd.h>
#include <vector>
#include "CAENVMElib.h"
#include "CAENVMEtypes.h"

using namespace std;

#define gLOOPS        1000000
#define gREGISTERS    100
#define gREADS        10000
#define gDAC          100
#define gBOARD_VME    0x800D0000
#define gBOARDS       8

#define CBV1724_BoardInfoReg              0x8140
#define CBV1724_BltEvNumReg               0xEF1C
#define CBV1724_DACReg                    0x1098
#define CBV1724_ChannelConfReg            0x8000
#define CBV1724_AcquisitionControlReg     0x8100
#define CBV1724_TriggerSourceReg          0x810C
#define CBV1724_DPPReg                    0x8080
#define CBV1724_BuffOrg                   0x800C
#define CBV1724_CustomSize                0x8020
#define CBV1724_SoftwareTriggerReg        0x8108
#define CBV1724_FrontPanelIOReg           0x811C
#define CBV1724_ChannelMaskReg            0x8120
#define CBV1724_BoardResetReg             0xEF24
#define CBV1724_VMEControlReg             0xEF00

/*
 * This is a simple test function that repeatedly initializes,
 * reads and writes registers, reads and writes data, and closes
 * a single CAEN V1724. You should have the board hooked up to 
 * a PCI card with a single optical link going directly to the board.
 *
 * Requires CAENVMElib. The A2818 or A3818 driver must be installed.
*/
int WriteRegister(u_int32_t reg, u_int32_t val, int handle){
  int ret = CAENVME_WriteCycle(handle,gBOARD_VME+reg,
			       &val,cvA32_U_DATA,cvD32);    
  if(ret!=cvSuccess)
    cout<<"Failed to write register "<<hex<<reg<<" with value "<<val<<
      ", returned value: "<<dec<<ret<<endl;
  return ret;
}

int ReadRegister(u_int32_t reg, u_int32_t &val, int handle){
  int ret = CAENVME_ReadCycle(handle,gBOARD_VME+reg,
			      &val,cvA32_U_DATA,cvD32);
  if(ret!=cvSuccess)
    cout<<"Failed to read register "<<hex<<reg<<" with return value: "<<
      dec<<ret<<endl;
  return ret;
}

int LoopDAC(int handle){
  // Set the DAC to some values over and over (simulate baseline fit procedure)
  for(int dac_loops=0; dac_loops<gDAC; dac_loops++){

    // Set between 0x1000 and 0x1500
    int dac_setting = 0x1000;
    if(dac_loops%2==0)
      dac_setting = 0x1500;

    // Loop over channels
    for( int channel=0; channel<8; channel++){
      u_int32_t data = 0x0;
      if(ReadRegister(0x1088+(0x100*channel), data, handle)!=cvSuccess){
	cout<<"Error reading channel status register for channel: "
	    <<channel<<" on iteration "<<dac_loops<<endl;
	return -1;
      }
      if(WriteRegister(0x1098+(0x100*channel), dac_setting, handle)!=cvSuccess){
	cout<<"Writing DAC register failed for channel: "<<channel<<endl;
	return -1;
      }
      
      // Add timeout to make sure setting takes effect. We will give it one second.
      for(int check_timer = 0; check_timer<1000; check_timer++){
	if(check_timer == 999){
	  cout<<"Timout reached for DAC register set @ 1 second! Quitting."<<endl;
	  return -1;
	}
	if(ReadRegister(0x1088+(0x100*channel), data, handle)!=cvSuccess){
	  cout<<"Error reading channel status register for channel: "
	      <<channel<<" on iteration "<<dac_loops<<" *after* setting."<<endl;
	  return -1;
	}
	if(data&0x4)
	  usleep(1000);
	else
	  break;
      }// end setting confirmation
    } // end channels
  } // end DAC loop
  return 0;
}

int main(){
  
  cout<<"Starting program"<<endl;
  
 
  for( int loop_counter=0; loop_counter<gLOOPS; loop_counter++){
    
    cout<<"Entering loop "<<loop_counter<<endl;
    vector<int> handles;
    
    // Initialize the boards
    for(unsigned int board=0; board<gBOARDS; board++){
      int handle = -1, cerror=-1;;
      if((cerror=CAENVME_Init(cvV2718,0,board,&handle)) != cvSuccess){
	cout<<"Failed to initialize board "<<handle<<" with error "<<cerror<<endl;
	return -1;
      }
      handles.push_back(handle);
    }
    if(handles.size()==0) {
      cout<<"No boards configured."<<endl;
      return -1;
    }

    cout<<"("<<loop_counter<<") Entering register cycle for "<<
      gREGISTERS<<" loops"<<endl;
    for( int cycle_counter=0; cycle_counter<gREGISTERS; cycle_counter++){
      
      // For each cycle we will read and write a bunch of registers
      // Then we will self-trigger and read out some data.
      
      for(unsigned int board=0; board<handles.size(); board++){
	int handle = handles[board];
	int r = 0;
	r+= WriteRegister(CBV1724_BoardResetReg, 0x1, handle); 
	r+= WriteRegister(CBV1724_BltEvNumReg, 0x1, handle);
	r+= WriteRegister(CBV1724_VMEControlReg, 0x10, handle);
	r+= WriteRegister(CBV1724_ChannelMaskReg, 0xFF, handle);
	r+= WriteRegister(CBV1724_ChannelConfReg,0x310, handle);
	r+= WriteRegister(CBV1724_DPPReg,0x1310000, handle);
	r+= WriteRegister(CBV1724_BuffOrg,0xA, handle);
	r+= WriteRegister(CBV1724_CustomSize,0xC8, handle);
	r+= WriteRegister(CBV1724_FrontPanelIOReg, 0x840, handle);      
	r+= WriteRegister(CBV1724_AcquisitionControlReg,0x0, handle);
	r+= WriteRegister(CBV1724_TriggerSourceReg,0x80000000, handle);
	if(r!=0){
	  cout<<"Crash in registers for board: "<<handle<<endl;
	  return -1;
	}

	// Do the DAC setting
	cout<<"("<<loop_counter<<") Entering DAC loop"<<endl;
	if(LoopDAC(handle)!=0){
	  cout<<"Crash in DAC for board: "<<handle<<endl;
	  return -1;
	}
	cout<<"("<<loop_counter<<") Done with DAC loop"<<endl;
      }
      
      // Read the boards
      cout<<"("<<loop_counter<<") Entering read loop"<<endl;
      for( int read_counter=0; read_counter<gREADS; read_counter++){

	for(unsigned int board = 0; board<handles.size(); board++){
	  
	  int handle = handles[board];
	  // Software trigger
	  int r=0;
	  r+= WriteRegister(CBV1724_AcquisitionControlReg, 0x4, handle);
	  r+= WriteRegister(CBV1724_SoftwareTriggerReg, 0x1, handle);
	  r+= WriteRegister(CBV1724_AcquisitionControlReg, 0x0, handle);
	  r+= WriteRegister(CBV1724_AcquisitionControlReg, 0x0, handle);
	  if(r!=0){
	    cout<<"Crash in software triggering for board: "<<handle<<endl;
	    return -1;
	  }

	  // Read board
	  unsigned int blt_bytes=0, buff_size=10000, blt_size=524288;
	  int nb=0,ret=-5;
	  u_int32_t *buff = new u_int32_t[buff_size]; //too large is OK
	  do{
	    ret = CAENVME_FIFOBLTReadCycle(handle,gBOARD_VME,
					   ((unsigned char*)buff)+blt_bytes,
					   blt_size,cvA32_U_BLT,cvD32,&nb);
	    if((ret!=cvSuccess) && (ret!=cvBusError)){
	      cout<<"Board read error: "<<ret<<" for board "<<handle<<endl;
	      delete[] buff;
	      return -1;
	    }
	  blt_bytes+=nb;
	  if(blt_bytes>buff_size)   {
	    cout<<"Buffer size too small for read!"<<endl;
	    delete[] buff;
	    return -1;
	  }
	  }while(ret!=cvBusError);
	  
	  delete[] buff;
	}// end loop through boards
      } // end read loop
      cout<<"("<<loop_counter<<") Done with read loop"<<endl;
    } // end write register loop
    cout<<"("<<loop_counter<<") Done with register loop"<<endl;

    // Close the boards
    for(unsigned int board = 0; board<handles.size(); board++){
      int handle = handles[board];
      int cerror=0;
      if((cerror=CAENVME_End(handle)) != cvSuccess){
	cout<<"Failed to close board "<<handle<<" with error "<<cerror<<endl;
	return -1;
      }
    }
  }

  cout<<"Program finished."<<endl;
}
