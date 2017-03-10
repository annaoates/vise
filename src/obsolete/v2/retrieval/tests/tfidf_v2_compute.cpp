/*
==== Author:

Relja Arandjelovic (relja@robots.ox.ac.uk)
Visual Geometry Group,
Department of Engineering Science
University of Oxford

==== Copyright:

The library belongs to Relja Arandjelovic and the University of Oxford.
No usage or redistribution is allowed without explicit permission.
*/

#include <stdint.h>
#include <string>
#include <vector>

#include "index_entry.pb.h"
#include "proto_db.h"
#include "proto_db_file.h"
#include "proto_index.h"
#include "tfidf.h"
#include "tfidf_data.pb.h"
#include "tfidf_v2.h"



int main(){
    
    #if 0
    std::string iidxFn= "/home/relja/Relja/Data/tmp/indexing_v2/iidx_oxc1_5k_hesaff_sift_hell_1000000_43.v2bin";
    std::string fidxFn= "/home/relja/Relja/Data/tmp/indexing_v2/fidx_oxc1_5k_hesaff_sift_hell_1000000_43.v2bin";
    std::string wghtFn= "/home/relja/Relja/Data/tmp/indexing_v2/wght_oxc1_5k_hesaff_sift_hell_1000000_43.v2bin";
    #else
    std::string iidxFn= "/home/relja/Relja/Data/tmp/indexing_v2/iidx_gmy_oxc1_5k_hesaff_sift_hell_1000000_43.v2bin";
    std::string fidxFn= "/home/relja/Relja/Data/tmp/indexing_v2/fidx_gmn_oxc1_5k_hesaff_sift_hell_1000000_43.v2bin";
    std::string wghtFn= "/home/relja/Relja/Data/tmp/indexing_v2/wght_iidxgm_oxc1_5k_hesaff_sift_hell_1000000_43.v2bin";
    #endif
    
    protoDbFile dbFidx_file(fidxFn);
    protoDbInRam dbFidx(dbFidx_file);
    protoIndex fidx(dbFidx, false);
    
    protoDbFile dbIidx_file(iidxFn);
    protoDbInRam dbIidx(dbIidx_file);
    protoIndex iidx(dbIidx, false);
    
    tfidfV2 tfidfObj(&iidx, &fidx, wghtFn);
    
    if (false) {
        rr::tfidfData data1, data2;
        
        {
            std::ifstream in("/home/relja/Relja/Data/tmp/indexing_v2/wght_oxc1_5k_hesaff_sift_hell_1000000_43.v2bin", std::ios::binary);
            ASSERT(data1.ParseFromIstream(&in));
            in.close();
        }
        
        {
            std::ifstream in("/home/relja/Relja/Data/tmp/indexing_v2/wght_iidxgm_oxc1_5k_hesaff_sift_hell_1000000_43.v2bin", std::ios::binary);
            ASSERT(data2.ParseFromIstream(&in));
            in.close();
        }
        
        tfidf tfidfV1(NULL, NULL, "/home/relja/Relja/Data/ox_exp/wght_oxc1_5k_hesaff_sift_hell_1000000_43.rr.bin");
        
        ASSERT( data1.idf_size() == data2.idf_size());
        ASSERT( data1.docl2_size() == data2.docl2_size());
        
        for (int i= 0; i < data1.idf_size(); ++i){
            ASSERT( fabs(data1.idf(i) - data2.idf(i)) < 1e-7 );
            ASSERT( fabs(data1.idf(i) - tfidfV1.idf[i]) < 1e-6 );
        }
        
        for (int i= 0; i < data1.docl2_size(); ++i){
            ASSERT( fabs(data1.docl2(i) - tfidfV1.docL2[i]) < 1e-3 );
            ASSERT( fabs(data1.docl2(i) - data2.docl2(i)) < 1e-3 );
        }
    }
    
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}
