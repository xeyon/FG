#include "apt_loader_wrapper.h"
#include "Navaids/NavDataCache.hxx"
#include "airport.hxx"
#include "runways.hxx"
#include "groundnetwork.hxx"
#include <Main/options.hxx>
//#include <Main/fg_props.hxx>
#include <Main/fg_init.hxx>
#include <simgear/props/props_io.hxx>
#include <simgear/structure/exception.hxx>

flightgear::NavDataCache* cache;

int initGlobals(char* sDatapath)
{
    if (!globals) {
        globals = new FGGlobals;

        globals->set_fg_home(SGPath(".\\"));
        if (sDatapath) {
            globals->set_fg_root(SGPath(sDatapath));
        } else {
            globals->set_fg_root(SGPath("E:\\FlightGearBuild\\fgdata"));
        }
    } else {
        return EXIT_SUCCESS;
    }

    cout << "Data home folder:" << globals->get_fg_home();

    // Load the configuration parameters.  (Command line options
    // override config file options.  Config file options override
    // defaults.)
    int configResult = fgInitConfig(0, nullptr, true);
    if (configResult == flightgear::FG_OPTIONS_ERROR) {
        return EXIT_FAILURE;
    } else if (configResult == flightgear::FG_OPTIONS_EXIT) {
        return EXIT_SUCCESS;
    }
}

