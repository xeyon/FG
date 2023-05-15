// Köppen-Geiger climate interface class
//
// Written by Erik Hofman, started October 2020
//
// Copyright (C) 2020-2021 by Erik Hofman <erik@ehofman.com>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License along
//  with this program; if not, write to the Free Software Foundation, Inc.,
//  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
//
// $Id$

#include <cstring>

#include <osgDB/ReadFile>

#include <simgear/misc/sg_path.hxx>
#include <simgear/math/SGVec3.hxx>
#include <simgear/math/SGVec4.hxx>
#include <simgear/timing/sg_time.hxx>
#include <simgear/constants.h>

#include "Main/fg_props.hxx"
#include "Time/light.hxx"

#include "environment.hxx"
#include "atmosphere.hxx"
#include "climate.hxx"

// Based on "World Map of the Köppen-Geiger climate classification"
// The map is provided with a spatial resolution of approx. 10x10km per pixel.
// http://koeppen-geiger.vu-wien.ac.at/present.htm
//
// References:
// * Teuling, A. J.: Technical note: Towards a continuous classification of
//   climate usingbivariate colour mapping, Hydrol. Earth Syst. Sci., 15,
//   3071–3075, https://doi.org/10.5194/hess-15-3071-2011, 2011.
//
// * Lawrence, Mark G., 2005: The relationship between relative humidity and
//   the dewpoint temperature in moist air: A simple conversion and
//   applications. Bull. Amer. Meteor. Soc., 86, 225-233.
//   doi: http://dx.doi.org/10.1175/BAMS-86-2-225
//
// *  A Simple Accurate Formula for Calculating Saturation Vapor Pressure of
//    Water and Ice. Jianhua Huang, J. Appl. Meteor. Climatol. (2018) 57 (6):
//    1265–1272. https://doi.org/10.1175/JAMC-D-17-0334.1


#define HOUR	(0.5/24.0)
#define MONTH	(1.0/12.0)

FGClimate::FGClimate()
{
    SGPath img_path = globals->get_fg_root() / "Geodata" / "koppen-geiger.png";

    image = osgDB::readImageFile(img_path.utf8Str());
    if (image)
    {
        _image_width = static_cast<double>( image->s() );
        _image_height = static_cast<double>( image->t() );
        _epsilon = 36.0/_image_width;
    }
}

void FGClimate::init()
{
    _monthNode = fgGetNode("/sim/time/utc/month", true);
    _metarSnowLevelNode = fgGetNode("/environment/params/metar-updates-snow-level", true);

    _positionLatitudeNode = fgGetNode("/position/latitude-deg", true);
    _positionLongitudeNode= fgGetNode("/position/longitude-deg", true);
    _gravityNode = fgGetNode("/environment/gravitational-acceleration-mps2", true);
}

void FGClimate::bind()
{
    _rootNode = fgGetNode("/environment/climate", true);
    _rootNode->getNode("description", true)->setStringValue(_description[_code]);
    _rootNode->getNode("classification", true)->setStringValue(_classification[_code]);

    _tiedProperties.setRoot( _rootNode );

    // environment properties
    _tiedProperties.Tie( "environment-update", this, &FGClimate::getEnvironmentUpdate, &FGClimate::setEnvironmentUpdate);

    // METAR
    _tiedProperties.Tie("data", this, &FGClimate::get_metar);

    // weather properties
    _tiedProperties.Tie( "weather-update", &_weather_update);
    _tiedProperties.Tie( "pressure-hpa", &_gl.pressure);
    _tiedProperties.Tie( "pressure-sea-level-hpa", &_gl.pressure);
    _tiedProperties.Tie( "relative-humidity", &_gl.relative_humidity);
    _tiedProperties.Tie( "relative-humidity-sea-level", &_sl.relative_humidity);
    _tiedProperties.Tie( "dewpoint-degc", &_gl.dewpoint);
    _tiedProperties.Tie( "dewpoint-sea-level-degc", &_sl.dewpoint);
    _tiedProperties.Tie( "temperature-degc", &_gl.temperature);
    _tiedProperties.Tie( "temperature-sea-level-degc", &_sl.temperature);
    _tiedProperties.Tie( "temperature-mean-degc", &_gl.temperature_mean);
    _tiedProperties.Tie( "temperature-mean-sea-level-degc", &_sl.temperature_mean);
    _tiedProperties.Tie( "temperature-seawater-degc", &_sl.temperature_water);
    _tiedProperties.Tie( "precipitation-month-mm", &_gl.precipitation);
    _tiedProperties.Tie( "precipitation-annual-mm", & _gl.precipitation_annual);
    _tiedProperties.Tie( "snow-level-m", &_snow_level);
    _tiedProperties.Tie( "wind-speed-mps", &_wind_speed);
    _tiedProperties.Tie( "wind-direction-deg", &_wind_direction);
}

void FGClimate::unbind ()
{
  _tiedProperties.Untie();
}

void FGClimate::reinit()
{
    _prev_lat = -99999.0;
    _prev_lon = -99999.0;

    _gl = ClimateTile();
    _sl = ClimateTile();

    _wind_direction = -99999.0;

    _snow_level = -99999.0;
    _snow_thickness = -99999.0;
    _ice_cover = -99999.0;
    _dust_cover = -99999.0;
    _wetness = -99999.0;
    _lichen_cover = -99999.0;
    _inland_ice_cover = false;
}

