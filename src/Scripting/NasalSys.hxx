#ifndef __NASALSYS_HXX
#define __NASALSYS_HXX

#include <simgear/math/SGMath.hxx> // keep before any cppbind include to enable
                                   // SGVec2<T> conversion.
#include <simgear/misc/sg_dir.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/nasal/cppbind/NasalHash.hxx>
#include <simgear/nasal/nasal.h>
#include <simgear/props/props.hxx>
#include <simgear/structure/subsystem_mgr.hxx>
#include <simgear/threads/SGQueue.hxx>

// Required only for MSVC
#ifdef _MSC_VER
#   include <Scripting/NasalModelData.hxx>
#endif

#include <map>
#include <memory>

class FGNasalScript;
class FGNasalListener;
class SGCondition;
class FGNasalModelData;
class NasalCommand;
class FGNasalModuleListener;
struct NasalTimer;  ///< timer created by settimer
class TimerObj;     ///< persistent timer created by maketimer

namespace simgear { class BufferedLogCallback; }

SGPropertyNode* ghostToPropNode(naRef ref);

class FGNasalSys : public SGSubsystem
{
public:
    FGNasalSys();
    virtual ~FGNasalSys();

    // Subsystem API.
    void init() override;
    void shutdown() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "nasal"; }

    // Loads a nasal script from an external file and inserts it as a
    // global module of the specified name.
    bool loadModule(SGPath file, const char* moduleName);

    // Simple hook to run arbitrary source code.  Returns a bool to
    // indicate successful execution.  Does *not* return any Nasal
    // values, because handling garbage-collected objects from C space
    // is deep voodoo and violates the "simple hook" idea.
    bool parseAndRun(const std::string& source);

    bool parseAndRunWithOutput(const std::string& source,
                     std::string& output,
                     std::string& errors);

    // Implementation of the settimer extension function
    void setTimer(naContext c, int argc, naRef* args);

    // Implementation of the setlistener extension function
    naRef setListener(naContext c, int argc, naRef* args);
    naRef removeListener(naContext c, int argc, naRef* args);

    // Returns a ghost wrapper for the current _cmdArg
    naRef cmdArgGhost();

    void setCmdArg(SGPropertyNode* aNode);

    /**
     * create Nasal props.Node for an SGPropertyNode*
     * This is the actual ghost, wrapped in a Nasal sugar class.
     */
    naRef wrappedPropsNode(SGPropertyNode* aProps);

    // Callbacks for command and timer bindings
    virtual bool handleCommand( const char* moduleName,
                                const char* fileName,
                                const char* src,
                                const SGPropertyNode* arg = 0,
                                SGPropertyNode* root = 0);
    virtual bool handleCommand(const SGPropertyNode* arg, SGPropertyNode *root);

    bool createModule(const char* moduleName, const char* fileName,
                      const char* src, int len, const SGPropertyNode* cmdarg=0,
                      int argc=0, naRef*args=0);

    void deleteModule(const char* moduleName);

    naRef getModule(const std::string& moduleName) const;
    naRef getModule(const char* moduleName);

    bool addCommand(naRef func, const std::string& name);
    bool removeCommand(const std::string& name);

    /**
     * Set member of specified hash to given value
     */
    void hashset(naRef hash, const char* key, naRef val);

    /**
     * Set member of globals hash to given value
     */
    void globalsSet(const char* key, naRef val);

    naRef call(naRef code, int argc, naRef* args, naRef locals);
    naRef callWithContext(naContext ctx, naRef code, int argc, naRef* args, naRef locals);

    naRef callMethod(naRef code, naRef self, int argc, naRef* args, naRef locals);
    naRef callMethodWithContext(naContext ctx, naRef code, naRef self, int argc, naRef* args, naRef locals);

    naRef propNodeGhost(SGPropertyNode* handle);

    void registerToLoad(FGNasalModelData* data);
    void registerToUnload(FGNasalModelData* data);

    // can't call this 'globals' due to naming clash
    naRef nasalGlobals() const
    { return _globals; }

    nasal::Hash getGlobals() const
    { return nasal::Hash(_globals, _context); }

    // This mechanism is here to allow naRefs to be passed to
    // locations "outside" the interpreter.  Normally, such a
    // reference would be garbage collected unexpectedly.  By passing
    // it to gcSave and getting a key/handle, it can be cached in a
    // globals.__gcsave hash.  Be sure to release it with gcRelease
    // when done.
    int gcSave(naRef r);
    void gcRelease(int key);

    /**
     * Check if IOrules correctly work to limit access from Nasal scripts to the
     * file system.
     *
     * @note Just a simple test is performed to check if access to a path is
     *       possible which should never be possible (The actual path refers to
     *       a file/folder named 'do-not-access' in the file system root).
     *
     * @see http://wiki.flightgear.org/IOrules
     *
     * @return Whether the check was successful.
     */
    bool checkIOrules();

    /// retrive the associated log object, for displaying log
    /// output somewhere (a UI, presumably)
    simgear::BufferedLogCallback* log() const
    { return _log.get(); }

    string_list getAndClearErrorList();

    /**
     @brief Convert the value of an SGPropertyNode to its Nasal representation. Used by
     props.Node.getValue internally, but exposed here for other use cases which don't want to create
     a props.Node wrapper each time.
     */
    static naRef getPropertyValue(naContext c, SGPropertyNode* node);

    bool reloadModuleFromFile(const std::string& moduleName);

