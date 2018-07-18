#include "vise/search_engine_manager.h"

search_engine_manager *search_engine_manager::search_engine_manager_ = NULL;

search_engine_manager* search_engine_manager::instance() {
  if ( !search_engine_manager_ ) {
    search_engine_manager_ = new search_engine_manager;
  }
  return search_engine_manager_;
}

void search_engine_manager::init(const boost::filesystem::path data_dir) {
  data_dir_ = data_dir;
  BOOST_LOG_TRIVIAL(debug) << "search_engine_manager::init() : data_dir=" << data_dir_.string() << flush;

  search_engine_index_thread_running_ = false;
  now_search_engine_index_state_.clear();

  search_engine_index_state_name_list_.clear();
  search_engine_index_state_name_list_.push_back("relja_retrival:trainDescs");
  search_engine_index_state_name_list_.push_back("relja_retrival:cluster");
  search_engine_index_state_name_list_.push_back("relja_retrival:trainAssign");
  search_engine_index_state_name_list_.push_back("relja_retrival:trainHamm");
  search_engine_index_state_name_list_.push_back("relja_retrival:index");
  search_engine_index_state_desc_list_.clear();
  search_engine_index_state_desc_list_.push_back("Computing image descriptors");
  search_engine_index_state_desc_list_.push_back("Clustering descriptors");
  search_engine_index_state_desc_list_.push_back("Assigning descriptors");
  search_engine_index_state_desc_list_.push_back("Computing embeddings");
  search_engine_index_state_desc_list_.push_back("Indexing");

  // automatically load a search engine based on user query
  auto_load_search_engine_ = true;
}

void search_engine_manager::process_cmd(const std::string search_engine_name,
                                        const std::string search_engine_version,
                                        const std::string search_engine_command,
                                        const std::map<std::string, std::string> uri_param,
                                        const std::string request_body,
                                        http_response& response) {
  if ( search_engine_command != "index_status" ) {
    BOOST_LOG_TRIVIAL(debug) << "processing search engine command: "
			     << search_engine_name << "," << search_engine_version
			     << "," << search_engine_command << ": payload="
			     << request_body.size() << " bytes";
  }

  if ( search_engine_command == "init" ) {
    if ( search_engine_exists(search_engine_name, search_engine_version) ) {
      response.set_status(400);
      response.set_field("Content-Type", "application/json");
      response.set_payload( "{\"error\":\"search engine already exists\"}" );
    } else {
      if ( request_body.length() ) {
        create_search_engine(search_engine_name, search_engine_version, request_body);
      } else {
        create_search_engine(search_engine_name, search_engine_version, "");
      }
      response.set_status(200);
      response.set_field("Content-Type", "application/json");
      std::ostringstream s;
      s << "{\"ok\":\"search engine created\", "
        << "\"search_engine_name\":\"" << search_engine_name << "\","
        << "\"search_engine_version\":\"" << search_engine_version << "\"}";

      response.set_payload( s.str() );
    }
    return;
  }

  if ( search_engine_command == "index_start" ) {
    if ( search_engine_index_thread_running_ ) {
      std::ostringstream s;
      s << "{\"error\":\"indexing ongoing for " << now_search_engine_name_
        << ":" << now_search_engine_version_ << "\"}";
      response.set_status(400);
      response.set_field("Content-Type", "application/json");
      response.set_payload( s.str() );
      return;
    }

    try {
      search_engine_index_thread_ = new boost::thread( boost::bind( &search_engine_manager::index_start, this, search_engine_name, search_engine_version ) );
      response.set_status(200);
      response.set_field("Content-Type", "application/json");
      response.set_payload("{\"ok\":\"indexing started\"}");
    } catch( std::exception &e ) {
      std::ostringstream s;
      s << "{\"error\":\"" << e.what() << "\"}";
      response.set_status(400);
      response.set_field("Content-Type", "application/json");
      response.set_payload( s.str() );
    }
    return;
  }

  if ( search_engine_command == "index_status" ) {
    if ( now_search_engine_index_state_.size() ) {
      std::ostringstream s;
      std::string name, desc;
      s << "[";
      for ( std::size_t i = 0; i < search_engine_index_state_name_list_.size(); ++i ) {
        name = search_engine_index_state_name_list_.at(i);
        desc = search_engine_index_state_desc_list_.at(i);
        if ( i != 0 ) {
          s << ",";
        }

        s << "{\"name\":\"" << name << "\","
          << "\"description\":\"" << desc << "\","
          << "\"state\":\"" << now_search_engine_index_state_[ name ] << "\"";
        if ( now_search_engine_index_state_[ name ] == "progress" ) {
          s << ",\"steps_done\":" << now_search_engine_index_steps_done_[ name ] << ","
            << "\"steps_count\":" << now_search_engine_index_steps_count_[ name ] << "";
        }
        s << "}";
      }
      s << "]";
      std::string state;
      response.set_status(200);
      response.set_field("Content-Type", "application/json");
      response.set_payload( s.str() );
    } else {
      std::string state;
      response.set_status(400);
      response.set_field("Content-Type", "application/json");
      response.set_payload( "{\"error\":\"indexing not started yet!\"}" );
    }
    return;
  }

  if ( search_engine_command == "engine_exists" ) {
    response.set_status(200);
    response.set_field("Content-Type", "application/json");
    if ( search_engine_exists(search_engine_name, search_engine_version) ) {
      response.set_payload( "{\"yes\":\"search engine exists\"}" );
    } else {
      response.set_payload( "{\"no\":\"search engine does not exist\"}" );
    }
    return;
  }

  // unhandled cases
  response.set_status(400);
}