// Set all environment parameters based on the koppen-classicfication
// https://en.wikipedia.org/wiki/K%C3%B6ppen_climate_classification
// http://vectormap.si.edu/Climate.htm
void FGClimate::update(double dt)
{
    auto l = globals->get_subsystem<FGLight>();
    if (l)
    {
        _sun_longitude_deg = l->get_sun_lon()*SGD_RADIANS_TO_DEGREES;
        _sun_latitude_deg = l->get_sun_lat()*SGD_RADIANS_TO_DEGREES;
    }

    double latitude_deg  = _positionLatitudeNode->getDoubleValue();
    double longitude_deg = _positionLongitudeNode->getDoubleValue();

    _adj_latitude_deg = latitude_deg - _sun_latitude_deg;
    _adj_longitude_deg = _sun_longitude_deg - longitude_deg;
    if (_adj_longitude_deg < 0.0) _adj_longitude_deg += 360.0;
    else if (_adj_longitude_deg >= 360.0) _adj_longitude_deg -= 360.0;

    double diff_pos = fabs(_prev_lat - _adj_latitude_deg) +
                      fabs(_prev_lon - _adj_longitude_deg);
    if (diff_pos > _epsilon )
    {
        if (diff_pos > 1.0) reinit();

         double north = latitude_deg >= 0.0 ? 1.0 : -1.0; // hemisphere
         if (north) {
             _is_autumn = (_monthNode->getIntValue() > 6) ? 1.0 : 0.0;
         } else {
             _is_autumn = (_monthNode->getIntValue() <= 6) ? 1.0 : 0.0;
         }

        _prev_lat = _adj_latitude_deg;
        _prev_lon = _adj_longitude_deg;

        update_day_factor();
        update_season_factor();
        update_daylight();
        update_wind();

        _code = 0; // Ocean
        osg::Vec4f color;
        if (image)
        {
            // from lat/lon to screen coordinates
            double x = 180.0 + longitude_deg;
            double y =  90.0 + latitude_deg;
            double xs = x * _image_width/360.0;
            double yt = y * _image_height/180.0;
            double rxs = round(xs);
            double ryt = round(yt);

            int s = static_cast<int>(rxs);
            int t = static_cast<int>(ryt);
            color = image->getColor(s, t);

            // convert from color shades to koppen-classicfication
            _gl.elevation_m = 5600.0*color[1];
            _gl.precipitation_annual = 150.0 + 9000.0*color[2];
            _code = static_cast<int>(floorf(255.0f*color[0]/4.0f));
            if (_code >= MAX_CLIMATE_CLASSES)
            {
                SG_LOG(SG_ENVIRONMENT, SG_WARN, "Climate Koppen code exceeds the maximum");
                _code = MAX_CLIMATE_CLASSES-1;
            }

            if (_code == 0) set_ocean();
            else if (_code < 5) set_tropical();
            else if (_code < 9) set_dry();
            else if (_code < 18) set_temperate();
            else if (_code < 30) set_continetal();
            else if (_code < 32) set_polar();
            else set_ocean();
        }

        _rootNode->getNode("description")->setStringValue(_description[_code]);
        _rootNode->getNode("classification")->setStringValue(_classification[_code]);

        // must be after the climate functions and after calculation of
        // _gl.elevation_m
        update_pressure();

        // Mark G. Lawrence:
        _gl.dewpoint = _gl.temperature - ((100.0 - _gl.relative_humidity)/5.0);

        // calculate sea level parameters from ground level parameters
        // adapted from metarproperties.cxx
        FGEnvironment dummy;
        dummy.set_live_update(false);
        dummy.set_elevation_ft(_gl.elevation_m*SG_METER_TO_FEET);
        dummy.set_dewpoint_degc(_gl.dewpoint);
        dummy.set_pressure_inhg(_gl.pressure*atmodel::mbar/atmodel::inHg);

        dummy.set_live_update(true);
        dummy.set_temperature_degc(_gl.temperature);

        _sl.dewpoint = dummy.get_dewpoint_sea_level_degc();
        _sl.relative_humidity = dummy.get_relative_humidity();
        _sl.temperature = dummy.get_temperature_sea_level_degc();
        _sl.pressure = dummy.get_pressure_sea_level_inhg()*atmodel::inHg/atmodel::mbar;

        dummy.set_temperature_degc(_gl.temperature_mean);
        _sl.temperature_mean = dummy.get_temperature_sea_level_degc();

        dummy.set_temperature_degc(_gl.temperature_water);
        _sl.temperature_water = dummy.get_temperature_sea_level_degc();

        set_environment();

#if REPORT_TO_CONSOLE
        report();
#endif
    }
}

// https://en.wikipedia.org/wiki/Sunrise_equation
void FGClimate::update_daylight()
{
    double declination = _sun_latitude_deg*SGD_DEGREES_TO_RADIANS;
    double latitude_deg = _positionLatitudeNode->getDoubleValue();
    double latitude = latitude_deg*SGD_DEGREES_TO_RADIANS;

    double fact = -tan(latitude) * tan(declination);
    if (fact > 1.0) fact = 1.0;
    else if (fact < -1.0) fact = -1.0;

    _day_light = acos(fact)/SGD_PI;
}

// _day_noon returns 0.0 for night up to 1.0 for noon
void FGClimate::update_day_factor()
{
    // noon is when lon == 180.0
    _day_noon = fabs(_adj_longitude_deg - 180.0)/180.0;

    double adj_lon = _adj_longitude_deg;
    if (adj_lon >= 180.0) adj_lon -= 360.0;
    _daytime = 1.0 - (adj_lon + 180.0)/360.0;
}

// _season_summer returns 0.0 for winter up to 1.0 for summer
// _season_transistional returns 0.0 for early autumn/late spring up to
//                               1.0 for late autumn/early spring
//                        to distinguish between the two use the _autumn flag.
void FGClimate::update_season_factor()
{
    double latitude_deg = _positionLatitudeNode->getDoubleValue();
    double sign = latitude_deg >= 0.0 ? 1.0 : -1.0; // hemisphere

    _season_summer = (23.5 + sign*_sun_latitude_deg)/(2.0*23.5);

    _seasons_year = (_is_autumn > 0.05) ? 1.0-0.5*_season_summer :  0.5*_season_summer;

    _season_transistional = 2.0*(1.0 - _season_summer) - 1.0;
    if (_season_transistional < 0.0) _season_transistional = 0.0;
    else if (_season_transistional > 1.0) _season_transistional = 1.0;
}

// https://en.wikipedia.org/wiki/Density_of_air#Humid_air
void FGClimate::update_pressure()
{
    // saturation vapor pressure for water, Jianhua Huang:
    double Tc = _gl.temperature ? _gl.temperature : 1e-9;
    double Psat = exp(34.494 - 4924.99/(Tc+237.1))/pow(Tc + 105.0, 1.57);

    // vapor pressure of water
    double Pv = 0.01*_gl.relative_humidity*Psat;

    // pressure at elevation_itude
    static const double R0 = 8.314462618; // Universal gas constant
    static const double P0 = 101325.0;    // Sea level standard atm. pressure
    static const double T0 = 288.15;      // Sea level standard temperature
    static const double L = 0.0065;       // Temperature lapse rate
    static const double M = 0.0289654;    // Molar mass of dry air

    double h = _gl.elevation_m/1000.0;
    double g = _gravityNode->getDoubleValue();
    double P = P0*pow(1.0 - L*h/T0, g*M/(L*R0));

    // partial pressure of dry air
    double Pd = P - Pv;

    // air pressure
    _gl.pressure = 0.01*(Pd+Psat);	// hPa
}

// https://sites.google.com/site/gitakrishnareach/home/global-wind-patterns
// https://commons.wikimedia.org/wiki/File:Global_Annual_10m_Average_Wind_Speed.png
void FGClimate::update_wind()
{
    double latitude_deg = _positionLatitudeNode->getDoubleValue();
    if (latitude_deg > 60.0)
    {
       double val = 1.0 - (latitude_deg - 60.0)/30.0;
       _set(_wind_direction, linear(val, 0.0, 90.0));
       if (_code == 0) _wind_speed = linear(val, 6.0, 10.0);
    }
    else if (latitude_deg > 30.0)
    {
       double val = (latitude_deg - 30.0)/30.0;
       _set(_wind_direction, linear(val, 180.0, 270.0));
       if (_code == 0) _wind_speed = linear(val, 5.0, 10.0);
       else _wind_speed = linear(1.0 - val, 3.0, 5.0);
    }
    else if (latitude_deg > 0)
    {
      double val = 1.0 - latitude_deg/30.0;
      _set(_wind_direction, linear(val, 0.0, 90.0));
      if (_code == 0) _wind_speed = triangular(val, 5.0, 7.0);
      else _wind_speed = triangular(fabs(val - 0.5), 3.0, 5.0);
    }
    else if (latitude_deg > -30.0)
    {
      double val = -latitude_deg/30.0;
      _set(_wind_direction, linear(val, 90.0, 180.0));
      if (_code == 0) _wind_speed = triangular(val, 5.0, 7.0);
      else _wind_speed = triangular(fabs(val - 0.5), 3.0, 5.0);
    }
    else if (latitude_deg > -60.0)
    {
       double val = 1.0 - (latitude_deg + 30.0)/30.0;
       _set(_wind_direction, linear(val, -90.0, 0.0));
       if (_code == 0) _wind_speed = linear(val, 5.0, 10.0);
       else _wind_speed = linear(1.0 - val, 3.0, 6.0);
    }
    else
    {
       double val = (latitude_deg + 60.0)/30.0;
       _wind_direction = linear(val, 90.0, 180.0);
       if (_code == 0) _wind_speed = linear(1.0 - val, 5.0, 10.0);
    }

    if (_wind_direction < 0.0) _wind_direction += 360.0;
}


