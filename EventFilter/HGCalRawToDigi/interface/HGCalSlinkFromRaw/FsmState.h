#ifndef hgcal_slinkfromraw_FsmState_h
#define hgcal_slinkfromraw_FsmState_h

#include <string>

namespace hgcal_slinkfromraw {

  class FsmState {

  public:
    enum State {
      // Statics
      Shutdown,
      Initial,
      Halted,
      ConfiguredA,
      ConfiguredB,
      Running,
      Paused,
      EndOfStaticEnum,
      
      // Transients
      Initializing=EndOfStaticEnum,
      ConfiguringA,
      ConfiguringB,
      Starting,
      Pausing,
      Resuming,
      Stopping,
      HaltingB,
      HaltingA,
      Resetting,
      Ending,
      EndOfTransientEnum,

      // For file and buffer records only
      Continuing=EndOfTransientEnum,
      EndOfStateEnum
    };

    FsmState() {
    }
    
    FsmState(State s) {
      _state=s;
    }
    
    State state() const {
      return _state;
    }

    void setState(State s) {
      _state=s;
    }

    bool staticState() const {
      //return _state<EndOfStaticEnum;
      return staticState(_state);
    }
    
    bool transientState() const {
      return transientState(_state);
    }
    
    bool allowedChange(State s) {
      return allowedChange(_state,s);
    }
    
    const std::string& stateName() const {
      return stateName(_state);
    }

    // Utilities
    static bool staticState(State s) {
      return s<EndOfStaticEnum;
    }

    static bool transientState(State s) {
      return s>=EndOfStaticEnum && s<EndOfTransientEnum;
    }
    
    static bool allowedChange(State s1, State s2) {

      // Resetting is an exception
      if(staticState(s1) && s2==Resetting) return true;
	
      return	      
	// Static to Transient
	(s1==Initial     && s2==Ending      ) ||
	(s1==Initial     && s2==Initializing) ||
	(s1==Halted      && s2==ConfiguringA) ||
	(s1==Halted      && s2==Resetting   ) ||
	(s1==ConfiguredA && s2==ConfiguringB) ||
	(s1==ConfiguredA && s2==HaltingA    ) ||
	(s1==ConfiguredB && s2==Starting    ) ||
	(s1==ConfiguredB && s2==HaltingB    ) ||
	(s1==Running     && s2==Pausing     ) ||	      
	(s1==Running     && s2==Stopping    ) ||
	(s1==Paused      && s2==Resuming    ) ||
      
	// Transient to Static
	(s1==Initializing && s2==Halted     ) ||
	(s1==ConfiguringA && s2==ConfiguredA) ||
	(s1==ConfiguringB && s2==ConfiguredB) ||
	(s1==Starting     && s2==Running    ) ||
	(s1==Pausing      && s2==Paused     ) ||
	(s1==Resuming     && s2==Running    ) ||
	(s1==Stopping     && s2==ConfiguredB) ||
	(s1==HaltingB     && s2==ConfiguredA) ||
	(s1==HaltingA     && s2==Halted     ) ||
	(s1==Resetting    && s2==Initial    ) ||
	(s1==Ending       && s2==Shutdown   );      
    }

    static State staticStateBeforeTransient(State s) {
      if(staticState(s)) return EndOfStateEnum;

      if(s==Initializing) return Initial;
      if(s==ConfiguringA) return Halted;
      if(s==ConfiguringB) return ConfiguredA;
      if(s==Starting    ) return ConfiguredB;
      if(s==Pausing     ) return Running;
      if(s==Resuming    ) return Paused;
      if(s==Stopping    ) return Running;
      if(s==HaltingB    ) return ConfiguredB;
      if(s==HaltingA    ) return ConfiguredA;
      if(s==Resetting   ) return Halted;
      if(s==Ending      ) return Initial;

      return EndOfStateEnum;
    }
        

    static State staticStateAfterTransient(State s) {
      if(staticState(s)) return EndOfStateEnum;

      if(s==Initializing) return Halted;
      if(s==ConfiguringA) return ConfiguredA;
      if(s==ConfiguringB) return ConfiguredB;
      if(s==Starting    ) return Running;
      if(s==Pausing     ) return Paused;
      if(s==Resuming    ) return Running;
      if(s==Stopping    ) return ConfiguredB;
      if(s==HaltingB    ) return ConfiguredA;
      if(s==HaltingA    ) return Halted;
      if(s==Resetting   ) return Initial;
      if(s==Ending      ) return Shutdown;

      return EndOfStateEnum;
    }
        
    // Conversion of enum values to human-readable strings    
    static const std::string& stateName(State s) {
      if(s<EndOfStateEnum) return _stateName[s];
      return _unknown;
    }
      
  private:
    State _state;
  
    static const std::string _unknown;
    static const std::string _stateName[EndOfStateEnum];
  };
 
  const std::string FsmState::_unknown="Unknown     ";
    
  const std::string FsmState::_stateName[EndOfStateEnum]={
    // Statics
    "Shutdown    ",
    "Initial     ",
    "Halted      ",
    "ConfiguredA ",
    "ConfiguredB ",
    "Running     ",
    "Paused      ",
      
    // Transients
    "Initializing",
    "ConfiguringA",
    "ConfiguringB",
    "Starting    ",
    "Pausing     ",
    "Resuming    ",
    "Stopping    ",
    "HaltingB    ",
    "HaltingA    ",
    "Resetting   ",
    "Ending      ",
    
    // For file and buffer records only    
    "Continuing  "
  };

}
#endif