void search_engine_manager::clear_now_search_engine_index_state(void) {
  now_search_engine_index_steps_done_.clear();
  now_search_engine_index_steps_count_.clear();
  now_search_engine_index_state_.clear();

  for ( std::size_t i = 0; i < search_engine_index_state_name_list_.size(); ++i ) {
    now_search_engine_index_state_[ search_engine_index_state_name_list_.at(i) ] = "not_started";
  }
}

bool search_engine_manager::index_start(const std::string search_engine_name,
                                        const std::string search_engine_version) {
  search_engine_index_thread_running_ = true;
  now_search_engine_name_ = search_engine_name;
  now_search_engine_version_ = search_engine_version;
  clear_now_search_engine_index_state();

  /*
  // @todo: find a way to automatically located these executables
  std::string index_exec   = "/home/tlm/dev/vise/bin/compute_index_v2";
  std::string cluster_exec = "/home/tlm/dev/vise/src/search_engine/relja_retrival/src/v2/indexing/compute_clusters.py";
  boost::filesystem::path config_fn = get_config_filename(search_engine_name, search_engine_version);

  std::ostringstream s;

  // 1. create image filename list
  boost::filesystem::path image_list_fn = get_image_list_filename(search_engine_name, search_engine_version);
  boost::filesystem::path image_data_dir = get_image_data_dir(search_engine_name, search_engine_version);
  s << "cd " << image_data_dir.string() << " && find *.jpg -print -type f > " << image_list_fn.string();

  // @todo: code is not portable for Windows
  std::system( s.str().c_str() );

  bool ok;

  // 2. extract descriptors
  s.str("");
  s.clear();
  s << "mpirun -np 8 " << index_exec
    << " trainDescs " << search_engine_name << " " << config_fn.string();
  now_search_engine_index_state_[ "relja_retrival:trainDescs" ] = "started";
  ok = run_shell_command( "relja_retrival:trainDescs", s.str() );
  if ( ! ok ) {
    search_engine_index_thread_running_ = false;
    return false;
  }

  // 3. cluster descriptors
  s.str("");
  s.clear();
  s << "mpirun -np 8 python " << cluster_exec
    << " " << search_engine_name << " " << config_fn.string() << " 8";
  now_search_engine_index_state_[ "relja_retrival:cluster" ] = "started";
  ok = run_shell_command( "relja_retrival:cluster", s.str() );
  if ( ! ok ) {
    search_engine_index_thread_running_ = false;
    return false;
  }

  // 4. trainAssign
  s.str("");
  s.clear();
  //s << "mpirun -np 8 " << index_exec // see docs/known_issues.txt
  s << index_exec
    << " trainAssign " << search_engine_name << " " << config_fn.string();
  now_search_engine_index_state_[ "relja_retrival:trainAssign" ] = "started";
  ok = run_shell_command( "relja_retrival:trainAssign", s.str() );
  if ( ! ok ) {
    search_engine_index_thread_running_ = false;
    return false;
  }

  // 4. trainHamm
  s.str("");
  s.clear();
  //s << "mpirun -np 8 " << index_exec
  s << index_exec
    << " trainHamm " << search_engine_name << " " << config_fn.string();
  now_search_engine_index_state_[ "relja_retrival:trainHamm" ] = "started";
  ok = run_shell_command( "relja_retrival:trainHamm", s.str() );
  if ( ! ok ) {
    search_engine_index_thread_running_ = false;
    return false;
  }

  // 5. index
  s.str("");
  s.clear();
  s << "mpirun -np 8 " << index_exec
    << " index " << search_engine_name << " " << config_fn.string();
  ok = run_shell_command( "relja_retrival:index", s.str() );
  if ( ! ok ) {
    search_engine_index_thread_running_ = false;
    return false;
  }

  search_engine_index_thread_running_ = false;
  return true;
}

bool search_engine_manager::run_shell_command(std::string cmd_name,
                                              std::string cmd) {
  boost::process::ipstream pipe;
  BOOST_LOG_TRIVIAL(debug) << "running command: {" << cmd_name << "} [" << cmd << "]";
  boost::process::child p(cmd, boost::process::std_out > pipe);
  std::string line;

  while ( pipe && std::getline(pipe, line) && !line.empty() ) {
    BOOST_LOG_TRIVIAL(debug) << "[" << cmd_name << "] " << line;
    if ( vise::util::starts_with(line, "relja_retrival,") ) {
      //trainDescs:relja_retrival,trainDescsManager(images),2018-Jun-25 11:47:52,1,40
      // 0                       , 1                       , 2                  ,3,4
      std::vector<std::string> d = vise::util::split(line, ',');
      now_search_engine_index_steps_done_[ cmd_name ] = d[3];
      now_search_engine_index_steps_count_[cmd_name ] = d[4];
      now_search_engine_index_state_[ cmd_name ] = "progress";
    }
  }
  // @todo check if the process returned non-zero status code
  // @todo process error cases

  p.wait();
  if ( p.exit_code() ) {
    now_search_engine_index_state_[ cmd_name ] = "error";
    return false;
  } else {
    now_search_engine_index_state_[ cmd_name ] = "done";
    return true;
  }
  */
}

