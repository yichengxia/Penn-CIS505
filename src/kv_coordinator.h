#include <string>
#include <utility>
#include <thread>
#include <fstream>
#include <iostream>
#include <vector>
#include <csignal>

#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/client_context.h>

#include "src/protos/kvmaster.grpc.pb.h"
#include "src/common/common_util.h"
#include "src/kv/storage_client_coordinator.h"

using namespace std;
using namespace grpc;


bool status_run = true;

unique_ptr<Server> this_srvr;

class KVMasterImpl final : public KVMaster::Service, public KVMasterClient {
public:

    bool global_debugging=false;
    
    string addr;

    int serv_ID;

    explicit KVMasterImpl(const string &addr, vector<MasterPartitionView> &partitions)
            : KVMasterClient(partitions), addr(addr) {}


    void do_healthcheck() {
        
        int P_ID = 0;
        KVMasterClient::do_healthcheck();

        for (; P_ID<=partitions.size()-1; P_ID++) {

            
            auto &this_p = partitions[P_ID];
            int wrong_out=-1;

            bool cond1=(this_p.primary_idx==wrong_out);
            bool check_liveness=(this_p.nodes[this_p.primary_idx].alive);
            bool condd=(check_liveness&&(!cond1));
            if (condd)
            {
                continue;
            }

            bool flag_pm_renew;
            int ii = 0;
            flag_pm_renew=false;
            
            for (; ii<=this_p.nodes.size()-1; ii++) {
                auto &nn = this_p.nodes[ii];
                bool check_liveness2=(nn.alive);
                if (check_liveness2) {
                    flag_pm_renew=true;
                    this_p.primary_idx=ii;
                    break;
                }
            }

            if (flag_pm_renew) {
                // OK
            }
            else{
                logf("<Warning> Lacking living nodes!\n");
                this_p.primary_idx=wrong_out;
                continue;
            }

            vector<thread> vec_ths;
            auto &aaa=this_p.nodes[this_p.primary_idx].addr;
            for (auto &this_nn:this_p.nodes){
                vec_ths.emplace_back(&KVMasterClient::_pm_renewal,&this_nn,aaa);
            }

            logf("<Procress> Assigned new primary master!\n");

            for (auto &thr: vec_ths){
                thr.join();
            }
                
        }
    }

    Status GetPartitionPrimary(ServerContext *context, const GetPrimaryRequest *req, GetPrimaryResponse *resp) override {
        
        unsigned temp = req->partition_idx();
        bool check_lim=(temp<partitions.size());
        if  (check_lim){
            //OK
        }
        else {
            return Status(StatusCode::INVALID_ARGUMENT, "Out of bound partition nodes!\n");
        }

        unsigned p_ID = req->partition_idx();

        int wrong_out=-1;

        auto &ppp = partitions[p_ID];
        bool check_pmID=(ppp.primary_idx==wrong_out);
        if (!check_pmID) {
            auto &aaaa = ppp.nodes[ppp.primary_idx].addr;
            fprintf(stdout, "Setting node address!\n");
            resp->set_addr(aaaa);
            for (auto &this_n: ppp.nodes) {

                auto set_n = resp->add_nodes();
                fprintf(stdout, "Adding nodes!\n");
                set_n->set_alive(this_n.alive);
                set_n->set_address(this_n.addr);
                
            }
        }
        else {
            resp->set_addr("");
        }
        
        return Status::OK;
    }

    Status GetNodeView(ServerContext *cntxt, const NodeViewRequest *req, NodeView *resp) override {

        logf("<Process> Now adding new nodes!\n");

        int ID=0;

        for (auto &this_pn:partitions) {

            auto pt=resp->add_partition();

            ID++;

            pt->set_primary_idx(this_pn.primary_idx);

            logf("<Process> Added a new node view with ID: %d!\n",ID);

            for (auto &this_n: this_pn.nodes) {

                auto added_n=pt->add_node();
                fprintf(stdout, "Adding nodes!\n");
                added_n->set_alive(this_n.alive);
                added_n->set_address(this_n.addr);
            }
        }

        return Status::OK;
    }
};



void terminate_system() {
    this_srvr->Shutdown();
}

void healthcheck_each_second(KVMasterImpl*sv) {

    int time_interval;
    while (status_run==true) {
        sv->do_healthcheck();
        time_interval=1;
        sleep(time_interval);
    }
}