private:
    void initLogLevelConstants();

    void loadPropertyScripts();
    void loadPropertyScripts(SGPropertyNode* n);
    void loadScriptDirectory(simgear::Dir nasalDir, SGPropertyNode* loadorder,
                             bool excludeUnspecifiedInLoadOrder);
    void addModule(std::string moduleName, simgear::PathList scripts);
    static void logError(naContext);
    naRef parse(naContext ctx, const char* filename, const char* buf, int len,
               std::string& errors);
    naRef genPropsModule();


private:
    //friend class FGNasalScript;
    friend class FGNasalListener;
    friend class FGNasalModuleListener;
    SGLockedQueue<SGSharedPtr<FGNasalModelData> > _loadList;
    SGLockedQueue<SGSharedPtr<FGNasalModelData> > _unloadList;
    // Delay removing items of the _loadList to ensure the are already attached
    // to the scene graph (eg. enables to retrieve world position in load
    // callback).
    bool _delay_load;

    // Listener
    std::map<int, FGNasalListener *> _listener;
    std::vector<FGNasalListener *> _dead_listener;

    std::vector<FGNasalModuleListener*> _moduleListeners;

    static int _listenerId;

    bool _inited;
    naContext _context;
    naRef _globals,
          _string;

    SGPropertyNode_ptr _cmdArg;

    std::unique_ptr<simgear::BufferedLogCallback> _log;

    typedef std::map<std::string, NasalCommand*> NasalCommandDict;
    NasalCommandDict _commands;

    naRef _wrappedNodeFunc;

    // track NasalTimer instances (created via settimer() call) -
    // this allows us to clean these up on shutdown
    std::vector<NasalTimer*> _nasalTimers;

    // NasalTimer is a friend to invoke handleTimer and do the actual
    // dispatch of the settimer-d callback
    friend NasalTimer;

    void handleTimer(NasalTimer* t);

    // track persistent timers. These are owned from the Nasal side, so we
    // only track a non-owning reference here.
    std::vector<TimerObj*> _persistentTimers;

    friend TimerObj;

    void addPersistentTimer(TimerObj* pto);
    void removePersistentTimer(TimerObj* obj);

    static void logNasalStack(naContext context, string_list& stack);
};

#if 0
class FGNasalScript
{
public:
    ~FGNasalScript() { _nas->gcRelease(_gcKey); }

    bool call() {
        naRef n = naNil();
        naCall(_nas->_context, _code, 0, &n, naNil(), naNil());
        return naGetError(_nas->_context) == 0;
    }

    FGNasalSys* sys() const { return _nas; }

private:
    friend class FGNasalSys;
    naRef _code;
    int _gcKey;
    FGNasalSys* _nas;
};
#endif

#endif // __NASALSYS_HXX
