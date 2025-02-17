#!/usr/bin/node

/*
 * This file is part of Hootenanny.
 *
 * Hootenanny is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * The following copyright notices are generated automatically. If you
 * have a new notice to add, please use the format:
 * " * @copyright Copyright ..."
 * This will properly maintain the copyright information. Maxar
 * copyrights will be updated automatically.
 *
 * @copyright Copyright (C) 2021 Maxar (http://www.maxar.com/)
 */

//
// Functions and setup for checking translations
//

var assert = require('assert'),
  // http = require('http'),
  // xml2js = require('xml2js'),
  // fs = require('fs'),
  // httpMocks = require('node-mocks-http'),
  osmtogeojson = require('osmtogeojson'),
  DOMParser = new require('@xmldom/xmldom').DOMParser;
parser = new DOMParser();

var HOOT_HOME = process.env.HOOT_HOME;

console.log('HOOT_HOME is ' + HOOT_HOME);

var hoot = require(HOOT_HOME + '/lib/HootJs');

var server = require(HOOT_HOME + '/translations/TranslationServer.js');


function translateFcode(schema,f_code,geom = '')
{
  try {
    var feature = server.handleInputs({
        fcode: f_code,
        translation: schema,
        geom: geom,
        method: 'GET',
        path: '/translateFrom'
    });
    return feature;
  }
  catch (err) {
    return {'error':err};
  }
};


function getFcodesForGeom(schema,geom)
{
  try {
    var feature = server.getFCodes({
        translation: schema,
        geometry: geom,
        method: 'GET',
        path: '/translateFrom'
    });
    return feature;
  }
  catch (err) {
    return {'error':err};
  }
};


// Handy Info:
// translateTo:
//    key | idelem = key to search for
//    value | idvalue = value to search for
//    geom = the geometry: Point|Vertex, Area, Line
//    translation = what to translate to
function schemaFromFcode(F_CODE,geometry,schema)
{
  try {
    var feature = server.handleInputs({
      idelem: 'fcode',
      idval: F_CODE,
      geom: geometry,
      translation: schema,
      method: 'GET',
      path: '/translateTo'
    });
    // console.log(schema + ': ' + code + ' ' + geo + ' ' + feature.name + '  ' + feature.desc);
    return feature;
  }
  catch (err) {
    return {'error':err};
  }
}; // End schemaFromFcode


function osmToOgr(feature,schema)
{
  try {
    var ogrXML = server.handleInputs({
      osm: feature,
      method: 'POST',
      translation: schema,
      path: '/translateTo'
    });
    return ogrXML;
  }
  catch (err) {
    return err;
  }
}; // End osmToOgr


function ogrToOsm(feature,schema)
{
  try {
    var osmXML = server.handleInputs({
      osm: feature,
      method: 'POST',
      translation: schema,
      path: '/translateFrom'
    });
    return osmXML;
  }
  catch (err) {
    return err;
  }
}; // End ogrToOsm


function makeJson(OSM)
{
  var osmJson = osmtogeojson(parser.parseFromString(OSM)).features[0].properties;
  delete osmJson.timestamp;
  delete osmJson.version;
  delete osmJson.id;

  // MGCP vs TDS
  if (osmJson.FCODE)
  {
    osmJson.F_CODE = osmJson.FCODE;
    delete osmJson.FCODE;
  }

  // return JSON.stringify(osmJson,Object.keys(osmJson).sort());
  return osmJson;
};


function getTranslationsList()
{
  try {
    var feature = server.handleInputs({
      method: 'GET',
      path: '/getTranslations'
    });
    // console.log(schema + ': ' + code + ' ' + geo + ' ' + feature.name + '  ' + feature.desc);
    return feature;
  }
  catch (err) {
    return {'error':err};
  }
}; // End getTranslationsList


// Wrapper for 'capabilities', 'translations','version' etc'
function getThing(thing)
{
  try {
    var feature = server.handleInputs({
      method: 'GET',
      path: '/' + thing
    });
    return feature;
  }
  catch (err) {
    return {'error':err};
  }
}; // End thing