void FGClimate::set_ocean()
{
    double day = _day_noon;
    double summer = _season_summer;

    // temperature based on latitude, season and time of day
    // the equator
    double temp_equator_night = triangular(season(summer, MONTH), 17.5, 22.5);
    double temp_equator_day = triangular(season(summer, MONTH), 27.5, 32.5);
    double temp_equator_mean = linear(_day_light, temp_equator_night, temp_equator_day);
    double temp_equator = linear(daytime(day, 3.0*HOUR), temp_equator_night, temp_equator_day);
    double temp_sw_Am = triangular(season(summer, 2.0*MONTH), 22.0, 27.5);

    // the poles
    double temp_pole_night = sinusoidal(season(summer, MONTH), -30.0, 0.0);
    double temp_pole_day = sinusoidal(season(summer, 1.5*MONTH), -22.5, 4.0);
    double temp_pole_mean = linear(_day_light, temp_pole_night, temp_pole_day);
    double temp_pole = linear(daytime(day, 3.0*HOUR), temp_pole_night, temp_pole_day);
    double temp_sw_ET = long_low(season(summer, 2.0*MONTH), -27.5, -3.5);

    // interpolate based on the viewers latitude
    double latitude_deg = _positionLatitudeNode->getDoubleValue();
    double fact_lat = pow(fabs(latitude_deg)/90.0, 2.5);
    double ifact_lat = 1.0 - fact_lat;

    _set(_gl.temperature, linear(ifact_lat, temp_pole, temp_equator));
    _set(_gl.temperature_mean, linear(ifact_lat, temp_pole_mean, temp_equator_mean));
    _set(_gl.temperature_water, linear(ifact_lat, temp_sw_ET, temp_sw_Am));

    // high relative humidity around the equator and poles
    _set(_gl.relative_humidity, triangular(fabs(fact_lat-0.5), 70.0, 87.0));

    _set(_gl.precipitation_annual, 990.0); // global average
    _set(_gl.precipitation, 100.0 - (_gl.precipitation_annual/25.0));

    _gl.has_autumn = false;
}

// https://en.wikipedia.org/wiki/Tropical_rainforest_climate
// https://en.wikipedia.org/wiki/Tropical_monsoon_climate
void FGClimate::set_tropical()
{
    double day = _day_noon;

    double summer = _season_summer;
    double winter = -summer;

    double hmin = sinusoidal(summer, 0.0, 0.36);
    double hmax = sinusoidal(season(winter), 0.86, 1.0);
    double humidity = linear(daytime(day, -9.0*HOUR), hmin, hmax);

    // wind speed based on latitude (0.0 - 15 degrees)
    double latitude_deg = _positionLatitudeNode->getDoubleValue();
    double fact_lat = std::max(abs(latitude_deg), 15.0)/15.0;
    double wind_speed = 3.0*fact_lat*fact_lat;

    double temp_water = _gl.temperature_water;
    double temp_night = _sl.temperature;
    double temp_day = _sl.temperature;
    double precipitation = _gl.precipitation;
    double relative_humidity = _gl.relative_humidity;
    switch(_code)
    {
    case 1: // Af: equatorial, fully humid
        temp_night = triangular(summer, 20.0, 22.5);
        temp_day = triangular(summer, 29.5, 32.5);
        temp_water = triangular(season(summer, MONTH), 25.0, 27.5);
        precipitation = sinusoidal(season(winter), 150.0, 280.0);
        relative_humidity = triangular(humidity, 75.0, 85.0);
        break;
    case 2: // Am: equatorial, monsoonal
        temp_night = triangular(season(summer, MONTH), 17.5, 22.5);
        temp_day = triangular(season(summer, MONTH), 27.5, 32.5);
        temp_water = triangular(season(summer, MONTH), 22.0, 27.5);
        precipitation = linear(season(summer, MONTH), 45.0, 340.0);
        relative_humidity = triangular(humidity, 75.0, 85.0);
        wind_speed *= 2.0*_gl.precipitation/340.0;
        break;
    case 3: // As: equatorial, summer dry
        temp_night = long_high(season(summer, .15*MONTH), 15.0, 22.5);
        temp_day = triangular(season(summer, MONTH), 27.5, 35.0);
        temp_water = triangular(season(summer, 2.0*MONTH), 21.5, 26.5);
        precipitation = sinusoidal(season(summer, 2.0*MONTH), 35.0, 150.0);
        relative_humidity = triangular(humidity, 60.0, 80.0);
        wind_speed *= 2.0*_gl.precipitation/150.0;
        break;
    case 4: // Aw: equatorial, winter dry
        temp_night = long_high(season(summer, 1.5*MONTH), 15.0, 22.5);
        temp_day = triangular(season(summer, 2.0*MONTH), 27.5, 35.0);
        temp_water = triangular(season(summer, 2.0*MONTH), 21.5, 28.5);
        precipitation = sinusoidal(season(summer, 2.0*MONTH), 10.0, 230.0);
        relative_humidity = triangular(humidity, 60.0, 80.0);
        wind_speed *= 2.0*_gl.precipitation/230.0;
        break;
    default:
        break;
    }

    _set(_gl.temperature, linear(daytime(day, 3.0*HOUR), temp_night, temp_day));
    _set(_gl.temperature_mean, linear(_day_light, temp_night, temp_day));
    _set(_gl.temperature_water, temp_water);

    _set(_gl.relative_humidity, relative_humidity);
    _set(_gl.precipitation, precipitation);

    _gl.has_autumn = false;
    _wind_speed = wind_speed;
}

