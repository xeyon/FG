#pragma once

const int SCHEMA_VERSION = 21;

#define SCHEMA_SQL                                                                              \
    "CREATE TABLE properties (key VARCHAR, value VARCHAR);"                                     \
    "CREATE TABLE stat_cache (path VARCHAR unique, stamp INT, sha VARCHAR);"                    \
                                                                                                \
    "CREATE TABLE positioned (type INT, ident VARCHAR collate nocase,"                          \
    "name VARCHAR collate nocase, airport INT64, lon FLOAT, lat FLOAT,"                         \
    "elev_m FLOAT, octree_node INT, cart_x FLOAT, cart_y FLOAT, cart_z FLOAT);"                 \
                                                                                                \
    "CREATE INDEX pos_octree ON positioned(octree_node);"                                       \
    "CREATE INDEX pos_ident ON positioned(ident collate nocase);"                               \
    "CREATE INDEX pos_name ON positioned(name collate nocase);"                                 \
    "CREATE INDEX pos_apt_type ON positioned(airport, type);"                                   \
                                                                                                \
    "CREATE TABLE airport (has_metar BOOL);"                                                    \
    "CREATE TABLE comm (freq_khz INT,range_nm INT);"                                            \
    "CREATE INDEX comm_freq ON comm(freq_khz);"                                                 \
                                                                                                \
    "CREATE TABLE runway (heading FLOAT, length_ft FLOAT, width_m FLOAT,"                       \
    "surface INT, displaced_threshold FLOAT,stopway FLOAT,reciprocal INT64,ils INT64);"         \
    "CREATE TABLE navaid (freq INT,range_nm INT,multiuse FLOAT, runway INT64,colocated INT64);" \
    "CREATE INDEX navaid_freq ON navaid(freq);"                                                 \
                                                                                                \
    "CREATE TABLE octree (children INT);"                                                       \
                                                                                                \
    "CREATE TABLE airway (ident VARCHAR collate nocase, network INT);"                          \
    "CREATE INDEX airway_ident ON airway(ident);"                                               \
                                                                                                \
    "CREATE TABLE airway_edge (network INT,airway INT64,a INT64,b INT64);"                      \
    "CREATE INDEX airway_edge_from ON airway_edge(a);"                                          \
    "CREATE INDEX airway_edge_to ON airway_edge(b);"