// Console.log version of translate.dumpSchema
function dumpSchema(schema)
{
  schema.forEach( function(item) {
    console.log('Feature: ' + item.name + '  Geom: ' + item.geom + '  FdName: ' + item.fdname);

    item.columns.forEach( function (column) {
      console.log('    Attr: ' + column.name + '  Desc: ' + column.desc + '  Type: ' + column.type + '  Default: ' + column.defValue);
      if (column.type == 'enumeration')
      {
        column.enumerations.forEach( function (eValue) {console.log('        Value: ' + eValue.value + '  Name: ' + eValue.name); });
      }
    });

    console.log(''); // just to get one blank line
  });
}


// From the schema:
  // name: 'PAL015',
  // fcode: 'AL015',
  // desc: 'General Building',
  // definition: 'A relatively permanent structure, roofed and usually walled and designed for some particular use.',
  // geom: 'Point',

// Test a list of F_CODES against a list of schema and geometries
function testF_CODE(codeList,schemaList,geomList = ['Point','Line','Area'])
{
  codeList.forEach(code => {
    console.log('F_CODE: ' + code);
    geomList.forEach(geom => {
      schemaList.forEach(schema => {
        var feature = schemaFromFcode(code,geom,schema);
        var result = '' + code + ' ' + geom + ' in ' + schema + ' schema'
        if (feature.error) result = '### Not Valid: ' + result;
        console.log(result);
      }); // End schema
    }); // End geomList
  }); // End fCodeList
}

// Strings used to build OSM XML features
var startPoint = '<osm version="0.6" upload="true" generator="hootenanny">\
   <node id="-13" lat="0.68270797876" lon="18.45141400736" >';

var startLine = '<osm version="0.6" upload="true" generator="hootenanny">\
   <node id="-10" lat="0.68307256979" lon="18.45073925651" /> <node id="-13" lat="0.68270797876" lon="18.45141400736" />\
   <way id="-19" >\
   <nd ref="-10" /> <nd ref="-13" />';

var startArea = '<osm version="0.6" upload="true" generator="hootenanny">\
   <node id="-10" lat="0.68307256979" lon="18.45073925651" /> <node id="-11" lat="0.68341620728" lon="18.45091527847" />\
   <node id="-12" lat="0.68306209303" lon="18.45157116983" /> <node id="-13" lat="0.68270797876" lon="18.45141400736" />\
   <way id="-19" >\
   <nd ref="-10" /> <nd ref="-11" /> <nd ref="-12" /> <nd ref="-13" /> <nd ref="-10" />';

var endPoint = '</node></osm>';
var endLine = '</way></osm>'; // NOTE: This is also for Areas as well


// Test translating OGR to and from OSM
function testTranslated(schema,featureCode,tagList,geomList = ['Point','Line','Area'])
{
  console.log('---------------');
  var osmFeatures = {};

  osmFeatures.Point = startPoint + '<tag k="F_CODE" v="' + featureCode + '"/>';
  for (var tag in tagList) { osmFeatures.Point += "<tag k='" + tag + "' v='" + tagList[tag] + "'/>" }
  osmFeatures.Point += endPoint;

  osmFeatures.Line = startLine + '<tag k="F_CODE" v="' + featureCode + '"/>';
  for (var tag in tagList) { osmFeatures.Line += "<tag k='" + tag + "' v='" + tagList[tag] + "'/>" }
  osmFeatures.Line += endLine;

  osmFeatures.Area = startArea + '<tag k="F_CODE" v="' + featureCode + '"/>';
  for (var tag in tagList) { osmFeatures.Area += "<tag k='" + tag + "' v='" + tagList[tag] + "'/>" }
  osmFeatures.Area += endLine;

  for (var geom of geomList)
  {
    console.log('' + schema + '  ' + featureCode + '  ' + geom);

    // Raw input
    var inputJson = makeJson(osmFeatures[geom]);
    console.log('Raw: ' + JSON.stringify(inputJson,Object.keys(inputJson).sort()));

    var toOsm = ogrToOsm(osmFeatures[geom],schema);
    var osmJson = makeJson(toOsm);

    if (Object.keys(osmJson).length > 0)
    {
      // console.log(osmJson);
      console.log('OSM: ' + JSON.stringify(osmJson,Object.keys(osmJson).sort()));
    }
    else
    {
      console.log('  ## No OSM tags for ' + featureCode + ' ##');
    }

    var toOgr = osmToOgr(toOsm,schema);
    var ogrJson = makeJson(toOgr);

    if (Object.keys(ogrJson).length > 0)
    {
      // console.log(ogrJson);
      console.log('Ogr: ' + JSON.stringify(ogrJson,Object.keys(ogrJson).sort()));
    }
    else
    {
      console.log('  ## No Ogr tags for ' + featureCode + ' ##');
    }

    // Sanity Check
    var backToOsm = ogrToOsm(toOgr,schema);
    var secondJson = makeJson(backToOsm);
    // console.log(secondJson);
    console.log('OSM: ' + JSON.stringify(secondJson,Object.keys(secondJson).sort()));

    if (JSON.stringify(osmJson,Object.keys(osmJson).sort()) !== JSON.stringify(secondJson,Object.keys(secondJson).sort()))
    {
      console.log('  ## Not Same OSM Tags: ' + ogrJson.F_CODE + ' vs ' + featureCode);
    }

    if (ogrJson.F_CODE !== featureCode)
    {
      console.log('  ## Not Same F_CODE: ' + ogrJson.F_CODE + ' vs ' + featureCode);
    }

    console.log('-----');
  } // End geom
}; // End testTranslated