// https://en.wikipedia.org/wiki/Desert_climate
// https://en.wikipedia.org/wiki/Semi-arid_climate
void FGClimate::set_dry()
{
    double day = _day_noon;

    double summer = _season_summer;
    double winter = -summer;

    double hmin = sinusoidal(summer, 0.0, 0.36);
    double hmax = sinusoidal(season(winter), 0.86, 1.0);
    double humidity = linear(daytime(day, -9.0*HOUR), hmin, hmax);

    double temp_water = _gl.temperature_water;
    double temp_night = _sl.temperature;
    double temp_day = _sl.temperature;
    double precipitation = _gl.precipitation;
    double relative_humidity = _gl.relative_humidity;
    switch(_code)
    {
    case 5: // BSh: arid, steppe, hot arid
        temp_night = long_high(season(summer, MONTH), 10.0, 22.0);
        temp_day = triangular(season(summer, 2.0*MONTH), 27.5, 35.0);
        temp_water = triangular(season(summer, 2.5*MONTH), 18.5, 28.5);
        precipitation = long_low(season(summer, 2.0*MONTH), 8.0, 117.0);
        relative_humidity = triangular(humidity, 20.0, 30.0);
        break;
    case 6: // BSk: arid, steppe, cold arid
        temp_night = sinusoidal(season(summer, MONTH), -14.0, 12.0);
        temp_day = sinusoidal(season(summer, MONTH), 0.0, 30.0);
        temp_water = sinusoidal(season(summer, 2.0*MONTH), 5.0, 25.5);
        precipitation = sinusoidal(season(summer, MONTH), 15.0, 34.0);
        relative_humidity = sinusoidal(humidity, 48.0, 67.0);
        break;
    case 7: // BWh: arid, desert, hot arid
        temp_night = sinusoidal(season(summer, 1.5*MONTH), 7.5, 22.0);
        temp_day = even(season(summer, 1.5*MONTH), 22.5, 37.5);
        temp_water = even(season(summer, 2.5*MONTH), 15.5, 33.5);
        precipitation = monsoonal(season(summer, 2.0*MONTH), 3.0, 18.0);
        relative_humidity = monsoonal(humidity, 25.0, 55.0);
        break;
    case 8: // BWk: arid, desert, cold arid
        temp_night = sinusoidal(season(summer, MONTH), -15.0, 15.0);
        temp_day = sinusoidal(season(summer, MONTH), -2.0, 30.0);
        temp_water = sinusoidal(season(summer, 2.0*MONTH), 4.0, 26.5);
        precipitation = linear(season(summer, MONTH), 4.0, 14.0);
        relative_humidity = linear(humidity, 45.0, 61.0);
        break;
    default:
        break;
    }

    _set(_gl.temperature, linear(daytime(day, 3.0*HOUR), temp_night, temp_day));
    _set(_gl.temperature_mean, linear(_day_light, temp_night, temp_day));
    _set(_gl.temperature_water, temp_water);

    _set(_gl.relative_humidity, relative_humidity);
    _set(_gl.precipitation, precipitation);
    _gl.has_autumn = false;
}

// https://en.wikipedia.org/wiki/Temperate_climate
void FGClimate::set_temperate()
{
    double day = _day_noon;

    double summer = _season_summer;
    double winter = -summer;

    double hmin = sinusoidal(summer, 0.0, 0.36);
    double hmax = sinusoidal(season(winter), 0.86, 1.0);
    double humidity = linear(daytime(day, -9.0*HOUR), hmin, hmax);

    double temp_water = _gl.temperature_water;
    double temp_night = _sl.temperature;
    double temp_day = _sl.temperature;
    double precipitation = _gl.precipitation;
    double relative_humidity = _gl.relative_humidity;
    switch(_code)
    {
    case 9: // Cfa: warm temperature, fully humid hot summer
        temp_night = sinusoidal(season(summer, 1.5*MONTH), -3.0, 20.0);
        temp_day = sinusoidal(season(summer, 1.5*MONTH), 10.0, 33.0);
        temp_water = sinusoidal(season(summer, 2.5*MONTH), 8.0, 28.5);
        precipitation = sinusoidal(summer, 60.0, 140.0);
        relative_humidity = sinusoidal(humidity, 65.0, 80.0);
        break;
    case 10: // Cfb: warm temperature, fully humid, warm summer
        temp_night = sinusoidal(season(summer, 1.5*MONTH), -3.0, 10.0);
        temp_day = sinusoidal(season(summer, 1.5*MONTH), 5.0, 25.0);
        temp_water = sinusoidal(season(summer, 2.5*MONTH), 3.0, 20.5);
        precipitation = sinusoidal(season(winter, 3.5*MONTH), 65.0, 90.0);
        relative_humidity = sinusoidal(humidity, 68.0, 87.0);
        break;
    case 11: // Cfc: warm temperature, fully humid, cool summer
        temp_night = long_low(season(summer, 1.5*MONTH), -3.0, 8.0);
        temp_day = long_low(season(summer, 1.5*MONTH), 2.0, 14.0);
        temp_water = long_low(season(summer, 2.5*MONTH), 3.0, 11.5);
        precipitation = linear(season(winter), 90.0, 200.0);
        relative_humidity = long_low(humidity, 70.0, 85.0);
        break;
    case 12: // Csa: warm temperature, summer dry, hot summer
        temp_night = sinusoidal(season(summer, MONTH), 2.0, 16.0);
        temp_day = sinusoidal(season(summer, MONTH), 12.0, 33.0);
        temp_water = sinusoidal(season(summer, 2.0*MONTH), 10.0, 27.5);
        precipitation = linear(season(winter), 25.0, 70.0);
        relative_humidity = sinusoidal(humidity, 58.0, 72.0);
        break;
    case 13: // Csb: warm temperature, summer dry, warm summer
        temp_night = linear(season(summer, 1.5*MONTH), -4.0, 10.0);
        temp_day = linear(season(summer, 1.5*MONTH), 6.0, 27.0);
        temp_water = linear(season(summer, 2.5*MONTH), 4.0, 21.5);
        precipitation = linear(season(winter), 25.0, 120.0);
        relative_humidity = linear(humidity, 50.0, 72.0);
        break;
    case 14: // Csc: warm temperature, summer dry, cool summer
        temp_night = sinusoidal(season(summer, 0.5*MONTH), -4.0, 5.0);
        temp_day = sinusoidal(season(summer, 0.5*MONTH), 5.0, 16.0);
        temp_water = sinusoidal(season(summer, 1.5*MONTH), 3.0, 14.5);
        precipitation = sinusoidal(season(winter, -MONTH), 60.0, 95.0);
        relative_humidity = sinusoidal(humidity, 55.0, 75.0);
        break;
    case 15: // Cwa: warm temperature, winter dry, hot summer
        temp_night = even(season(summer, MONTH), 4.0, 20.0);
        temp_day = long_low(season(summer, MONTH), 15.0, 30.0);
        temp_water = long_low(season(summer, 2.0*MONTH), 7.0, 24.5);
        precipitation = long_low(season(summer, MONTH), 10.0, 320.0);
        relative_humidity = sinusoidal(humidity, 60.0, 79.0);
        break;
    case 16: // Cwb: warm temperature, winter dry, warm summer
        temp_night = even(season(summer, MONTH), 1.0, 13.0);
        temp_day = long_low(season(summer, MONTH), 15.0, 27.0);
        temp_water = even(season(summer, 2.0*MONTH), 5.0, 22.5);
        precipitation = long_low(season(summer, MONTH), 10.0, 250.0);
        relative_humidity = sinusoidal(humidity, 58.0, 72.0);
        break;
    case 17: // Cwc: warm temperature, winter dry, cool summer
        temp_night = long_low(season(summer, MONTH), -9.0, 6.0);
        temp_day = long_high(season(summer, MONTH), 6.0, 17.0);
        temp_water = long_high(season(summer, 2.0*MONTH), 8.0, 15.5);
        precipitation = long_low(season(summer, MONTH), 5.0, 200.0);
        relative_humidity = long_high(humidity, 50.0, 58.0);
        break;
    default:
        break;
    }

    _set(_gl.temperature, linear(daytime(day, 3.0*HOUR), temp_night, temp_day));
    _set(_gl.temperature_mean, linear(_day_light, temp_night, temp_day));
    _set(_gl.temperature_water, temp_water);

    _set(_gl.relative_humidity, relative_humidity);
    _set(_gl.precipitation, precipitation);

    _gl.has_autumn = true;

}

