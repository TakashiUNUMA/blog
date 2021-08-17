/**
@example decode_bufr_sample.c
@english
Decode BUFR messages in a file to text data

@verbatim
gcc -Wall -O2 -static -I/[path-to-libecbufr-installed-dir]/include -o decode_bufr_sample decode_bufr_sample.c -L/[path-to-libecbufr-installed-dir]/lib -lecbufr -lm
BUFR_TABLES=. ./decode_bufr_sample ./Z__C_RJTD_20200214000000_OBS_AMDS_Rjp_N2_bufr4.bin
@endverbatim

Sample data could be obtained from the following URL:http://www.jmbsc.or.jp/jp/online/c-onlineGsample.html

@endenglish
*/

#include <stdio.h>
#include <stdlib.h>
#include "bufr_api.h"

static void bufr_show_dataset( BUFR_Dataset *dts, char *filename );
/*
 * define these string and point them to a valid BUFR TABLE B file and TABLE D file
 * in order to load other local tables
 */
static char *str_ltableb = NULL;
static char *str_ltabled = NULL;

int main(int argc, char *argv[]){
  BUFR_Dataset  *dts;
  FILE          *fp;
  BUFR_Tables   *tables = NULL;
  BUFR_Message  *msg;
  int            rtrn;
  int            count;
  char           filename[256];

  // load CMC Table B and D
  //  includes local descriptors
  tables = bufr_create_tables();
  bufr_load_cmc_tables( tables );  
  // load local tables if any 
  if (str_ltableb) bufr_load_l_tableB( tables, str_ltableb );
  if (str_ltabled) bufr_load_l_tableD( tables, str_ltabled );
  // open a file for reading
  fp = fopen( argv[1], "r" );
  if (fp == NULL){
    bufr_free_tables( tables );
    fprintf( stderr, "Error: can't open file \"%s\"\n", argv[1] );
    exit(-1);
  }
  // read all messages from the input file
  count = 0;
  // a new instance of BUFR_Message is assigned to msg at each invocation
  while ( (rtrn = bufr_read_message( fp, &msg )) > 0 ){
    ++count;
    // BUFR_Message ==> BUFR_Dataset 
    //  decode the message using the BUFR Tables
    dts = bufr_decode_message( msg, tables ); 
    if (dts == NULL){
      fprintf( stderr, "Error: can't decode messages #%d\n", count );
      continue;
    }
    // dump the content of the Message into a file
    sprintf( filename, "./OUTPUT.TXT" );
    bufr_show_dataset( dts, filename );
    // discard the message
    bufr_free_message( msg );
    // discard an instance of the Decoded message pointed to by dts
    bufr_free_dataset( dts );
  }
  // close all file and cleanup
  fclose( fp );

  bufr_free_tables( tables );
}

// print out the content of a Dataset by going through each DataSubset contained inside it.
static void bufr_show_dataset( BUFR_Dataset *dts, char *filename ){
  DataSubset     *subset;
  int             sscount, cvcount;
  int             i, j;
  BufrDescriptor *bcv;
  FILE           *fp;

  fp = fopen( filename, "w" );
  if (fp == NULL){
    fprintf( stderr, "Error: can't open the output file: %s\n", filename );
    exit(-1);
  }
  // see how many DataSubset are present in the Dataset, this is an extract from section3 but actually kept as an array size
  sscount = bufr_count_datasubset( dts );

  float lat[sscount];
  float lon[sscount];
  float data1[sscount];
  float data2[sscount];
  int32_t aqc1[sscount];
  int32_t aqc2[sscount];
  // loop through each of them
  for (i = 0; i < sscount ; i++){
    // get a pointer of a DataSubset of each
    subset = bufr_get_datasubset( dts, i );
    // obtain the number of codes present in a Subset
    cvcount = bufr_datasubset_count_descriptor( subset );

    // 反復因子用フラグ
    int firstvisit_aqc = 0;
    int firstvisit_data = 0;
    for (j = 0; j < cvcount ; j++){
      // obtain a pointer to a BufrDescriptor structure which contains the descriptor and value
      bcv = bufr_datasubset_get_descriptor( subset, j );
      if (bcv->flags & FLAG_SKIPPED){
        //if (1202 == bcv->descriptor || 5001 == bcv->descriptor || 6001 == bcv->descriptor) fprintf( fp, "#  %.6d ", bcv->descriptor );
      } else {
        // 機関番号・緯度・経度・前10分間降水量・前1時間降水量の場合のみ処理
        if (bcv->value && (1202 == bcv->descriptor || 5001 == bcv->descriptor || 6001 == bcv->descriptor || 13011 == bcv->descriptor || 13019 == bcv->descriptor || 25211 == bcv->descriptor)){
          // 変数の型を判別しながら処理
          if (bcv->value->type == VALTYPE_INT32){
            int32_t value = bufr_descriptor_get_ivalue( bcv );
            if (25211 == bcv->descriptor){
              if (0 == firstvisit_aqc) {
                aqc1[i] = value;
                ++firstvisit_aqc;
              } else {
                aqc2[i] = value;
              }
            }
          } else if (bcv->value->type == VALTYPE_FLT64){
            double value = bufr_descriptor_get_dvalue( bcv );
            if (bufr_is_missing_double( value )){
              value = -9999.;
            }
            if (5001 == bcv->descriptor) lat[i] = value;
            if (6001 == bcv->descriptor) lon[i] = value;
            if (13011 == bcv->descriptor || 13019 == bcv->descriptor){
              if (0 == firstvisit_data) {
                data1[i] = value;
                ++firstvisit_data;
              } else {
                data2[i] = value;
              }
            }
          }
        }
      }
    }
    fprintf( fp, "%f %f %f %d %f %d\n", lat[i],lon[i],data1[i],aqc1[i],data2[i],aqc2[i]);
  }
  fclose ( fp );
}
