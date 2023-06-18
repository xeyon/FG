#ifndef APT_LOADER_DLL_H
#define APT_LOADER_DLL_H


#define NVCACHE_REBUILD_DONE 600

/// <summary>
/// This reflect the mem block for EP Airport Information Response from Word 11. 
/// </summary>
struct Apt_Info {

    int airport_info_icao; /*  0-31 */

    int airport_info_gate_cnt; /*  0-31 */

    int airport_info_rwy_cnt; /*  0-31 */

    /// <summary>
    /// db_info[0]: GATE ID ARRAY SIZE
    /// db_info[1 : gate_cnt]: GATE ID ARRAY
    /// db_info[1+gate_cnt]: END A RUNWAY ID ARRAY SIZE
    /// db_info[(2+gate_cnt) : (1+GATE_CNT+RWY_CNT)]: END A RWY ID ARRAY
    /// db_info[2+gate_cnt+rwy_cnt]: END B RUNWAY ID ARRAY SIZE
    /// db_info[(3+gate_cnt+rwy_cnt) : (2+gate_cnt+2*rwy_cnt)]: END B RWY ID ARRAY
    /// db_info[3+gate_cnt+2*rwy_cnt]: RUNWAY LIGHT GROUP ARRAY SIZE
    /// db_info[(4+gate_cnt+2*rwy_cnt) : (3+gate_cnt+3*rwy_cnt)]
    /// </summary>]
    int db_info[54];                /* 20 gates + 10 rwy (end a + end b + light)+ 4 for array size */
} ;

#define EXPORT_DLL __declspec(dllexport)

extern "C" EXPORT_DLL int initGlobals(char* sDatapath);

extern "C" EXPORT_DLL int loadPropsXml(const char* path);
extern "C" EXPORT_DLL int savePropsXml(const char* path);

extern "C" EXPORT_DLL int getPropsBool(const char* name, int* val);
extern "C" EXPORT_DLL int getPropsInt(const char* name, int* val);
extern "C" EXPORT_DLL int getPropsLong(const char* name, long long* val);
extern "C" EXPORT_DLL int getPropsFloat(const char* name, float* val);
extern "C" EXPORT_DLL int getPropsDouble(const char* name, double* val);
extern "C" EXPORT_DLL int getPropsString(const char* name, const char* val);
extern "C" EXPORT_DLL int getPropsBoolList(const char* name, int* val, int num);
extern "C" EXPORT_DLL int getPropsIntList(const char* name, int* val, int num);
extern "C" EXPORT_DLL int getPropsLongList(const char* name, long long* val, int num);
extern "C" EXPORT_DLL int getPropsFloatList(const char* name, float* val, int num);
extern "C" EXPORT_DLL int getPropsDoubleList(const char* name, double* val, int num);

extern "C" EXPORT_DLL int setPropsBool(const char* name, int val);
extern "C" EXPORT_DLL int setPropsInt(const char* name, int val);
extern "C" EXPORT_DLL int setPropsLong(const char* name, long long val);
extern "C" EXPORT_DLL int setPropsFloat(const char* name, float val);
extern "C" EXPORT_DLL int setPropsDouble(const char* name, double val);
extern "C" EXPORT_DLL int setPropsString(const char* name, const char* val);
extern "C" EXPORT_DLL int setPropsBoolList(const char* name, int* val, int num);
extern "C" EXPORT_DLL int setPropsIntList(const char* name, int* val, int num);
extern "C" EXPORT_DLL int setPropsLongList(const char* name, long long* val, int num);
extern "C" EXPORT_DLL int setPropsFloatList(const char* name, float* val, int num);
extern "C" EXPORT_DLL int setPropsDoubleList(const char* name, double* val, int num);

extern "C" EXPORT_DLL int initNavCache(char* sDatapath);
extern "C" EXPORT_DLL int getRebuildProgress();
/// <summary>
///  Retrieve all runways at the airport, but excluding the reciprocal runways.
/// </summary>
/// <param name="sICAO"></param>
/// <param name="nRw"></param>
/// <returns></returns>
extern "C" EXPORT_DLL int getAirportRunways(const char* sICAO, char* result[]);
extern "C" EXPORT_DLL int getAirportGates(const char* sICAO, char* result[]);
/// <summary>
/// Retrieve all runways and gates at the airport
/// </summary>
/// <param name=""></param>
/// <returns>valid lenght of db_info</returns>
extern "C" EXPORT_DLL int getAirportInfoByICAO(struct Apt_Info*);
/// <summary>
/// get rwy dimentions
/// </summary>
/// <param name="sICAO">input airport icao</param>
/// <param name="sRW">2 string elements, sRW[0] is the input near end, sRW[1] is output opp end</param>
/// <param name="fLat">array of 2 Latitudes</param>
/// <param name="fLon">array of 2 Longtitudes</param>
/// <param name="fEle">array of 2 Elevations, usually the same</param>
/// <param name="fHdg">array of 2 Headings, 180deg different</param>
/// <param name="iWid">runway width</param>
/// <returns></returns>
extern "C" EXPORT_DLL int getRunwayGeo(const char* sICAO, const char* rwList[], double* fLat, double* fLon, double* fEle, double* fHdg, int* iWid);
extern "C" EXPORT_DLL int getOppRunwayGeo(const char* sICAO, const char* sRW, double* fLat, double* fLon, double* fEle, double* fHdg, int* iWid);

/// <summary>
/// find the closest airport to specific Lat/Lon within range
/// </summary>
/// <param name="sICAO"></param>
/// <param name="fLat"></param>
/// <param name="fLon"></param>
/// <param name="fRng"></param>
/// <returns>0 - if no airport is found</returns>
extern "C" EXPORT_DLL int findClosestAirport(char* sICAO, char* sRW, double fLat, double fLon, double fRng);

#endif //APT_LOADER_DLL_H