// Test translating from OGR to OSM
function testToOSM(schema,featureCode,tagList,geomList = ['Point','Line','Area'])
{
  console.log('---------------');
  var osmFeatures = {};

  osmFeatures.Point = startPoint + '<tag k="F_CODE" v="' + featureCode + '"/>';
  for (var tag in tagList) { osmFeatures.Point += "<tag k='" + tag + "' v='" + tagList[tag] + "'/>" }
  osmFeatures.Point += endPoint;

  osmFeatures.Line = startLine + '<tag k="F_CODE" v="' + featureCode + '"/>';
  for (var tag in tagList) { osmFeatures.Line += "<tag k='" + tag + "' v='" + tagList[tag] + "'/>" }
  osmFeatures.Line += endLine;

  osmFeatures.Area = startArea + '<tag k="F_CODE" v="' + featureCode + '"/>';
  for (var tag in tagList) { osmFeatures.Area += "<tag k='" + tag + "' v='" + tagList[tag] + "'/>" }
  osmFeatures.Area += endLine;

  for (var geom of geomList)
  {
    console.log('' + schema + '  ' + featureCode + '  ' + geom);

    // Raw input
    var inputJson = makeJson(osmFeatures[geom]);
    console.log('Raw: ' + JSON.stringify(inputJson,Object.keys(inputJson).sort()));

    var toOsm = ogrToOsm(osmFeatures[geom],schema);
    var osmJson = makeJson(toOsm);

    if (Object.keys(osmJson).length > 0)
    {
      // console.log(osmJson);
      console.log('OSM: ' + JSON.stringify(osmJson,Object.keys(osmJson).sort()));
    }
    else
    {
      console.log('  ## No OSM tags for ' + featureCode + ' ##');
    }

    var toOgr = osmToOgr(toOsm,schema);
    var ogrJson = makeJson(toOgr);

    if (Object.keys(ogrJson).length > 0)
    {
      // console.log(ogrJson);
      console.log('Ogr: ' + JSON.stringify(ogrJson,Object.keys(ogrJson).sort()));
    }
    else
    {
      console.log('  ## No Ogr tags for ' + featureCode + ' ##');
    }

    // Sanity Check
    var backToOsm = ogrToOsm(toOgr,schema);
    var secondJson = makeJson(backToOsm);
    // console.log(secondJson);
    console.log('OSM: ' + JSON.stringify(secondJson,Object.keys(secondJson).sort()));

    if (JSON.stringify(osmJson,Object.keys(osmJson).sort()) !== JSON.stringify(secondJson,Object.keys(secondJson).sort()))
    {
      console.log('  ## Not Same OSM Tags: ' + ogrJson.F_CODE + ' vs ' + featureCode);
    }

    if (ogrJson.F_CODE !== featureCode)
    {
      console.log('  ## Not Same F_CODE: ' + ogrJson.F_CODE + ' vs ' + featureCode);
    }

    console.log('-----');
  } // End geom
}; // End testToOSM