std::string search_engine_manager::get_unique_filename(std::string extension) {
  return boost::filesystem::unique_path("%%%%%%%%%%").string() + extension;
}

bool search_engine_manager::search_engine_exists(const std::string search_engine_name,
                                                 const std::string search_engine_version) {
  boost::filesystem::path search_engine_dir = data_dir_ / search_engine_name;
  search_engine_dir = search_engine_dir / search_engine_version;

  if ( boost::filesystem::exists(search_engine_dir) ) {
    return true;
  } else {
    return false;
  }
}

bool search_engine_manager::create_search_engine(const std::string search_engine_name,
                                                 const std::string search_engine_version,
                                                 const std::string search_engine_description) {
}

//
// search engine image i/o
//
bool search_engine_manager::add_image_from_http_payload(const boost::filesystem::path filename,
                                                        const std::string& request_body) {
  try {
    Magick::Blob blob(request_body.c_str(), request_body.size());

    if ( blob.length() ) {
      Magick::Image im(blob);
      im.magick("JPEG");
      im.colorSpace(Magick::sRGBColorspace);
      im.write(filename.string());
      BOOST_LOG_TRIVIAL(debug) << "written file [" << filename.string() << "]";
      return true;
    } else {
      BOOST_LOG_TRIVIAL(debug) << "failed to write file [" << filename.string() << "]";
      return false;
    }
  } catch( std::exception &e ) {
    BOOST_LOG_TRIVIAL(debug) << "exception occured while writing file [" << filename.string() << "] : " << e.what();
    return false;
  }
}

//
// search engine load/unload/maintenance
//
bool search_engine_manager::load_search_engine(std::string search_engine_name, std::string search_engine_version) {
  if ( ! search_engine_exists(search_engine_name, search_engine_version) ) {
    return false;
  }

  std::string search_engine_id = vise::relja_retrival::get_search_engine_id(search_engine_name, search_engine_version);

  boost::filesystem::path se_dir = data_dir_ / search_engine_id;
  vise::search_engine *se = new vise::relja_retrival(search_engine_id, se_dir);
  se->init();
  se->load();
  search_engine_list_.insert( std::pair<std::string, vise::search_engine*>(search_engine_id, se) );
  return true;
}

bool search_engine_manager::unload_all_search_engine() {
  // unload all the search engines from search_engine_list_
  std::map<std::string, vise::search_engine* >::iterator it;
  for ( it =  search_engine_list_.begin(); it !=  search_engine_list_.end(); ++it ) {
    BOOST_LOG_TRIVIAL(debug) << "unloading search engine: " << it->second->id();
    it->second->unload();
    delete it->second;
  }
}

//
// search engine query
//
void search_engine_manager::query(const std::string search_engine_name,
                                  const std::string search_engine_version,
                                  const std::map<std::string, std::string> uri_param,
                                  const std::string request_body,
                                  http_response& response) {
  std::string search_engine_id = vise::relja_retrival::get_search_engine_id(search_engine_name, search_engine_version);

  uint32_t file_id;
  std::stringstream ss;
  ss << uri_param.find("file_id")->second;
  ss >> file_id;

  ss.clear();
  ss.str("");
  ss << uri_param.find("region")->second;
  unsigned int x, y, w, h;
  char t;
  ss >> x >> t >> y >> t >> w >> t >> h;

  ss.clear();
  ss.str("");
  uint32_t from;
  ss << uri_param.find("from")->second;
  ss >> from;

  ss.clear();
  ss.str("");
  uint32_t to;
  ss << uri_param.find("to")->second;
  ss >> to;

  ss.clear();
  ss.str("");
  double score_threshold;
  ss << uri_param.find("score_threshold")->second;
  ss >> score_threshold;

  BOOST_LOG_TRIVIAL(debug) << "starting query ";
  search_engine_list_[ search_engine_id ]->query_using_file_region(file_id,
                                                                   x, y, w, h,
                                                                   from, to,
                                                                   score_threshold);
}


//
// search engine admin
//
void search_engine_manager::admin(const std::string command,
                                  const std::map<std::string, std::string> uri_param,
                                  const std::string request_body,
                                  http_response& response) {
  BOOST_LOG_TRIVIAL(debug) << "admin command: [" << command << "]";
}