// https://en.wikipedia.org/wiki/Continental_climate
void FGClimate::set_continetal()
{
    double day = _day_noon;

    double summer = _season_summer;
    double winter = -summer;

    double hmin = sinusoidal(summer, 0.0, 0.36);
    double hmax = sinusoidal(season(winter), 0.86, 1.0);
    double humidity = linear(daytime(day, -9.0*HOUR), hmin, hmax);

    double temp_water = _gl.temperature_water;
    double temp_day = _sl.temperature;
    double temp_night = _sl.temperature;
    double precipitation = _gl.precipitation;
    double relative_humidity = _gl.relative_humidity;
    switch(_code)
    {
    case 18: // Dfa: snow, fully humid, hot summer
        temp_night = sinusoidal(season(summer, MONTH), -15.0, 13.0);
        temp_day = sinusoidal(season(summer, MONTH), -5.0, 30.0);
        temp_water = sinusoidal(season(summer, 2.0*MONTH), 0.0, 26.5);
        precipitation = linear(season(summer, MONTH), 30.0, 70.0);
        relative_humidity = sinusoidal(humidity, 68.0, 72.0);
        break;
    case 19: // Dfb: snow, fully humid, warm summer, warm summer
        temp_night = sinusoidal(season(summer, MONTH), -17.5, 10.0);
        temp_day = sinusoidal(season(summer, MONTH), -7.5, 25.0);
        temp_water = sinusoidal(season(summer, 2.0*MONTH), -2.0, 22.5);
        precipitation = linear(season(summer, MONTH), 30.0, 70.0);
        relative_humidity = sinusoidal(humidity, 69.0, 81.0);
        break;
    case 20: // Dfc: snow, fully humid, cool summer, cool summer
        temp_night = sinusoidal(season(summer, MONTH), -30.0, 4.0);
        temp_day = sinusoidal(season(summer, MONTH), -20.0, 15.0);
        temp_water = sinusoidal(season(summer, 2.0*MONTH), -10.0, 12.5);
        precipitation = linear(season(summer, 1.5*MONTH), 22.0, 68.0);
        relative_humidity = sinusoidal(humidity, 70.0, 88.0);
        _wind_speed = 3.0;
        break;
    case 21: // Dfd: snow, fully humid, extremely continetal
        temp_night = sinusoidal(season(summer, MONTH), -45.0, 4.0);
        temp_day = sinusoidal(season(summer, MONTH), -35.0, 10.0);
        temp_water = sinusoidal(season(summer, 2.0*MONTH), -15.0, 8.5);
        precipitation = long_low(season(summer, 1.5*MONTH), 7.5, 45.0);
        relative_humidity = sinusoidal(humidity, 80.0, 90.0);
        break;
    case 22: // Dsa: snow, summer dry, hot summer
        temp_night = sinusoidal(season(summer, 1.5*MONTH), -10.0, 10.0);
        temp_day = sinusoidal(season(summer, 1.5*MONTH), 0.0, 30.0);
        temp_water = sinusoidal(season(summer, 3.5*MONTH), 4.0, 24.5);
        precipitation = long_high(season(winter, 2.0*MONTH), 5.0, 65.0);
        relative_humidity = sinusoidal(humidity, 48.0, 58.08);
        break;
    case 23: // Dsb: snow, summer dry, warm summer
        temp_night = sinusoidal(season(summer, 1.5*MONTH), -15.0, 6.0);
        temp_day = sinusoidal(season(summer, 1.5*MONTH), -4.0, 25.0);
        temp_water = sinusoidal(season(summer, 2.5*MONTH), 0.0, 19.5);
        precipitation = long_high(season(winter, 2.0*MONTH), 12.0, 65.0);
        relative_humidity = sinusoidal(humidity, 50.0, 68.0);
        break;
    case 24: // Dsc: snow, summer dry, cool summer
        temp_night = sinusoidal(season(summer, MONTH), -27.5, 2.0);
        temp_day = sinusoidal(season(summer, MONTH), -4.0, 15.0);
        temp_water = sinusoidal(season(summer, 2.0*MONTH), 0.0, 12.5);
        precipitation = long_low(season(summer, MONTH), 32.5, 45.0);
        relative_humidity = sinusoidal(humidity, 50.0, 60.0);
        break;
    case 25: // Dsd: snow, summer dry, extremely continetal
        temp_night = sinusoidal(season(summer, MONTH), -11.5, -6.5);
        temp_day = sinusoidal(season(summer, MONTH), 14.0, 27.0);
        temp_water = sinusoidal(season(summer, 2.0*MONTH), 8.0, 20.5);
        precipitation = long_low(season(summer, MONTH), 5.0, 90.0);
        relative_humidity = sinusoidal(humidity, 48.0, 62.0);
        break;
    case 26: // Dwa: snow, winter dry, hot summer
        temp_night = sinusoidal(season(summer, MONTH), -18.0, 16.5);
        temp_day = sinusoidal(season(summer, MONTH), -5.0, 25.0);
        temp_water = sinusoidal(season(summer, 2.0*MONTH), 0.0, 23.5);
        precipitation = long_low(season(summer, 1.5*MONTH), 5.0, 180.0);
        relative_humidity = sinusoidal(humidity, 60.0, 68.0);
        break;
    case 27: // Dwb: snow, winter dry, warm summer
        temp_night = sinusoidal(season(summer, MONTH), -28.0, 10.0);
        temp_day = sinusoidal(season(summer, MONTH), -12.5, 22.5);
        temp_water = sinusoidal(season(summer, 2.0*MONTH), -5.0, 18.5);
        precipitation = long_low(season(summer, 1.5*MONTH), 10.0, 140.0);
        relative_humidity = sinusoidal(humidity, 60.0, 72.0);
        break;
    case 28: // Dwc: snow, winter dry, cool summer
        temp_night = sinusoidal(season(summer, MONTH), -33.0, 5.0);
        temp_day = sinusoidal(season(summer, MONTH), -20.0, 20.0);
        temp_water = sinusoidal(season(summer, 2.0*MONTH), -10.0, 16.5);
        precipitation = long_low(season(summer, 1.5*MONTH), 10.0, 110.0);
        relative_humidity = sinusoidal(humidity, 60.0, 78.0);
        break;
    case 29: // Dwd: snow, winter dry, extremely continetal
        temp_night = sinusoidal(season(summer, MONTH), -57.5, 0.0);
        temp_day = sinusoidal(season(summer, MONTH), -43.0, 15.0);
        temp_water = sinusoidal(season(summer, 2.0*MONTH), -28.0, 11.5);
        precipitation = sinusoidal(season(summer, 1.5*MONTH), 8.0, 63.0);
        relative_humidity = 80.0;
        break;
    default:
        break;
    }

    _set(_gl.temperature, linear(daytime(day, 3.0*HOUR), temp_night, temp_day));
    _set(_gl.temperature_mean, linear(_day_light, temp_night, temp_day));
    _set(_gl.temperature_water, temp_water);

    _set(_gl.relative_humidity, relative_humidity);
    _set(_gl.precipitation, precipitation);

    _gl.has_autumn = true;
}