int loadPropsXml(const char* path)
{
    SGPath fullpath = SGPath::fromLocal8Bit(path);

    try {
        readProperties(fullpath, globals->get_props());
    } catch (const sg_exception& e) {
        cout << "Error reading properties: ", e;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int savePropsXml(const char* path)
{
    SGPath autosaveFile = SGPath::fromLocal8Bit(path);

    autosaveFile.create_dir(0700);
    SG_LOG(SG_IO, SG_INFO, "Saving user settings to " << autosaveFile);
    try {
        writeProperties(autosaveFile, globals->get_props(), false, SGPropertyNode::USERARCHIVE);
    } catch (const sg_exception& e) {
        cout << "Error writing props to xml:", e;
        return EXIT_FAILURE;
    }
    SG_LOG(SG_INPUT, SG_DEBUG, "Finished Saving user settings");

    return EXIT_SUCCESS;
}

int getPropsBool(const char* name, int* val)
{
    SGPropertyNode* prop = globals->get_props()->getNode(name, false);

    if (prop == NULL)
        return 0;
    else {
        *val = prop->getBoolValue();
        return 1;
    }
}

int getPropsInt(const char* name, int* val)
{
    SGPropertyNode* prop = globals->get_props()->getNode(name, false);

    if (prop == NULL)
        return 0;
    else {
        *val = prop->getIntValue();
        return 1;
    }

}
int getPropsLong(const char* name, long long* val)
{
    SGPropertyNode* prop = globals->get_props()->getNode(name, false);

    if (prop == NULL)
        return 0;
    else {
        *val = prop->getLongValue();
        return 1;
    }
}
int getPropsFloat(const char* name, float* val)
{
    SGPropertyNode* prop = globals->get_props()->getNode(name, false);

    if (prop == NULL)
        return 0;
    else {
        *val = prop->getFloatValue();
        return 1;
    }
}
int getPropsDouble(const char* name, double* val)
{
    SGPropertyNode* prop = globals->get_props()->getNode(name, false);

    if (prop == NULL)
        return 0;
    else {
        *val = prop->getDoubleValue();
        return 1;
    }
}
int getPropsString(const char* name, const char* val)
{
    SGPropertyNode* prop = globals->get_props()->getNode(name, false);

    if (prop == NULL)
        return 0;
    else {
        val = prop->getStringValue().c_str();
        return 1;
    }
}

int getPropsBoolList(const char* name, int* val, int num)
{
    SGPropertyNode* prop = NULL;

    for (int i = 0; i < num; i++) {
        prop = globals->get_props()->getNode(name, i, false);

        if (prop == NULL)
            return i;
        else {
            *val = prop->getBoolValue();
        }
        val++;
    }
    return num;
}
int getPropsIntList(const char* name, int* val, int num)
{
    SGPropertyNode* prop = NULL;

    for (int i = 0; i < num; i++) {
        prop = globals->get_props()->getNode(name, i, false);

        if (prop == NULL)
            return i;
        else {
            *val = prop->getIntValue();
        }
        val++;
    }
    return num;
}
int getPropsLongList(const char* name, long long* val, int num)
{
    SGPropertyNode* prop = NULL;

    for (int i = 0; i < num; i++) {
        prop = globals->get_props()->getNode(name, i, false);

        if (prop == NULL)
            return i;
        else {
            *val = prop->getLongValue();
        }
        val++;
    }
    return num;
}
int getPropsFloatList(const char* name, float* val, int num)
{
    SGPropertyNode* prop = NULL;

    for (int i = 0; i < num; i++) {
        prop = globals->get_props()->getNode(name, i, false);

        if (prop == NULL)
            return i;
        else {
            *val = prop->getFloatValue();
        }
        val++;
    }
    return num;
}
int getPropsDoubleList(const char* name, double* val, int num)
{
    SGPropertyNode* prop = NULL;

    for (int i = 0; i < num; i++) {
        prop = globals->get_props()->getNode(name, i, false);

        if (prop == NULL)
            return i;
        else {
            *val = prop->getDoubleValue();
        }
        val++;
    }
    return num;
}

int setPropsBool(const char* name, int val)
{
    SGPropertyNode* prop = globals->get_props()->getNode(name, true);

    if (prop == NULL)
        return 0;
    else {
        prop->setBoolValue(val);
        prop->setAttribute(SGPropertyNode::USERARCHIVE, true);
        return 1;
    }
}
int setPropsInt(const char* name, int val)
{
    SGPropertyNode* prop = globals->get_props()->getNode(name, true);

    if (prop == NULL)
        return 0;
    else {
        prop->setIntValue(val);
        prop->setAttribute(SGPropertyNode::USERARCHIVE, true);
        return 1;
    }
}
int setPropsLong(const char* name, long long val)
{
    SGPropertyNode* prop = globals->get_props()->getNode(name, true);

    if (prop == NULL)
        return 0;
    else {
        prop->setLongValue(val);
        prop->setAttribute(SGPropertyNode::USERARCHIVE, true);
        return 1;
    }
}
int setPropsFloat(const char* name, float val)
{
    SGPropertyNode* prop = globals->get_props()->getNode(name, true);

    if (prop == NULL)
        return 0;
    else {
        prop->setFloatValue(val);
        prop->setAttribute(SGPropertyNode::USERARCHIVE, true);
        return 1;
    }
}
int setPropsDouble(const char* name, double val)
{
    SGPropertyNode* prop = globals->get_props()->getNode(name, true);

    if (prop == NULL)
        return 0;
    else {
        prop->setDoubleValue(val);
        prop->setAttribute(SGPropertyNode::USERARCHIVE, true);
        return 1;
    }
}
int setPropsString(const char* name, const char* val)
{
    SGPropertyNode* prop = globals->get_props()->getNode(name, true);

    if (prop == NULL)
        return 0;
    else {
        prop->setStringValue(val);
        prop->setAttribute(SGPropertyNode::USERARCHIVE, true);
        return 1;
    }
}
int setPropsBoolList(const char* name, int* val, int num)
{
    SGPropertyNode* prop = NULL;
        
    for (int i = 0; i < num; i++) {
        prop = globals->get_props()->getNode(name, i, true);

        if (prop == NULL)
            return i;
        else {
            prop->setBoolValue(*val);
            prop->setAttribute(SGPropertyNode::USERARCHIVE, true);
            val++;
        }
    }
    return num;
}
int setPropsIntList(const char* name, int* val, int num)
{
    SGPropertyNode* prop = NULL;

    for (int i = 0; i < num; i++) {
        prop = globals->get_props()->getNode(name, i, true);

        if (prop == NULL)
            return i;
        else {
            prop->setIntValue(*val);
            prop->setAttribute(SGPropertyNode::USERARCHIVE, true);
            val++;
        }
    }
    return num;
}
int setPropsLongList(const char* name, long long* val, int num)
{
    SGPropertyNode* prop = NULL;

    for (int i = 0; i < num; i++) {
        prop = globals->get_props()->getNode(name, i, true);

        if (prop == NULL)
            return i;
        else {
            prop->setLongValue(*val);
            prop->setAttribute(SGPropertyNode::USERARCHIVE, true);
            val++;
        }
    }
    return num;
}
int setPropsFloatList(const char* name, float* val, int num)
{
    SGPropertyNode* prop = NULL;

    for (int i = 0; i < num; i++) {
        prop = globals->get_props()->getNode(name, i, true);

        if (prop == NULL)
            return i;
        else {
            prop->setFloatValue(*val);
            prop->setAttribute(SGPropertyNode::USERARCHIVE, true);
            val++;
        }
    }
    return num;
}
int setPropsDoubleList(const char* name, double* val, int num)
{
    SGPropertyNode* prop = NULL;

    for (int i = 0; i < num; i++) {
        prop = globals->get_props()->getNode(name, i, true);

        if (prop == NULL)
            return i;
        else {
            prop->setDoubleValue(*val);
            prop->setAttribute(SGPropertyNode::USERARCHIVE, true);
            val++;
        }
    }
    return num;
}

int initNavCache(char* sDatapath)
{
    if (!globals) {
        initGlobals(sDatapath);
    }

    if (flightgear::NavDataCache::isAnotherProcessRebuilding()) {

        cout << "Another process is rebuilding NavCache. Quit!\n";
        return -1;
    }

    cache = (flightgear::NavDataCache::createInstance());

    if (cache->isRebuildRequired()) {
        auto phase = cache->rebuild();
        //cout << "please wait for the rebuild patiently...\n";
        return phase;
    }
    //cout << "NavDataCache rebuild done!\n";
    return NVCACHE_REBUILD_DONE;
}

int getRebuildProgress()
{
    auto phase = cache->rebuild();
    return (phase * 100) + cache->rebuildPhaseCompletionPercentage();
}

int getAirportGates(const char* sICAO, char* result[])
{
    if (!cache) {
        initNavCache(NULL);
    }

    const string airportId = sICAO;
    const FGAirport* apt = fgFindAirportID(airportId);
    if (apt) {
        int nRw;
        int numMatches = 0;
        auto gateList = apt->groundNetwork()->allParkings();
        nRw = gateList.size();
        //char** result = (char**)malloc(sizeof(char*) * (nRw));
        for (auto rwId : gateList) {
            result[numMatches++] = (char*)rwId->ident().c_str();
            //cout << "\tRW id: " << rwId->ident().c_str();
        }

        return numMatches;
    } else {
        free(result);
        result = NULL;
        return 0;
    }
}

int getAirportRunways(const char* sICAO, char* result[])
{
    if (!cache) {
        initNavCache(NULL);
    }

    const string airportId = sICAO;
    const FGAirport* apt = fgFindAirportID(airportId);
    if (apt) {
        int nRw;
        int numMatches = 0;
        auto rwyList = apt->getRunwaysWithoutReciprocals();     //getRunways();
        nRw = rwyList.size();
        //result = (char**)malloc(sizeof(char*) * (nRw));
        for (auto rwId : rwyList) {
            result[numMatches++] = (char *)rwId->ident().c_str();
            //cout << "\tRW id: " << rwId->ident().c_str();
        }

        return numMatches;
    } else {
        //free(result);
        result = NULL;
        return 0;
    }

}

int getAirportInfoByICAO(struct Apt_Info *resp)
{
    int rwy_cnt, gate_cnt;
    char icao[5] = "XXXX";
    memcpy(icao, &(resp->airport_info_icao), 4);
    string airportId = icao;
    const FGAirport* apt = fgFindAirportID(airportId);

    if (!apt) return 0;

    // list of gates
    auto gateList = apt->groundNetwork()->allParkings();
    gate_cnt = gateList.size();

    int i = 0;
    resp->db_info[i++] = gate_cnt * 4;  // i = 0;
    for (auto rwId : gateList) {
        strcpy_s((char *)&(resp->db_info[i++]), 4, (char*)rwId->ident().c_str());  // i = 1 to gate_cnt
    }

    // list of runways
    auto rwyList = apt->getRunwaysWithoutReciprocals();
    rwy_cnt = rwyList.size();
    resp->db_info[i++] = rwy_cnt * 4; // i = (gate_cnt +1);
    resp->db_info[i + rwy_cnt] = rwy_cnt * 4; // i = (gate_cnt + rwy_cnt+ 2);

    for (auto rwId : rwyList) {
        // i = (gate_cnt + 2) to (gate_cnt + rwy_cnt + 1)
        strcpy_s((char*)&(resp->db_info[i]), 4, (char*)rwId->ident().c_str());
        // i = (gate_cnt + rwy_cnt + 3) to (gate_cnt + 2 * rwy_cnt + 2)
        strcpy_s((char*)&(resp->db_info[i + 1 + rwy_cnt]), 4, (char*)rwId.get()->reciprocalRunway()->ident().c_str());
        i++;
    }
    resp->airport_info_gate_cnt = gate_cnt;
    resp->airport_info_rwy_cnt = rwy_cnt;

    return i;
}

int getRunwayGeo(const char* sICAO, const char* sRW[], double* fLat, double* fLon, double* fEle, double* fHdg, int* iWid)
    {
    if (!cache) {
        initNavCache(NULL);
    }
    
    const string airportId = sICAO;
    const string rwId = sRW[0];
    const FGAirport* apt = fgFindAirportID(airportId);
    FGRunwayRef runway = apt->getRunwayByIdent(rwId);
    
    if (!runway) return 0;

    fLat[0] = runway.get()->geod().getLatitudeDeg();
    fLon[0] = runway.get()->geod().getLongitudeDeg();
    fEle[0] = apt->elevationM();
    fHdg[0] = runway.get()->headingDeg();
    *iWid = (int)runway.get()->widthM();

    // Get reciprocal runway
    runway = runway.get()->reciprocalRunway();
    fLat[1] = runway.get()->geod().getLatitudeDeg();
    fLon[1] = runway.get()->geod().getLongitudeDeg();
    fEle[1] = apt->elevationM();
    fHdg[1] = runway.get()->headingDeg();
    // Update reciprocal runway id
    sRW[1] = runway.get()->ident().c_str();

    return 1;
}

int getOppRunwayGeo(const char* sICAO, const char* sRW, double* fLat, double* fLon, double* fEle, double* fHdg, int* iWid)
{
    if (!cache) {
        initNavCache(NULL);
    }

    const string airportId = sICAO;
    const string rwId = sRW;
    const FGAirport* apt = fgFindAirportID(airportId);
    FGRunwayRef runway = apt->getRunwayByIdent(rwId);

    if (!runway) return 0;

    // Get reciprocal runway
    runway = runway.get()->reciprocalRunway();
    *fLat = runway.get()->geod().getLatitudeDeg();
    *fLon = runway.get()->geod().getLongitudeDeg();
    *fEle = apt->elevationM();
    *fHdg = runway.get()->headingDeg();
    *iWid = (int)runway.get()->widthM();

    return 1;
}

int findClosestAirport(char* sICAO, char* sRW, double fLat, double fLon, double fRng)
{
    int ret = -1;
    SGGeod apos = SGGeod::fromDeg(fLon, fLat);
    FGAirport* apt = FGAirport::findClosest(apos, fRng);

    if (!apt) {
        return ret;
    }

    memcpy(sICAO, apt->getId().c_str(), 5);
    ret = 0;

    FGRunway* _runway = apt->findBestRunwayForPos(apos).get();
    if (!_runway) {
        return ret;
    }

    memcpy(sRW, _runway->ident().c_str(), 4);
    ret = 1;

    return ret;
}
    
/*
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
*/