// Test translating OSM to and from OGR
function testOSM(schema,tagList,geomList = ['Point','Line','Area'])
{
  console.log('---------------');

  var osmFeatures = {};

  osmFeatures.Point = startPoint;
  for (var tag in tagList) { osmFeatures.Point += "<tag k='" + tag + "' v='" + tagList[tag] + "'/>" }
  osmFeatures.Point += endPoint;

  osmFeatures.Line = startLine;
  for (var tag in tagList) { osmFeatures.Line += "<tag k='" + tag + "' v='" + tagList[tag] + "'/>" }
  osmFeatures.Line += endLine;

  osmFeatures.Area = startArea;
  for (var tag in tagList) { osmFeatures.Area += "<tag k='" + tag + "' v='" + tagList[tag] + "'/>" }
  osmFeatures.Area += endLine;

  for (var geom of geomList)
  {
    console.log('' + schema + '  ' + geom + ':');

    // Raw input
    var inputJson = makeJson(osmFeatures[geom]);
    console.log('Raw: ' + JSON.stringify(inputJson,Object.keys(inputJson).sort()));

    var toOgr = osmToOgr(osmFeatures[geom],schema);
    var ogrJson = makeJson(toOgr);

    if (Object.keys(ogrJson).length > 0)
    {
      // console.log(ogrJson);
      console.log('Ogr: ' + JSON.stringify(ogrJson,Object.keys(ogrJson).sort()));
    }
    else
    {
      console.log('  ## No Ogr tags for ' + featureCode + ' ##');
    }

    // Sanity Check
    var backToOsm = ogrToOsm(toOgr,schema);
    var secondJson = makeJson(backToOsm);
    // console.log(secondJson);
    console.log('OSM: ' + JSON.stringify(secondJson,Object.keys(secondJson).sort()));

    if (JSON.stringify(inputJson,Object.keys(inputJson).sort()) !== JSON.stringify(secondJson,Object.keys(secondJson).sort()))
    {
      console.log('  ## Not Same OSM Tags');
    }

    var backToOgr = osmToOgr(backToOsm,schema);
    var thirdJson = makeJson(backToOgr);

    if (Object.keys(thirdJson).length > 0)
    {
      // console.log(ogrJson);
      console.log('Ogr: ' + JSON.stringify(thirdJson,Object.keys(thirdJson).sort()));
    }
    else
    {
      console.log('  ## No Ogr tags ##');
    }

    console.log('-----');
  } // End geom
}; // End testOSM

// Test translating from OSM to OGR
function testToOGR(schema,tagList,geomList = ['Point','Line','Area'])
{
  console.log('---------------');

  var osmFeatures = {};

  osmFeatures.Point = startPoint;
  for (var tag in tagList) { osmFeatures.Point += "<tag k='" + tag + "' v='" + tagList[tag] + "'/>" }
  osmFeatures.Point += endPoint;

  osmFeatures.Line = startLine;
  for (var tag in tagList) { osmFeatures.Line += "<tag k='" + tag + "' v='" + tagList[tag] + "'/>" }
  osmFeatures.Line += endLine;

  osmFeatures.Area = startArea;
  for (var tag in tagList) { osmFeatures.Area += "<tag k='" + tag + "' v='" + tagList[tag] + "'/>" }
  osmFeatures.Area += endLine;

  for (var geom of geomList)
  {
    console.log('' + schema + '  ' + geom + ':');

    // Raw input
    var inputJson = makeJson(osmFeatures[geom]);
    console.log('Raw: ' + JSON.stringify(inputJson,Object.keys(inputJson).sort()));

    var toOgr = osmToOgr(osmFeatures[geom],schema);
    var ogrJson = makeJson(toOgr);

    if (Object.keys(ogrJson).length > 0)
    {
      // console.log(ogrJson);
      console.log('Ogr: ' + JSON.stringify(ogrJson,Object.keys(ogrJson).sort()));
    }
    else
    {
      console.log('  ## No Ogr tags for ' + featureCode + ' ##');
    }
    console.log('-----');
  } // End geom
}; // End testToOGR


