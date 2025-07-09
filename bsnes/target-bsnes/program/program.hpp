struct Program : Lock, Emulator::Platform {
  Application::Namespace tr{"Program"};

  //program.cpp
  auto create() -> void;
  auto main() -> void;
  auto quit() -> void;

  //platform.cpp
  auto open(uint id, string name, vfs::file::mode mode, bool required) -> shared_pointer<vfs::file> override;
  auto load(uint id, string name, string type, vector<string> options = {}) -> Emulator::Platform::Load override;
  auto videoFrame(const uint16* data, uint pitch, uint width, uint height, uint scale) -> void override;
  auto audioFrame(const double* samples, uint channels) -> void override;
  auto inputPoll(uint port, uint device, uint input) -> int16 override;
  auto inputRumble(uint port, uint device, uint input, bool enable) -> void override;

  //game.cpp
  auto load() -> void;
  auto loadFile(string location) -> vector<uint8_t>;
  auto loadSuperFamicom(string location) -> bool;
  auto loadGameBoy(string location) -> bool;
  auto loadBSMemory(string location) -> bool;
  auto loadSufamiTurboA(string location) -> bool;
  auto loadSufamiTurboB(string location) -> bool;
  auto save() -> void;
  auto reset() -> void;
  auto power() -> void;
  auto unload() -> void;
  auto verified() const -> bool;

  //game-pak.cpp
  auto openPakSuperFamicom(string name, vfs::file::mode mode) -> shared_pointer<vfs::file>;
  auto openPakGameBoy(string name, vfs::file::mode mode) -> shared_pointer<vfs::file>;
  auto openPakBSMemory(string name, vfs::file::mode mode) -> shared_pointer<vfs::file>;
  auto openPakSufamiTurboA(string name, vfs::file::mode mode) -> shared_pointer<vfs::file>;
  auto openPakSufamiTurboB(string name, vfs::file::mode mode) -> shared_pointer<vfs::file>;

  //game-rom.cpp
  auto openRomSuperFamicom(string name, vfs::file::mode mode) -> shared_pointer<vfs::file>;
  auto openRomGameBoy(string name, vfs::file::mode mode) -> shared_pointer<vfs::file>;
  auto openRomBSMemory(string name, vfs::file::mode mode) -> shared_pointer<vfs::file>;
  auto openRomSufamiTurboA(string name, vfs::file::mode mode) -> shared_pointer<vfs::file>;
  auto openRomSufamiTurboB(string name, vfs::file::mode mode) -> shared_pointer<vfs::file>;

  //paths.cpp
  auto path(string type, string location, string extension = "") -> string;
  auto gamePath() -> string;
  auto cheatPath() -> string;
  auto statePath() -> string;
  auto screenshotPath() -> string;

  //states.cpp
  struct State {
    string name;
    uint64_t date;
    static const uint Signature;
  };
  auto availableStates(string type) -> vector<State>;
  auto hasState(string filename) -> bool;
  auto loadStateData(string filename) -> vector<uint8_t>;
  auto loadState(string filename) -> bool;
  auto saveState(string filename) -> bool;
  auto saveUndoState() -> bool;
  auto saveRedoState() -> bool;
  auto removeState(string filename) -> bool;
  auto renameState(string from, string to) -> bool;

  //movies.cpp
  struct Movie {
    enum Mode : uint { Inactive, Playing, Recording } mode = Mode::Inactive;
    serializer state;
    vector<int16> input;
  } movie;
  auto movieMode(Movie::Mode) -> void;
  auto moviePlay() -> void;
  auto movieRecord(bool fromBeginning) -> void;
  auto movieStop() -> void;

  //rewind.cpp
  struct Rewind {
    enum Mode : uint { Playing, Rewinding } mode = Mode::Playing;
    vector<serializer> history;
    uint length = 0;
    uint frequency = 0;
    uint counter = 0;  //in frames
  } rewind;
  auto rewindMode(Rewind::Mode) -> void;
  auto rewindReset() -> void;
  auto rewindRun() -> void;

  // netplay.cpp
  struct Netplay {
    enum Mode : uint { Inactive, Running } mode = Mode::Inactive;
    // network poller
    struct Poller {
      nall::thread pollThread;
      GekkoSession* session = nullptr;
      std::atomic<bool> running = false;
      std::mutex session_mutex;
      
      ~Poller() { if (running) stop(); }
      auto init(GekkoSession* session) -> void { this->session = session; }
      auto start() -> void {
          running = true;
          pollThread = nall::thread::create([](uintptr p) { ((Poller*)p)->run(p); }, (uintptr)this);
      }
      auto stop() -> void { running = false; pollThread.join(); }
      
      template<typename Func>
      auto with_session(Func&& func) -> decltype(func(session)) {
          std::lock_guard<std::mutex> lock(session_mutex);
          return func(session);
      }
      
    private:
      auto run(uintptr param) -> void {
          while(running) {
              {
                std::lock_guard<std::mutex> lock(session_mutex);
                gekko_network_poll(session);
              }
              std::this_thread::sleep_for(std::chrono::milliseconds(1));
          }
      }
    } poller;
    // rollback savestate  
    struct SaveState {
      ~SaveState() { clear(); }
      auto data() -> const uint8_t* { return _ptr; }
      auto set(const uint8_t* data, uint size) -> void { 
          if(size == 0) return;
          if(_capacity < size) {
            if(_ptr) delete _ptr;
            _ptr = new uint8_t[size];
            memcpy(_ptr, data, size);
            _capacity = size;
            _size = size;
            return;
          }
          memcpy(_ptr, data, size);
          _size = size;
      }
      auto size() -> uint { return _size; }
      auto clear() -> void {
        if(_ptr) delete _ptr;
        _size = 0;
        _capacity = 0;
        _ptr = nullptr;
      }
      private:
        uint _capacity = 0, _size = 0;
        uint8_t* _ptr = nullptr;
    };
    // netplay peer
    struct Peer {
      uint8 id = 0;
      string nickname;
      struct connection {
        string addr;
      } conn;
    };
    struct Buttons {
      union u {
        struct btn {
          uint b      : 1; 
          uint y      : 1; 
          uint select : 1;
          uint start  : 1;
          uint up     : 1;
          uint down   : 1;
          uint left   : 1;
          uint right  : 1;
          uint a      : 1;
          uint x      : 1;
          uint l      : 1;
          uint r      : 1;
        } btn;
        int16 value;
      } u;
    };
    enum SnesButton: uint {
      Up, Down, Left, Right, B, A, Y, X, L, R, Select, Start,
    };
    vector<SaveState> states;
    vector<Buttons> inputs;
    vector<Peer> peers;
    GekkoNetworkStats stats = {};
    GekkoConfig config = {};
    GekkoSession* session = nullptr;
    uint counter = 0;
    uint stallCounter = 0;
    uint localDelay = 0;
  } netplay;
  auto netplayMode(Netplay::Mode) -> void;
  auto netplayStart(uint16 port, uint8 local, uint8 rollback, uint8 delay, string remoteAddr, vector<string>& spectators ) -> void;
  auto netplayStop() -> void;
  auto netplayRun() -> bool;
  auto netplayPollLocalInput(Netplay::Buttons& localInput) -> void;
  auto netplayGetInput(uint port, uint button) -> int16;
  auto netplayHaltFrame() -> void;

  //video.cpp
  auto updateVideoDriver(Window parent) -> void;
  auto updateVideoExclusive() -> void;
  auto updateVideoBlocking() -> void;
  auto updateVideoFlush() -> void;
  auto updateVideoMonitor() -> void;
  auto updateVideoFormat() -> void;
  auto updateVideoShader() -> void;
  auto updateVideoPalette() -> void;
  auto updateVideoEffects() -> void;
  auto toggleVideoFullScreen() -> void;
  auto toggleVideoPseudoFullScreen() -> void;

  //audio.cpp
  auto updateAudioDriver(Window parent) -> void;
  auto updateAudioExclusive() -> void;
  auto updateAudioDevice() -> void;
  auto updateAudioBlocking() -> void;
  auto updateAudioDynamic() -> void;
  auto updateAudioFrequency() -> void;
  auto updateAudioLatency() -> void;
  auto updateAudioEffects() -> void;

  //input.cpp
  auto updateInputDriver(Window parent) -> void;

  //utility.cpp
  auto openGame(BrowserDialog& dialog) -> string;
  auto openFile(BrowserDialog& dialog) -> string;
  auto saveFile(BrowserDialog& dialog) -> string;
  auto selectPath() -> string;
  auto showMessage(string text) -> void;
  auto showFrameRate(string text) -> void;
  auto updateStatus() -> void;
  auto captureScreenshot() -> bool;
  auto inactive() -> bool;
  auto focused() -> bool;

  //patch.cpp
  auto appliedPatch() const -> bool;
  auto applyPatchIPS(vector<uint8_t>& data, string location) -> bool;
  auto applyPatchBPS(vector<uint8_t>& data, string location) -> bool;

  //hacks.cpp
  auto hackCompatibility() -> void;
  auto hackPatchMemory(vector<uint8_t>& data) -> void;

  //filter.cpp
  auto filterSelect(uint& width, uint& height, uint scale) -> Filter::Render;

  //viewport.cpp
  auto viewportSize(uint& width, uint& height, uint scale) -> void;
  auto viewportRefresh() -> void;

public:
  struct Game {
    explicit operator bool() const { return (bool)location; }

    string option;
    string location;
    string manifest;
    Markup::Node document;
    boolean patched;
    boolean verified;
  };

  struct SuperFamicom : Game {
    string title;
    string region;
    vector<uint8_t> program;
    vector<uint8_t> data;
    vector<uint8_t> expansion;
    vector<uint8_t> firmware;
  } superFamicom;

  struct GameBoy : Game {
    vector<uint8_t> program;
  } gameBoy;

  struct BSMemory : Game {
    vector<uint8_t> program;
  } bsMemory;

  struct SufamiTurbo : Game {
    vector<uint8_t> program;
  } sufamiTurboA, sufamiTurboB;

  vector<string> gameQueue;

  uint32_t palette[32768];
  uint32_t paletteDimmed[32768];

  struct Screenshot {
    const uint16* data = nullptr;
    uint pitch  = 0;
    uint width  = 0;
    uint height = 0;
    uint scale  = 0;
  } screenshot;

  bool frameAdvanceLock = false;

  uint64 autoSaveTime;

  uint64 statusTime;
  string statusMessage;
  string statusFrameRate;

  bool startFullScreen = false;

  struct Mute { enum : uint {
    Always      = 1 << 1,
    Unfocused   = 1 << 2,
    FastForward = 1 << 3,
    Rewind      = 1 << 4,
  };};
  uint mute = 0;

  bool fastForwarding = false;
  bool rewinding = false;
};

extern Program program;