void FGClimate::set_polar()
{
    double day = _day_noon;

    double summer = _season_summer;
    double winter = -summer;

    double hmin = sinusoidal(summer, 0.0, 0.36);
    double hmax = sinusoidal(season(winter), 0.86, 1.0);
    double humidity = linear(daytime(day, -9.0*HOUR), hmin, hmax);

    // polar climate also occurs high in the mountains
    double temp_water = _gl.temperature_water;
    double temp_day = _sl.temperature;
    double temp_night = _sl.temperature;
    double precipitation = _gl.precipitation;
    double relative_humidity = _gl.relative_humidity;
    switch(_code)
    {
    case 30: // EF: polar frost
        temp_night = long_low(season(summer, MONTH), -35.0, -6.0);
        temp_day = long_low(season(summer, MONTH), -32.5, 0.0);
        temp_water = long_low(season(summer, 2.0*MONTH), -27.5, -3.5);
        precipitation = linear(season(summer, 2.5*MONTH), 50.0, 80.0);
        relative_humidity = long_low(humidity, 65.0, 75.0);
        _wind_speed = 5.5;
        break;
    case 31: // ET: polar tundra
        temp_night = sinusoidal(season(summer, MONTH), -30.0, 0.0);
        temp_day = sinusoidal(season(summer, 1.5*MONTH), -22.5, 8.0);
        temp_water = sinusoidal(season(summer, 2.0*MONTH), -15.0, 5.0);
        precipitation = sinusoidal(season(summer, 2.0*MONTH), 15.0, 45.0);
        relative_humidity = sinusoidal(humidity, 60.0, 88.0);
        _wind_speed = 4.0;
        break;
    default:
        break;
    }

    _set(_gl.temperature, linear(daytime(day, 3.0*HOUR), temp_night, temp_day));
    _set(_gl.temperature_mean, linear(_day_light, temp_night, temp_day));
    _set(_gl.temperature_water, temp_water);

    _set(_gl.relative_humidity, relative_humidity);
    _set(_gl.precipitation, precipitation);

    _gl.has_autumn = true;
}

void FGClimate::set_environment()
{
    double snow_fact, precipitation;

    _set(_snow_level, 1000.0*_sl.temperature_mean/9.8);

    // snow chance based on latitude, mean temperature and monthly precipitation
    if (_gl.precipitation < 60.0) {
        precipitation = 0.0;
    }
    else
    {
        precipitation = _gl.precipitation - 60.0;
        precipitation = (precipitation > 240.0) ? 1.0 : precipitation/240.0;
    }

    if (_gl.temperature_mean > 5.0 || precipitation < 0.1) {
        snow_fact = 0.0;
    }
    else
    {
        snow_fact = fabs(_gl.temperature_mean) - 5.0;
        snow_fact = (snow_fact > 10.0) ? 1.0 : snow_fact/10.0;
        snow_fact *= precipitation;
    }

    // Sea water will start to freeze at -2° water temperaure.
    if (_is_autumn < 0.95 || _sl.temperature_mean > -2.0) {
        _set(_ice_cover, 0.0);
    } else {
        _set(_ice_cover, std::min(-(_sl.temperature_water+2.0)/10.0, 1.0));
    }

    // Inland water will start to freeze at 0⁰ water temperature.
    if (_gl.temperature_mean > 0.5) {
        _inland_ice_cover = false;
    } else {
        _inland_ice_cover = true;
    }

    // less than 20 mm/month of precipitation is considered dry.
    if (_gl.precipitation < 20.0 || _gl.precipitation_annual < 240.0)
    {
        _set(_dust_cover, 0.3 - 0.3*sqrtf(_gl.precipitation/20.0));
        _set(_snow_thickness, 0.0);
        _set(_lichen_cover, 0.0);
        _set(_wetness, 0.0);
    }
    else
    {
        _dust_cover = 0.0;

        if (_gl.temperature_mean < 5.0)
        {
            double wetness = 12.0*_gl.precipitation/990.0;
            wetness = pow(sin(atan(SGD_PI*wetness)), 2.0);
            _snow_thickness = wetness;
        } else {
            _snow_thickness = 0.0;
        }

        if (_gl.temperature_mean > 0.0)
        {
            // https://weatherins.com/rain-guidelines/
            _wetness = 9.0*std::max(_gl.precipitation-50.0, 0.0)/990.0;

            double diff = std::max(_gl.temperature - _gl.dewpoint, 1.0);
            _wetness = std::min(_wetness/diff, 1.0);
        } else {
            _wetness = 0.0;
        }

        double cover = std::min(_gl.precipitation_annual, 990.0)/990.0;
        _set(_lichen_cover, 0.5*pow(_wetness*cover, 1.5));
    }

    _rootNode->setDoubleValue("daytime-pct", _daytime*100.0);
    _rootNode->setDoubleValue("daylight-pct", _day_light*100.0);
    _rootNode->setDoubleValue("day-noon-pct", _day_noon*100.0);
    _rootNode->setDoubleValue("season-summer-pct", _season_summer*100.0);
    _rootNode->setDoubleValue("seasons-year-pct", _seasons_year*100.0);
    _rootNode->setDoubleValue("season-autumn-pct", ((_gl.has_autumn && _is_autumn > 0.05) ? _season_transistional*100.0 : 0.0));

#if 0
    if (_weather_update)
    {
        fgSetDouble("/environment/relative-humidity", _gl.relative_humidity);
        fgSetDouble("/environment/dewpoint-sea-level-degc", _sl.dewpoint);
        fgSetDouble("/environment/dewpoint-degc", _gl.dewpoint);
        fgSetDouble("/environment/temperature-sea-level-degc", _sl.temperature);
        fgSetDouble("/environment/temperature-degc", _gl.temperature);
        fgSetDouble("/environment/wind-speed-mps", _wind_speed);
    }
#endif

    if (_environment_adjust)
    {
        if (!_metarSnowLevelNode->getBoolValue()) {
            fgSetDouble("/environment/snow-level-m", _snow_level);
        }
        fgSetDouble("/environment/surface/snow-thickness-factor", _snow_thickness);
        fgSetBool("/environment/surface/ice-cover", _inland_ice_cover);
        fgSetDouble("/environment/sea/surface/ice-cover", _ice_cover);
        fgSetDouble("/environment/surface/dust-cover-factor", _dust_cover);
        fgSetDouble("/environment/surface/wetness-set", _wetness);
        fgSetDouble("/environment/surface/lichen-cover-factor", _lichen_cover);

        double season = 1.7*std::pow(_season_transistional, 4.0);
        if (_gl.has_autumn && season > 0.01)
        {
            if (_is_autumn > 0.01) {	// autumn
                fgSetDouble("/environment/season", _is_autumn*season);
            } else { 			// sping
                fgSetDouble("/environment/season", 1.5+0.25*season);
            }
        }
        else
        {
            fgSetDouble("/environment/season", 0.0);
        }
    }

    // for material animation
    double latitude_deg = _positionLatitudeNode->getDoubleValue();
    double desert_pct = std::min( fabs(latitude_deg-23.5)/23.5, 1.0);
    fgSetDouble("/sim/rendering/desert", 0.1-0.2*desert_pct);
}