// Go through a schema and test each feature
function testSchema(schemaMap)
{
  for (var schema in schemaMap)
  {
    console.log('---------------');
    schemaMap[schema].getDbSchema().forEach(feature => {
      console.log(schema + '  F_CODE: ' + feature.fcode + '  Geom: ' + feature.geom + '  Desc: ' + feature.desc);

      var osmFeatures = {};

      osmFeatures.Point = startPoint + '<tag k="F_CODE" v="' + feature.fcode + '"/>' + endPoint;
      osmFeatures.Line = startLine + '<tag k="F_CODE" v="' + feature.fcode + '"/>' + endLine;
      osmFeatures.Area = startArea + '<tag k="F_CODE" v="' + feature.fcode + '"/>' + endLine;

      var toOsm = ogrToOsm(osmFeatures[feature.geom],schema);
      var osmJson = makeJson(toOsm);

      if (Object.keys(osmJson).length > 0)
      {
        // console.log(osmJson);
        console.log('OSM: ' + JSON.stringify(osmJson,Object.keys(osmJson).sort()));
      }
      else
      {
        console.log('  ## No OSM tags for ' + feature.fcode + ' ##');
      }

      var toOgr = osmToOgr(toOsm,schema);
      var ogrJson = makeJson(toOgr);


      if (Object.keys(ogrJson).length > 0)
      {
        if (Object.keys(ogrJson).length > 1)
        {
          console.log('Ogr Multi: ' + JSON.stringify(ogrJson,Object.keys(ogrJson).sort()));
        }
        else
        {
          console.log('Ogr: ' + JSON.stringify(ogrJson,Object.keys(ogrJson).sort()));
        }
      }
      else
      {
        console.log('  ## No Ogr tags for ' + feature.fcode + ' ##');
      }

      // Sanity Check
      var backToOsm = ogrToOsm(toOgr,schema);
      var secondJson = makeJson(backToOsm);
      // console.log(secondJson);
      console.log('OSM: ' + JSON.stringify(secondJson,Object.keys(secondJson).sort()));

      if (JSON.stringify(osmJson,Object.keys(osmJson).sort()) !== JSON.stringify(secondJson,Object.keys(secondJson).sort()))
      {
        console.log('  ## Not Same OSM Tags: ' + ogrJson.F_CODE + ' vs ' + feature.fcode);
      }


      if (ogrJson.F_CODE !== feature.fcode)
      {
        console.log('  ## Not Same F_CODE: ' + ogrJson.F_CODE + ' vs ' + feature.fcode);
      }

      console.log('-----');

    }); // End single schema
  }; // End schemaMap
}; // End testSchema


// Dump out the F_CODES with a particular attribute.
// If it is enumerzted, dump the values as well
var dumpValues = function (schema,aName)
{
  console.log('---------------');
  var enumList = {};

  schema.getDbSchema().forEach(feature => {

    feature['columns'].forEach(attr => {
      if (attr.name == aName)
      {
        console.log('F_CODE: ' + feature.fcode + '  Geom: ' + feature.geom + '  Desc: ' + feature.desc);
        if (attr['type'] == 'enumeration')
        {
          attr['enumerations'].forEach(enValue => {
            console.log(' name:' + enValue.name + '  Value:' + enValue.value);
            if (!enumList[enValue.value]) enumList[enValue.value] = enValue.name;
          });
        }
        else
        {
          console.log(' name:' + aName + '  Type:' + attr['type']);
        }
        console.log('-----');
      }
    });
  }); // End single schema
}; // End dumpValues


if (typeof exports !== 'undefined') {
    exports.translateFcode = translateFcode;
    exports.getFcodesForGeom = getFcodesForGeom;
    exports.schemaFromFcode = schemaFromFcode;
    exports.osmToOgr = osmToOgr;
    exports.ogrToOsm = ogrToOsm;
    exports.makeJson = makeJson;
    exports.testF_CODE = testF_CODE;
    exports.testTranslated = testTranslated;
    exports.testOSM = testOSM;
    exports.testToOSM = testToOSM;
    exports.testToOGR = testToOGR;
    exports.testSchema = testSchema;
    exports.dumpValues = dumpValues;
    exports.dumpSchema = dumpSchema;
    exports.getThing = getThing;
}
