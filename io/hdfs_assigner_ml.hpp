#pragma once

#include "hdfs/hdfs.h"

#include "husky/master/hdfs_assigner.hpp"
#include "husky/master/master.hpp"
#include "husky/core/context.hpp"

#include "core/constants.hpp"

namespace husky {

class HDFSBlockAssignerML : public HDFSBlockAssigner {
   public: 
    HDFSBlockAssignerML() {
        Master::get_instance().register_main_handler(constants::kIOHDFSSubsetLoad, std::bind(&HDFSBlockAssignerML::master_main_handler_ml, this));
    }

    void master_main_handler_ml() {
        auto& master = Master::get_instance();
        auto master_socket = master.get_socket();
        std::string url, host, load_type;
        int num_threads, id; 
        BinStream stream = zmq_recv_binstream(master_socket.get());
        stream >> url >> host >> num_threads >> id >> load_type;
        
        // reset num_worker_alive
        num_workers_alive = num_threads;

        std::pair<std::string, size_t> ret = answer(host, url, id, load_type);
        stream.clear();
        stream << ret.first << ret.second;

        zmq_sendmore_string(master_socket.get(), master.get_cur_client());
        zmq_sendmore_dummy(master_socket.get());
        zmq_send_binstream(master_socket.get(), stream);
    }

    void browse_hdfs(int id, const std::string& url) {
        if (!fs_)
            return;

        int num_files;
        int dummy;
        hdfsFileInfo* file_info = hdfsListDirectory(fs_, url.c_str(), &num_files);
        auto& files_locality = files_locality_multi_dict[id][url];
        for (int i = 0; i < num_files; ++i) {
            // for every file in a directory
            if (file_info[i].mKind != kObjectKindFile)
                continue;
            size_t k = 0;
            while (k < file_info[i].mSize) {
                // for every block in a file
                auto blk_loc = hdfsGetFileBlockLocations(fs_, file_info[i].mName, k, 1, &dummy);
                for (int j = 0; j < blk_loc->numOfNodes; ++j) {
                    // for every replication in a block
                    files_locality[blk_loc->hosts[j]].insert(
                        BlkDesc{std::string(file_info[i].mName) + '\0', k, std::string(blk_loc->hosts[j])});
                    // init finish_multi_dict, 0 means none thread has been rejected by this (url, host)
                    (finish_multi_dict[id][url].first)[blk_loc->hosts[j]] = 0;
                }
                k += file_info[i].mBlockSize;
            }
        }
        hdfsFreeFileInfo(file_info, num_files);
    }

    std::pair<std::string, size_t> answer(const std::string& host, const std::string& url, int id, const std::string& load_type) {
        if (!fs_)
            return {"", 0};

        // cannot find id
        if (files_locality_multi_dict.find(id) == files_locality_multi_dict.end()) {
            browse_hdfs(id, url);
            // count the url accessing times
            finish_multi_dict[id][url].second = 0;
        }

        // cannot find url
        if (files_locality_multi_dict[id].find(url) == files_locality_multi_dict[id].end()) {
            browse_hdfs(id, url);
            finish_multi_dict[id][url].second = 0;
        }
        
        // selected_file, offset
        std::pair<std::string, size_t> ret = {"", 0};

        auto& all_files_locality = files_locality_multi_dict[id][url];
        /*
         * selected_host has two situation: 
         * 1. when load_locally, selected_host always equals host, or null
         * 2. when load_globally, 
         *     when loading local host, selected_host equals host, 
         *     when loading gloabl host, selected_host equals an unfinished host 
         */
        std::string selected_host;
        if (load_type.empty() || load_type == husky::constants::kLoadHdfsGlobally) { // default is load data globally
            // when loading data globally, util that all hosts finished means finishing
            bool is_finish = true;
            for(auto& item:all_files_locality) {
                // when loading globally, any host havingn't been finished means unfinished  
                if (item.second.size() != 0) {
                    is_finish = false;
                    // in fact, there can be optimizing. for example, everytime, we can calculate the host
                    // which has the longest blocks. It may be good for load balance but search is time-consuming
                    selected_host = item.first;   // default allocate a unfinished host block
                    break;
                }
            }

            if (is_finish) {
                finish_multi_dict[id][url].second += 1;
                if(finish_multi_dict[id][url].second == num_workers_alive) {
                    // this means all workers's requests about this url are rejected
                    // blocks under this url are all allocated
                    files_locality_multi_dict[id].erase(url); 
                }
                return {"", 0};
            } else {
                // selected_file
                if (all_files_locality[host].size()) {  // if there is local file, allocate local file
                    selected_host = host;
                }
            }
        } else {    // load data globally
            auto& local_files_locality = all_files_locality[host];

            if (local_files_locality.size() == 0) {     // local data is empty
                (finish_multi_dict[id][url].first)[host] += 1;
                if ((finish_multi_dict[id][url].first)[host] == num_workers_alive) {
                    files_locality_multi_dict[id][url].erase(host);
                }
                return {"", 0};
            }

            // local host has data, so set host to seletced_host 
            selected_host = host;   
        }

        // according selected_host to get the select file
        auto selected_file = all_files_locality[selected_host].begin();
        // select
        ret = {selected_file->filename, selected_file->offset};

        // remove
        all_files_locality[selected_host].erase(selected_file);
        
        return ret;
    }

   private:
    /*
     * files_locality_multi_dict is a map about hdfs info
     * {
     *     {task_id: {{url: {host:[{filename,offset,block_location}, {filename,offset,block_location}...]}},....}},...
     * }
     */
    // std::map<int, std::map<std::string, std::unordered_set<BlkDesc>>> files_locality_multi_dict;
    std::map<size_t, std::map<std::string, std::map<std::string, std::unordered_set<BlkDesc>>>> files_locality_multi_dict;
    /*
     * finish_multi_dict describes each available thread has request the url, and response null
     * if none thread can get anything from an url, this means this url has dispensed all block, this url can be removed
     * {
     *     {task_id:{{url: {{host: count},count_num},...}},...
     * }
     */
    // std::map<int, std::map<std::string, int>> finish_multi_dict;
    // std::map<int, std::map<std::string, std::map<std::string, int>>> finish_multi_dict;
    std::map<size_t, std::map<std::string, std::pair<std::map<std::string, size_t>, size_t>>> finish_multi_dict;
};

}
