/*
 * bulk_extractor.cpp:
 * Feature Extraction Framework...
 *
 */

#include "bulk_extractor.h"
#include "image_process.h"
#include "threadpool.h"
#include "be13_api/aftimer.h"
#include "histogram.h"
#include "dfxml/src/dfxml_writer.h"
#include "dfxml/src/hash_t.h"

#include "phase1.h"

#include <dirent.h>
#include <ctype.h>
#include <fcntl.h>
#include <set>
#include <setjmp.h>
#include <vector>
#include <queue>
#include <unistd.h>
#include <ctype.h>

/****************************************************************
 *** 
 *** Here is the bulk_extractor API
 *** It is under development.
 ****************************************************************/

typedef int be_callback(int32_t flag,
                        uint32_t arg,
                        const char *feature_recorder_name,
                        const char *feature,size_t feature_len,
                        const char *context,size_t context_len);
    
/* The API relies on a special version of the feature recorder set
 * that writes to a callback function instead of a
 */

class callback_feature_recorder: public feature_recorder {
    // neither copying nor assignment are implemented
    callback_feature_recorder(const callback_feature_recorder &cfr);
    callback_feature_recorder &operator=(const callback_feature_recorder&cfr);
    be_callback *cb;
public:
    callback_feature_recorder(be_callback *cb_,
                              class feature_recorder_set &fs,string name_):
        feature_recorder(fs,"<no-outdir>","<no-fname>",name_),cb(cb_){
    }
    virtual std::string carve(const sbuf_t &sbuf,size_t pos,size_t len, 
                              const std::string &ext, // appended to forensic path
                        const struct be13::hash_def &hasher){
        return("");                     // no file created
    }
    virtual void open(){}               // we don't open
    virtual void close(){}               // we don't open
    virtual void flush(){}               // we don't open
    virtual void write(const std::string &str){
        cppmutex::lock lock(Mf);
        (*cb)(0,1,name.c_str(),str.c_str(),str.size(),"",0);
    }
    virtual void write0(const pos0_t &pos0,const std::string &feature,const std::string &context){
        cppmutex::lock lock(Mf);
        (*cb)(0,1,name.c_str(),feature.c_str(),feature.size(),context.c_str(),context.size());
    }
};

class callback_feature_recorder_set: public feature_recorder_set {
    // neither copying nor assignment are implemented
    callback_feature_recorder_set(const callback_feature_recorder_set &cfs);
    callback_feature_recorder_set &operator=(const callback_feature_recorder_set&cfs);
    be_callback *cb;

public:
    virtual feature_recorder *create_name_factory(const std::string &outdir_,
                                                  const std::string &input_fname_,
                                                  const std::string &name_){
        return new callback_feature_recorder(cb,*this,name_);
    }
    callback_feature_recorder_set(be_callback *cb_):feature_recorder_set(0),cb(cb_){
        feature_file_names_t feature_file_names;
        be13::plugin::get_scanner_feature_file_names(feature_file_names);
        init(feature_file_names,"cfrs_input","cfrs_outdir",0); // no histograms
    }
};


struct BEFILE_t {
    BEFILE_t(be_callback cb):fd(),cfs(cb),cfg(){};
    int         fd;
    callback_feature_recorder_set  cfs;
    BulkExtractor_Phase1::Config   cfg;
};

typedef struct BEFILE_t BEFILE;
extern "C" {
    void bulk_extractor_enable(const char *scanner_name)
    {
        be13::plugin::scanners_enable(scanner_name);
    }

    BEFILE *bulk_extractor_open(be_callback cb)
    {
        histograms_t histograms;
        feature_recorder::set_main_threadid();
        scanner_info::scanner_config   s_config; // the bulk extractor config
        be13::plugin::load_scanners(scanners_builtin,s_config);
        be13::plugin::scanners_process_enable_disable_commands();
        BEFILE *bef = new BEFILE_t(cb);

        /* How do we enable or disable individual scanners? */
        /* How do we set or not set a find pattern? */
        /* We want to disable carving, right? */
        /* How do we create the feature recorder with a callback? */
        return bef;
    }
    
    int bulk_extractor_analyze_buf(BEFILE *bef,uint8_t *buf,size_t buflen)
    {
        pos0_t pos0("");
        const sbuf_t sbuf(pos0,buf,buflen,buflen,false);
        be13::plugin::process_sbuf(scanner_params(scanner_params::PHASE_SCAN,sbuf,bef->cfs));
        return 0;
    }
    
    int bulk_extractor_close(BEFILE *bef)
    {
        bef->cfs.process_histograms(0);
        delete bef;
        return 0;
    }
}

    