void FGClimate::setEnvironmentUpdate(bool value)
{
    _environment_adjust = value;

    if (_metarSnowLevelNode)
    {
        if (value) {
            update(0.0);
        }
        else
        {
            if (!_metarSnowLevelNode->getBoolValue()) {
                fgSetDouble("/environment/snow-level-m", 8000.0);
            }
            fgSetDouble("/environment/surface/snow-thickness-factor", 0.0);
            fgSetDouble("/environment/sea/surface/ice-cover", 0.0);
            fgSetDouble("/environment/surface/dust-cover-factor", 0.0);
            fgSetDouble("/environment/surface/wetness-set", 0.0);
            fgSetDouble("/environment/surface/lichen-cover-factor", 0.0);
            fgSetDouble("/environment/season", 0.0);
        }
    }
}

const char* FGClimate::get_metar() const
{
    std::stringstream m;

    m << std::fixed << std::setprecision(0);
    m << "XXXX ";

    m << setw(2) << std::setfill('0') << fgGetInt("/sim/time/utc/day");
    m << setw(2) << std::setfill('0') << fgGetInt("/sim/time/utc/hour");
    m << setw(2) << std::setfill('0') << fgGetInt("/sim/time/utc/minute");
    m << "Z AUTO ";

    m << setw(3) << std::setfill('0') << _wind_direction;
    m << setw(2) << std::setfill('0') << _wind_speed << "MPS ";

    if (_gl.temperature < 0.0) {
        m << "M" << setw(2) << std::setfill('0') << fabs(_gl.temperature);
    } else {
        m << setw(2) << std::setfill('0') << _gl.temperature;
    }

    m << "/";

    if (_gl.dewpoint < 0.0) {
        m << "M" << setw(2) << std::setfill('0') << fabs(_gl.dewpoint);
    } else {
        m << setw(2) << std::setfill('0') << _gl.dewpoint;
    }

    m << " Q" << _sl.pressure;

    m << " NOSIG";

   char *rv = std::strncpy((char*)&_metar, m.str().c_str(), 255);
   rv[255] = '\0';

    return rv;
}

// ---------------------------------------------------------------------------

const std::string FGClimate::_classification[MAX_CLIMATE_CLASSES] = {
    "Ocean",
    "Af", "Am", "As", "Aw",
    "BSh", "BSk", "BWh", "BWk",
    "Cfa", "Cfb", "Cfc",
    "Csa", "Csb", "Csc",
    "Cwa", "Cwb", "Cwc",
    "Dfa", "Dfb", "Dfc", "Dfd",
    "Dsa", "Dsb", "Dsc", "Dsd",
    "Dwa", "Dwb", "Dwc", "Dwd",
    "EF", "ET"
};

const std::string FGClimate::_description[MAX_CLIMATE_CLASSES] = {
    "Ocean",
    "Equatorial, fully humid",
    "Equatorial, monsoonal",
    "Equatorial, summer dry",
    "Equatorial, winter dry",
    "Arid, steppe, hot arid",
    "Arid, steppe, cold arid",
    "Arid, desert, hot arid",
    "Arid, desert, cold arid",
    "Warm temperature, fully humid hot summer",
    "Warm temperature, fully humid, warm summer",
    "Warm temperature, fully humid, cool summer",
    "Warm temperature, summer dry, hot summer",
    "Warm temperature, summer dry, warm summer",
    "Warm temperature, summer dry, cool summer",
    "Warm temperature, winter dry, hot summer",
    "Warm temperature, winter dry, warm summer",
    "Warm temperature, winter dry, cool summer",
    "Snow, fully humid, hot summer",
    "Snow, fully humid, warm summer",
    "Snow, fully humid, cool summer",
    "Snow, fully humid, extremely continetal",
    "Snow, summer dry, hot summer",
    "Snow, summer dry, warm summer",
    "Snow, summer dry, cool summer",
    "Snow, summer dry, extremely continetal",
    "Snow, winter dry, hot summer",
    "Snow, winter dry, warm summer",
    "Snow, winter dry, cool summer",
    "Snow, winter dry, extremely continetal",
    "Polar frost",
    "Polar tundra"
};


// interpolators
double FGClimate::daytime(double val, double offset)
{
    val = fmod(fabs(_daytime - offset), 1.0);
    val = (val > 0.5)  ? 2.0 - 2.0*val : 2.0*val;
    return val;
}

double FGClimate::season(double val, double offset)
{
    val = (val >= 0.0) ? fmod(fabs(_seasons_year - offset), 1.0)
                       : fmod(fabs(1.0 - _seasons_year + offset), 1.0);
    val = (val > 0.5)  ? 2.0 - 2.0*val : 2.0*val;
    return val;
}

// val is the progress indicator between 0.0 and 1.0
double FGClimate::linear(double val, double min, double max)
{
    double diff = max-min;
    return min + val*diff;
}

// google: y=1-abs(-1+2*x)
// low season is around 0.0 and 1.0, high season around 0.5
double FGClimate::triangular(double val, double min, double max)
{
    double diff = max-min;
    val = 1.0 - fabs(-1.0 + 2.0*val);
    return min + val*diff;
}


// google: y=0.5-0.5*cos(x)
// the short low season is round 0.0, the short high season around 1.0
double FGClimate::sinusoidal(double val, double min, double max)
{
    double diff = max-min;
    return min + diff*(0.5 - 0.5*cos(SGD_PI*val));
}

// google: y=0.5-0.6366*atan(cos(x))
// the medium long low season is round 0.0, medium long high season round 1.0
double FGClimate::even(double val, double min, double max)
{
    double diff = max-min;
    return min + diff*(0.5 - 0.6366*atan(cos(SGD_PI*val)));
}

// google: y=0.5-0.5*cos(x^1.5)
// 2.145 = pow(SGD_PI, 1.0/1.5)
// the long low season is around 0.0, the short high season around 1.0
double FGClimate::long_low(double val, double min, double max)
{
    double diff = max-min;
    return min + diff*(0.5 - 0.5*cos(pow(2.145*val, 1.5)));
}

// google: y=0.5+0.5*cos(x^1.5)
// 2.14503 = pow(SGD_PI, 1.0/1.5)
// the long high season is around 0.0, the short low season around 1.0
double FGClimate::long_high(double val, double min, double max)
{
    double diff = max-min;
    return max - diff*(0.5 - 0.5*cos(pow(2.14503 - 2.14503*val, 1.5)));
}

// goole: y=cos(atan(x*x))
// the monsoon is around 0.0
double FGClimate::monsoonal(double val, double min, double max)
{
    double diff = max-min;
    val = 2.0*SGD_2PI*(1.0-val);
    return min + diff*cos(atan(val*val));
}

void FGClimate::test()
{
    double offs = 0.25;

    printf("month:\t\t");
    for (int month=0; month<12; ++month) {
        printf("%5i", month+1);
    }

    printf("\nlinear:\t\t");
    for (int month=0; month<12; ++month) {
        double val = (month > 6) ? (12 - month)/6.0 : month/6.0;
        _seasons_year = month/12.0;
        printf(" %-3.2f", linear(season(val, offs), 0.0, 1.0) );
    }

    printf("\ntriangular:\t");
    for (int month=0; month<12; ++month) {
        double val = (month > 6) ? (12 - month)/6.0 : month/6.0;
        _seasons_year = month/12.0;
        printf(" %-3.2f", triangular(season(val, offs), 0.0, 1.0) );
    }

    printf("\neven:\t\t");
    for (int month=0; month<12; ++month) {
        double val = (month > 6) ? (12 - month)/6.0 : month/6.0;
        _seasons_year = month/12.0;
        printf(" %-3.2f", sinusoidal(season(val, offs), 0.0, 1.0) );
    }

    printf("\nlong:\t\t");
    for (int month=0; month<12; ++month) {
        double val = (month > 6) ? (12 - month)/6.0 : month/6.0;
        _seasons_year = month/12.0;
        printf(" %-3.2f", even(season(val, offs), 0.0, 1.0) );
    }

    printf("\nlong low:\t");
    for (int month=0; month<12; ++month) {
        double val = (month > 6) ? (12 - month)/6.0 : month/6.0;
        _seasons_year = month/12.0;
        printf(" %-3.2f", long_low(season(val, offs), 0.0, 1.0) );
    }

    printf("\nlong high:\t");
    for (int month=0; month<12; ++month) {
        double val = (month > 6) ? (12 - month)/6.0 : month/6.0;
        _seasons_year = month/12.0;
        printf(" %-3.2f", long_high(season(val, offs), 0.0, 1.0) );
    }

    printf("\nmonsoonal:\t");
    for (int month=0; month<12; ++month) {
        double val = (month > 6) ? (12 - month)/6.0 : month/6.0;
        _seasons_year = month/12.0;
        printf(" %-3.2f", monsoonal(season(val, offs), 0.0, 1.0) );
    }
    printf("\n");
}

#if REPORT_TO_CONSOLE
void FGClimate::report()
{
    struct tm *t = globals->get_time_params()->getGmt();

    std::cout << "===============================================" << std::endl;
    std::cout << "Climate report for:" << std::endl;
    std::cout << "  Date: " << sgTimeFormatTime(t) << " GMT" << std::endl;
    std::cout << "  Sun latitude:    " << _sun_latitude_deg << " deg."
              << std::endl;
    std::cout << "  Sun longitude:   " << _sun_longitude_deg << " deg."
              << std::endl;
    std::cout << "  Viewer latitude:  "
              << _positionLatitudeNode->getDoubleValue() << " deg."
              << " (adjusted: " << _adj_latitude_deg << ")" << std::endl;
    std::cout << "  Viewer longitude: "
              << _positionLongitudeNode->getDoubleValue() << " deg."
              << " (adjusted: " << _adj_longitude_deg << ")" << std::endl;
    std::cout << std::endl;
    std::cout << "  Köppen classification: " << _classification[_code]
              << std::endl;
    std::cout << "  Description: " << _description[_code]
              << std::endl << std::endl;
    std::cout << "  Daylight: " << _day_light*100.0 << " %"
              << std::endl;
    std::cout << "  Daytime:  "<< _daytime*24.0 << " hours" << std::endl;
    std::cout << "  Daytime...(0.0 = night .. 1.0 = noon): " << _day_noon
              << std::endl;
    std::cout << "  Season (0.0 = winter .. 1.0 = summer): " << _season_summer
              << std::endl;
    std::cout << "  Year (0.25 = spring .. 0.75 = autumn): " << _seasons_year
              << std::endl << std::endl;
    std::cout << "  Dewpoint:              " << _gl.dewpoint << " deg. C."
              << std::endl;
    std::cout << "  Ground temperature:    " << _gl.temperature << " deg. C."
              << std::endl;
    std::cout << "  Sea level temperature: " << _sl.temperature << " deg. C."
              << std::endl;
    std::cout << "  Ground Mean temp.:     " << _gl.temperature_mean
              << " deg. C." << std::endl;
    std::cout << "  Sea level mean temp.:   " << _sl.temperature_mean
              << " deg. C." << std::endl;
    std::cout << "  Seawater temperature: " << _sl.temperature_water
              << " deg. C." << std::endl;

    std::cout << "  Months precipitation:  " << _gl.precipitation << " mm"
              << std::endl;
    std::cout << "  Annual precipitation:  " << _gl.precipitation_annual << " mm"
              << std::endl;
    std::cout << "  Relative humidity:     " << _gl.relative_humidity << " %"
              << std::endl;
    std::cout << "  Ground Air Pressure:   " << _gl.pressure << " hPa"
              << std::endl;
    std::cout << "  Sea level Air Pressure: " << _sl.pressure << " hPa"
              << std::endl;
    std::cout << "  Wind speed:     " << _wind_speed << " m/s = "
              << _wind_speed*SG_MPS_TO_KT << " kt" << std::endl;
    std::cout << "  Wind direction: " << _wind_direction << " degrees"
              << std::endl;
    std::cout << "  Snow level:     " << _snow_level << " m." << std::endl;
    std::cout << "  Inland water bodies ice cover: "
                 << _inland_ice_cover ? "yes" : "no" << std::endl;
    std::cout << "  Snow thickness.(0.0 = thin .. 1.0 = thick): "
              << _snow_thickness << std::endl;
    std::cout << "  Ice cover......(0.0 = none .. 1.0 = thick): " << _ice_cover
              << std::endl;
    std::cout << "  Dust cover.....(0.0 = none .. 1.0 = dusty): " << _dust_cover
              << std::endl;
    std::cout << "  Wetness........(0.0 = dry  .. 1.0 = wet):   " << _wetness
              << std::endl;
    std::cout << "  Lichen cover...(0.0 = none .. 1.0 = mossy): "
              << _lichen_cover << std::endl;
    std::cout << "  Season (0.0 = summer .. 1.0 = late autumn): "
              << ((_gl.has_autumn && _is_autumn > 0.05) ? _season_transistional : 0.0)
              << std::endl << std::endl;
    std::cout << "  METAR: " << get_metar() << std::endl;
    std::cout << "===============================================" << std::endl;
}
#endif // REPORT_TO_CONSOLE
